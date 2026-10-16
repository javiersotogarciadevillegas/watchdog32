#include "WatchdogWeb.h"

// ============================================================
// VARIABLES GLOBALES — definidas aquí, extern en .h
// ============================================================
Dispositivo    dispositivos[MAX_DISPOSITIVOS];
LogEntry       logEventos[MAX_LOG];
int            logIndex = 0;
WebServer      server(80);
Preferences    prefs;
RCSwitch       rf;

// Estado en RAM
uint8_t        fallosActuales[MAX_DISPOSITIVOS];
unsigned long  tiempoUltimoOK[MAX_DISPOSITIVOS];
unsigned long  tiempoReinicio[MAX_DISPOSITIVOS];
bool           enGracia[MAX_DISPOSITIVOS];
bool           esperandoReencender[MAX_DISPOSITIVOS];
unsigned long  tiempoApagado[MAX_DISPOSITIVOS];

// Configuración global
String         cfgSSID      = DEF_SSID;
String         cfgPass      = DEF_PASS;
String         cfgWebPass   = DEF_WEBPASS;
String         cfgIP        = DEF_IP;
String         cfgGW        = DEF_GW;
String         cfgSN        = DEF_SN;
String         cfgDNS       = DEF_DNS;
int            cfgIntervalo = DEF_INTERVALO;

// ✅ Variables multi-red WiFi
String         wifiSSIDs[WIFI_MAX_SLOTS];
String         wifiPasses[WIFI_MAX_SLOTS];
int            wifiPref = 0;

// Control de tiempo del ciclo de ping
unsigned long  ultimoCicloPing = 0;

// ✅ MUTEX — protege variables compartidas entre Core 0 (web) y Core 1 (ping)
SemaphoreHandle_t watchdogMutex = NULL;

// Handle de la tarea de ping en Core 1
TaskHandle_t TareaPing = NULL;

// ============================================================
// CALLBACK ESP-NOW — confirmación de envío
// Compatible con ESP32 core 3.3.6 (firma actualizada)
// ============================================================
void OnDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  Serial.printf("📡 ESP-NOW enviado — %s\n",
    status == ESP_NOW_SEND_SUCCESS ? "✅ OK" : "❌ FALLO");
}

// ============================================================
// INICIALIZAR DISPOSITIVOS CON VALORES POR DEFECTO
// ============================================================
void inicializarDispositivosPorDefecto() {
  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    dispositivos[i].nombre     = "";
    dispositivos[i].ip         = "";
    for (int j = 0; j < 6; j++) dispositivos[i].mac[j] = 0;
    dispositivos[i].numBoton   = 1;     // botón 1 por defecto
    dispositivos[i].estadoESP  = false; // OFF por defecto
    dispositivos[i].codigoRF   = 0;
    dispositivos[i].usarESPNOW = false;
    dispositivos[i].usarRF     = false;
    dispositivos[i].activo     = false;
    dispositivos[i].maxFallos  = DEF_MAX_FALLOS;
    dispositivos[i].tApagado   = DEF_T_APAGADO;
    dispositivos[i].tGracia    = DEF_T_GRACIA;

    // Estado RAM
    fallosActuales[i]       = 0;
    tiempoUltimoOK[i]       = 0;
    tiempoReinicio[i]       = 0;
    enGracia[i]             = false;
    esperandoReencender[i]  = false;
    tiempoApagado[i]        = 0;
  }

  // Log vacío
  for (int i = 0; i < MAX_LOG; i++) {
    logEventos[i].valido = false;
  }
}

// ============================================================
// CONECTAR WIFI CON IP FIJA
// ============================================================
void conectarWiFi() {
  WiFi.mode(WIFI_STA);

  // Aplicar IP estática
  IPAddress ip, gw, sn, dns;
  if (ip.fromString(cfgIP) && gw.fromString(cfgGW) &&
      sn.fromString(cfgSN) && dns.fromString(cfgDNS)) {
    WiFi.config(ip, gw, sn, dns);
    Serial.println("📍 IP fija configurada: " + cfgIP);
  } else {
    Serial.println("⚠️ IP inválida en NVS, usando por defecto: " + String(DEF_IP));
    IPAddress dIP(192,168,1,250), dGW(192,168,1,1);
    IPAddress dSN(255,255,255,0), dDNS(8,8,8,8);
    WiFi.config(dIP, dGW, dSN, dDNS);
  }

  WiFi.begin(cfgSSID.c_str(), cfgPass.c_str());
  Serial.print("📶 Conectando a WiFi");

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 40) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi conectado. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n❌ No se pudo conectar al WiFi");
  }
}

// ============================================================
// INICIALIZAR ESP-NOW
// ============================================================
void iniciarESPNOW() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error iniciando ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  Serial.println("✅ ESP-NOW iniciado");
}

// ============================================================
// REGISTRAR PEER ESP-NOW DINÁMICO
// ============================================================
bool registrarPeer(const uint8_t* mac) {
  // Si ya existe como peer, no lo añadimos de nuevo
  if (esp_now_is_peer_exist(mac)) return true;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;    // canal actual
  peer.encrypt = false;
  return (esp_now_add_peer(&peer) == ESP_OK);
}

