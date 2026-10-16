#include "WatchdogWeb.h"
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>

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
uint8_t        reiniciosSeguidos[MAX_DISPOSITIVOS];

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

// ✅ MUTEX — protege variables compartidas entre Core 0 (web) y Core 1 (ping)
SemaphoreHandle_t watchdogMutex = NULL;
// ✅ MUTEX del log — anadirLog() se llama desde los dos núcleos
SemaphoreHandle_t logMutex = NULL;

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
    reiniciosSeguidos[i]    = 0;
  }

  // Log vacío
  for (int i = 0; i < MAX_LOG; i++) {
    logEventos[i].valido = false;
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
// ENVIAR ESP-NOW EN BROADCAST — FF:FF:FF:FF:FF:FF
// Todos los enchufes escuchan, no hace falta la MAC de cada uno
// ============================================================
bool enviarESPNOW(int controlValue) {
  uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  registrarPeer(broadcast);
  struct_message msg;
  msg.controlValue = controlValue;
  return esp_now_send(broadcast, (uint8_t*)&msg, sizeof(msg)) == ESP_OK;
}

// ============================================================
// REVISAR TEMPORIZADORES — reencendido tras apagado y fin de gracia
// Se llama cada TICK_TAREA_MS desde la tarea de Core 1 (y también
// entre ping y ping), así T.Apagado y T.Gracia se cumplen con
// precisión aunque el intervalo de ping sea de 30 s o más.
// ============================================================
void revisarTemporizadores() {
  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    bool reencender = false, finGracia = false;
    Dispositivo d;

    if (xSemaphoreTake(watchdogMutex, pdMS_TO_TICKS(100)) != pdTRUE) continue;
    if (esperandoReencender[i] &&
        millis() - tiempoApagado[i] >= (unsigned long)dispositivos[i].tApagado * 1000UL) {
      esperandoReencender[i] = false;
      enGracia[i]            = true;
      tiempoReinicio[i]      = millis();
      fallosActuales[i]      = 0;
      reencender = true;
    } else if (enGracia[i] &&
        millis() - tiempoReinicio[i] >= (unsigned long)dispositivos[i].tGracia * 1000UL) {
      enGracia[i] = false;
      finGracia = true;
    }
    if (reencender || finGracia) d = dispositivos[i]; // copia para usar fuera del mutex
    xSemaphoreGive(watchdogMutex);

    if (reencender) {
      Serial.printf("🔛 Reencendiendo %s...\n", d.nombre.c_str());
      // ✅ REENCENDER: enviamos el estado NORMAL (alimentación ON)
      if (d.usarESPNOW) {
        int cv = (d.numBoton * 2) - (d.estadoESP ? 1 : 0);
        bool ok = enviarESPNOW(cv);
        Serial.printf("📡 ESP-NOW BROADCAST REENCENDER B%d→%s (controlValue=%d): %s\n",
          d.numBoton, d.estadoESP ? "ON" : "OFF", cv, ok ? "enviado" : "error");
      }
      // Reencender por RF
      if (d.usarRF && d.codigoRF != 0) {
        rf.send(d.codigoRF, 24);
      }
      anadirLog(d.nombre, "🔛 Reencendido tras " + String(d.tApagado) + "s");
    }

    if (finGracia) {
      Serial.printf("✅ %s — fin gracia, vigilancia reanudada\n", d.nombre.c_str());
      anadirLog(d.nombre, "✅ Fin de gracia — vigilancia reanudada");
    }
  }
}

// ============================================================
// TAREA CORE 1 — ciclo de ping + lógica de reencendido/gracia
// Corre en Core 1 para no bloquear el servidor web (Core 0)
// La reconexión WiFi la hace solo chequearCambioWiFi() en loop()
// ============================================================
void TareaPingCodigo(void* pvParameters) {
  esp_task_wdt_add(NULL); // vigilar también esta tarea
  bool primerCiclo = true;
  unsigned long ultimoCicloPing = 0;

  for (;;) {
    esp_task_wdt_reset();

    // Ciclo de ping cada cfgIntervalo segundos
    if (WiFi.status() == WL_CONNECTED &&
        (primerCiclo || millis() - ultimoCicloPing >= (unsigned long)cfgIntervalo * 1000UL)) {
      primerCiclo = false;
      ultimoCicloPing = millis();
      ejecutarCicloPing();
    }

    // Comprobar reencendidos y fin de gracia (cada medio segundo)
    revisarTemporizadores();

    vTaskDelay(pdMS_TO_TICKS(TICK_TAREA_MS));
  }
}

// ============================================================
// WATCHDOG HARDWARE — si el programa se cuelga, el ESP32 se reinicia
// ============================================================
void iniciarWatchdogHardware() {
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms     = WDT_TIMEOUT_S * 1000;
  cfg.idle_core_mask = 0;      // no vigilar las tareas idle
  cfg.trigger_panic  = true;   // reiniciar al saltar
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
  esp_task_wdt_add(NULL);      // vigilar loop()
  Serial.printf("✅ Watchdog hardware activo (%ds)\n", WDT_TIMEOUT_S);
}

// ============================================================
// OTA — actualizar el programa por WiFi desde el IDE de Arduino
// Herramientas → Puerto → "watchdog32 at 192.168.1.250"
// Contraseña: la misma que la de la web
// ============================================================
void iniciarOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(cfgWebPass.c_str());
  ArduinoOTA.onStart([]() {
    Serial.println("⬆️ OTA: actualización iniciada");
    if (TareaPing) vTaskSuspend(TareaPing);   // no actuar sobre enchufes mientras tanto
    esp_task_wdt_delete(NULL);                // la subida puede tardar
  });
  ArduinoOTA.onEnd([]()   { Serial.println("✅ OTA: terminada, reiniciando"); });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("❌ OTA error %u\n", e);
    if (TareaPing) vTaskResume(TareaPing);
    esp_task_wdt_add(NULL);
  });
  ArduinoOTA.begin();
  Serial.println("✅ OTA lista — host: " OTA_HOSTNAME);
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

  // 2. Crear mutex ANTES de nada (el log y las tareas los usan)
  watchdogMutex = xSemaphoreCreateMutex();
  logMutex      = xSemaphoreCreateMutex();
  if (watchdogMutex == NULL || logMutex == NULL) {
    Serial.println("❌ Error creando mutex — reiniciando");
    delay(1000); ESP.restart();
  }
  Serial.println("✅ Mutex creados");

  // 3. Watchdog hardware
  iniciarWatchdogHardware();

  // 4. Inicializar dispositivos con defaults
  inicializarDispositivosPorDefecto();

  // 5. Migrar NVS si viene de versión antigua (11 claves → 1 clave por dispositivo)
  //    Si ya está en formato nuevo no hace nada — seguro llamarlo siempre
  migrarNVSsiNecesario();

  // 6. Cargar NVS
  cargarTodoNVS();
  Serial.println("✅ NVS cargado");

  // 7. Conectar WiFi — multi-red
  conectarWiFiMultiRed();
  esp_task_wdt_reset();

  // 8. Hora real por internet (se sincroniza sola en segundo plano)
  configTzTime(NTP_TZ, NTP_SERVER);

  // 9. Iniciar ESP-NOW
  iniciarESPNOW();

  // 10. Iniciar RF 433MHz
  rf.enableTransmit(RF_TX_PIN);
  rf.setProtocol(1);
  rf.setRepeatTransmit(10);
  rf.setPulseLength(350);
  Serial.println("✅ RF 433MHz iniciado en GPIO" + String(RF_TX_PIN));

  // 11. Pre-registrar peers ESP-NOW
  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    if (dispositivos[i].activo && dispositivos[i].usarESPNOW) {
      bool ok = registrarPeer(dispositivos[i].mac);
      Serial.printf("📡 Peer %s: %s\n", dispositivos[i].nombre.c_str(), ok ? "OK" : "FALLO");
    }
  }

  // 12. Iniciar servidor web en Core 0 (loop) y OTA
  iniciarServidorWeb();
  Serial.println("✅ Servidor web en http://" + WiFi.localIP().toString());
  iniciarOTA();

  // 13. Lanzar tarea de ping en Core 1
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

  anadirLog("SISTEMA", "Watchdog arrancado — IP: " + WiFi.localIP().toString());
  digitalWrite(LED_PIN, LOW);
  Serial.println("--- SISTEMA LISTO ---");
}

// ============================================================
// LOOP — Core 0: SOLO servidor web, siempre responsivo
// ============================================================
void loop() {
  esp_task_wdt_reset();
  procesarWeb();
  ArduinoOTA.handle();
  chequearCambioWiFi(); // reconexión y auto-switch si señal mala
  delay(2);
}