// ============================================================
// TAREA CORE 1 — ciclo de ping + lógica de reencendido/gracia
// Corre en Core 1 para no bloquear el servidor web (Core 0)
// ============================================================
void TareaPingCodigo(void* pvParameters) {
  for (;;) {
    // Ciclo de ping cada cfgIntervalo segundos
    if (WiFi.status() == WL_CONNECTED) {
      ejecutarCicloPing();
    }

    // Verificar dispositivos esperando reencenderse o en gracia
    for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
      if (xSemaphoreTake(watchdogMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool estaEsperando = esperandoReencender[i];
        bool estaEnGracia  = enGracia[i];
        xSemaphoreGive(watchdogMutex);

        if (estaEsperando) {
          if (millis() - tiempoApagado[i] >= (unsigned long)dispositivos[i].tApagado * 1000UL) {
            if (xSemaphoreTake(watchdogMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
              esperandoReencender[i] = false;
              enGracia[i]            = true;
              tiempoReinicio[i]      = millis();
              fallosActuales[i]      = 0;
              xSemaphoreGive(watchdogMutex);
            }
            Serial.printf("🔛 Reencendiendo %s...\n", dispositivos[i].nombre.c_str());
            // ✅ REENCENDER: broadcast FF:FF:FF:FF:FF:FF — todos los enchufes escuchan
            if (dispositivos[i].usarESPNOW) {
              uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
              registrarPeer(broadcast);
              struct_message msg;
              uint8_t boton = dispositivos[i].numBoton;
              bool    on    = dispositivos[i].estadoESP; // estado normal = alimentación ON
              msg.controlValue = (boton * 2) - (on ? 1 : 0);
              esp_now_send(broadcast, (uint8_t*)&msg, sizeof(msg));
              Serial.printf("📡 ESP-NOW BROADCAST REENCENDER B%d→%s (controlValue=%d)\n",
                boton, on ? "ON" : "OFF", msg.controlValue);
            }
            // Reencender por RF
            if (dispositivos[i].usarRF && dispositivos[i].codigoRF != 0) {
              rf.send(dispositivos[i].codigoRF, 24);
            }
            añadirLog(dispositivos[i].nombre, "🔛 Reencendido tras " +
              String(dispositivos[i].tApagado) + "s");
          }
        }

        if (estaEnGracia) {
          if (millis() - tiempoReinicio[i] >= (unsigned long)dispositivos[i].tGracia * 1000UL) {
            if (xSemaphoreTake(watchdogMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
              enGracia[i] = false;
              xSemaphoreGive(watchdogMutex);
            }
            Serial.printf("✅ %s — fin gracia, vigilancia reanudada\n", dispositivos[i].nombre.c_str());
            añadirLog(dispositivos[i].nombre, "✅ Fin de gracia — vigilancia reanudada");
          }
        }
      }
    }

    // Reconexión WiFi si se cae
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("⚠️ WiFi caído, reconectando...");
      WiFi.disconnect(false);
      delay(1000);
      conectarWiFi();
    }

    // Esperar el intervalo antes del siguiente ciclo
    vTaskDelay(pdMS_TO_TICKS((unsigned long)cfgIntervalo * 1000UL));
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n🐕 WATCHDOG ESP32 — Arrancando...");

  // 1. LED de estado
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // 2. Inicializar dispositivos con defaults
  inicializarDispositivosPorDefecto();

  // 3. Migrar NVS si viene de versión antigua (11 claves → 1 clave por dispositivo)
  //    Si ya está en formato nuevo no hace nada — seguro llamarlo siempre
  migrarNVSsiNecesario();

  // 4. Cargar NVS
  cargarTodoNVS();
  Serial.println("✅ NVS cargado");

  // 4. Conectar WiFi — multi-red
  conectarWiFiMultiRed();

  // 5. Iniciar ESP-NOW
  iniciarESPNOW();

  // 6. Iniciar RF 433MHz
  rf.enableTransmit(RF_TX_PIN);
  rf.setProtocol(1);
  rf.setRepeatTransmit(10);
  rf.setPulseLength(350);
  Serial.println("✅ RF 433MHz iniciado en GPIO" + String(RF_TX_PIN));

  // 7. Pre-registrar peers ESP-NOW
  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    if (dispositivos[i].activo && dispositivos[i].usarESPNOW) {
      bool ok = registrarPeer(dispositivos[i].mac);
      Serial.printf("📡 Peer %s: %s\n", dispositivos[i].nombre.c_str(), ok ? "OK" : "FALLO");
    }
  }

  // 8. Crear mutex ANTES de lanzar tareas
  watchdogMutex = xSemaphoreCreateMutex();
  if (watchdogMutex == NULL) {
    Serial.println("❌ Error creando mutex — reiniciando");
    delay(1000); ESP.restart();
  }
  Serial.println("✅ Mutex creado");

  // 9. Iniciar servidor web en Core 0 (loop)
  iniciarServidorWeb();
  Serial.println("✅ Servidor web en http://" + WiFi.localIP().toString());

  // 10. Lanzar tarea de ping en Core 1
  xTaskCreatePinnedToCore(
    TareaPingCodigo,  // función
    "TareaPing",      // nombre
    8192,             // stack (8KB suficiente para HTTP)
    NULL,             // parámetros
    1,                // prioridad
    &TareaPing,       // handle
    1                 // Core 1
  );
  Serial.println("✅ Tarea ping lanzada en Core 1");

  añadirLog("SISTEMA", "Watchdog arrancado — IP: " + WiFi.localIP().toString());
  digitalWrite(LED_PIN, LOW);
  Serial.println("--- SISTEMA LISTO ---");
}

// ============================================================
// LOOP — Core 0: SOLO servidor web, siempre responsivo
// ============================================================
void loop() {
  procesarWeb();
  chequearCambioWiFi(); // auto-switch si señal mala, llamar desde Core 0
  delay(2);
}
