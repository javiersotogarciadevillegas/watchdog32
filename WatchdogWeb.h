#ifndef WATCHDOG_WEB_H
#define WATCHDOG_WEB_H

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <HTTPClient.h>
#include <RCSwitch.h>
#include <time.h>

// ============================================================
// CONFIGURACIÓN HARDWARE
// ============================================================
#define RF_TX_PIN       4       // GPIO4 — FS1000A transmisor RF 433MHz
#define LED_PIN         2       // GPIO2 — LED interno ESP32
#define MAX_DISPOSITIVOS 16     // Máximo de dispositivos vigilados
#define MAX_LOG         30      // Máximo de entradas en el log

// ============================================================
// CLAVES PRIVADAS — van en secrets.h, que NO se sube a GitHub
// Copia secrets.h.example como secrets.h y pon tus claves.
// Si no existe secrets.h se usan estos valores vacíos/genéricos.
// ============================================================
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef DEF_SSID
#define DEF_SSID        "esp32"
#endif
#ifndef DEF_PASS
#define DEF_PASS        ""
#endif
#ifndef DEF_WEBPASS
#define DEF_WEBPASS     "1234"
#endif

// ============================================================
// CONFIGURACIÓN POR DEFECTO (se sobreescriben con NVS si existen)
// ============================================================
#define DEF_IP          "192.168.1.250"   // IP fija del watchdog
#define DEF_GW          "192.168.1.1"
#define DEF_SN          "255.255.255.0"
#define DEF_DNS         "8.8.8.8"
#define DEF_INTERVALO   30      // segundos entre pings globales
#define DEF_MAX_FALLOS  3       // fallos consecutivos antes de actuar
#define DEF_T_APAGADO   5       // segundos con relé apagado
#define DEF_T_GRACIA    20      // segundos de gracia tras reinicio
#define NVS_NAMESPACE   "watchdog"

// Límites de los campos (los mismos que en el formulario web)
#define MAX_BOTON       18
#define MAX_FALLOS_LIM  10
#define MAX_T_APAGADO   60
#define MIN_T_GRACIA    10
#define MAX_T_GRACIA    300

// Si un dispositivo sigue caído tras varios reinicios seguidos,
// se espera un rato antes de volver a cortarle la corriente
#define MAX_REINICIOS_SEGUIDOS  3
#define PAUSA_TRAS_REINICIOS_MS (30UL * 60UL * 1000UL)  // 30 minutos

// Cada cuánto revisa la tarea de Core 1 los temporizadores (apagado/gracia)
#define TICK_TAREA_MS   500

// Watchdog hardware: si el programa se cuelga más de esto, el ESP32 se reinicia
#define WDT_TIMEOUT_S   120

// Hora real por internet (NTP) — zona horaria de España peninsular
#define NTP_SERVER      "pool.ntp.org"
#define NTP_TZ          "CET-1CEST,M3.5.0,M10.5.0/3"

// Nombre del equipo para actualizar por WiFi (OTA) desde el IDE de Arduino
#define OTA_HOSTNAME    "watchdog32"

// Tamaño máximo del fichero JSON que se puede importar
#define MAX_IMPORT_BYTES 16384

// ============================================================
// CONFIGURACIÓN MULTI-RED WIFI
// ============================================================
#define WIFI_MAX_SLOTS        5       // máximo de redes guardadas
#define WIFI_CHECK_INTERVAL   30000   // ms entre chequeos de señal
#define WIFI_SWITCH_THRESHOLD -75     // dBm: si baja de aquí busca mejor red
#define WIFI_MIN_IMPROVEMENT  10      // dBm: mejora mínima para cambiar

// MODO RESCATE: si no conecta a ninguna red, el Watchdog crea su propia WiFi
// Conéctate a ella con el móvil y entra en http://192.168.4.1 para arreglar la config
#ifndef AP_RESCATE_SSID
#define AP_RESCATE_SSID       "Watchdog32-Config"
#endif
#ifndef AP_RESCATE_PASS
#define AP_RESCATE_PASS       "watchdog32"   // mínimo 8 caracteres
#endif
#define WIFI_RETRY_RESCATE_MS 180000  // en modo rescate, reintentar cada 3 min
extern bool apRescateActivo;

// Variables multi-red (definidas en watchdog32.ino)
extern String  wifiSSIDs[WIFI_MAX_SLOTS];
extern String  wifiPasses[WIFI_MAX_SLOTS];
extern int     wifiPref;              // índice de red preferida

// ============================================================
// ESTRUCTURA DE DISPOSITIVO
// ============================================================
struct Dispositivo {
  String   nombre;          // Nombre descriptivo
  String   ip;              // IP a vigilar (HTTP /status)
  uint8_t  mac[6];          // MAC ESP-NOW destino
  uint8_t  numBoton;        // Número de botón del tablero (1-18)
  bool     estadoESP;       // Estado NORMAL del relé (alimentación conectada)
  // Para APAGAR: se envía !estadoESP — Para ENCENDER: se envía estadoESP
  // Ejemplo: relé normalmente ON para alimentar → estadoESP=true
  //          Watchdog apaga enviando OFF, luego enciende enviando ON
  uint32_t codigoRF;        // Código RF 433MHz (24 bits, máx 16777215)
  bool     usarESPNOW;      // Enviar por ESP-NOW al actuar
  bool     usarRF;          // Enviar por RF al actuar
  bool     activo;          // Vigilancia activa para este dispositivo
  uint8_t  maxFallos;       // Fallos antes de actuar
  uint8_t  tApagado;        // Segundos apagado antes de reencender
  uint16_t tGracia;         // Segundos de gracia tras reinicio (10-300, por eso 16 bits)
};

// ============================================================
// ESTRUCTURA LOG DE EVENTOS
// ============================================================
struct LogEntry {
  String         dispositivo;   // Nombre del dispositivo
  String         evento;        // Descripción del evento
  unsigned long  timestamp;     // millis() cuando ocurrió
  time_t         hora;          // hora real (NTP) — 0 si aún no había hora
  bool           valido;        // Entrada válida
};

// ============================================================
// ESTRUCTURA ESP-NOW — debe coincidir con el receptor (tablero)
// El tablero espera struct { int controlValue; } = 4 bytes
// ============================================================
typedef struct {
  int controlValue;
} struct_message;

// ============================================================
// MUTEX — protege variables compartidas entre Core 0 y Core 1
// ============================================================
extern SemaphoreHandle_t watchdogMutex;
// Mutex aparte para el log — se usa desde los dos núcleos
extern SemaphoreHandle_t logMutex;

// ============================================================
// VARIABLES GLOBALES — definidas en .ino, extern aquí
// ============================================================
extern Dispositivo     dispositivos[MAX_DISPOSITIVOS];
extern LogEntry        logEventos[MAX_LOG];
extern int             logIndex;           // índice circular del log
extern WebServer       server;
extern Preferences     prefs;
extern RCSwitch        rf;

// Estado en RAM (no se guarda en NVS — se recalcula en cada arranque)
extern uint8_t         fallosActuales[MAX_DISPOSITIVOS];
extern unsigned long   tiempoUltimoOK[MAX_DISPOSITIVOS];
extern unsigned long   tiempoReinicio[MAX_DISPOSITIVOS];
extern bool            enGracia[MAX_DISPOSITIVOS];
extern bool            esperandoReencender[MAX_DISPOSITIVOS];
extern unsigned long   tiempoApagado[MAX_DISPOSITIVOS];
extern uint8_t         reiniciosSeguidos[MAX_DISPOSITIVOS]; // reinicios sin recuperarse

// Configuración global
extern String          cfgSSID;
extern String          cfgPass;
extern String          cfgWebPass;
extern String          cfgIP;
extern String          cfgGW;
extern String          cfgSN;
extern String          cfgDNS;
extern int             cfgIntervalo;

// ============================================================
// PROTOTIPOS — watchdog32.ino (funciones definidas en el .ino)
// ============================================================
bool registrarPeer(const uint8_t* mac);  // registrar peer ESP-NOW dinámico
bool enviarESPNOW(int controlValue);     // enviar por broadcast a todos los enchufes
void revisarTemporizadores();            // reencendido y fin de gracia (Core 1)
void iniciarServidorWeb();
void procesarWeb();
void handleRaiz();
void handleGuardarDispositivos();
void handleGuardarConfig();
void handleLog();
void handleStatus();
void handleBorrarLog();
void handleConfiguracion();
// Test manual desde la web
void handleTestRF();       // POST /testRF    — enviar código RF manual
void handleTestESP();      // POST /testESP   — enviar código ESP-NOW manual
void handleEstado();       // GET  /estado    — JSON para auto-refresco

// Backup / Restauración NVS
void handleExportNVS();    // GET  /exportNVS — descarga JSON con toda la config
void handleImportNVS();    // POST /importNVS — recibe el fichero JSON (por trozos)
void handleImportNVSFin(); // POST /importNVS — al terminar: aplica la config y reinicia

// WiFi multi-red
void handleConfigWiFi();      // GET  /configWiFi — página gestión multi-red
void handleGuardarWiFi();     // POST /guardarWiFi — guarda red en slot
void handleBorrarWiFi();      // GET  /borrarWiFi  — borra slot
void handleScanWiFi();        // GET  /scanWiFi    — escaneo AJAX
void handleSetPreferidaWiFi();// POST /setPreferida — marca red preferida
void conectarWiFiMultiRed();  // Conectar al arranque — intenta todas las redes
void chequearCambioWiFi();    // Llamar en loop() — auto-switch si señal mala

// Ayuda
void handleAyuda();        // GET  /ayuda — página de documentación

// ============================================================
// PROTOTIPOS — WatchdogNVS.cpp
// ============================================================
void cargarTodoNVS();
void guardarDispositivoNVS(int idx);
void guardarRedWiFiNVS(int slot);
void guardarWiFiPrefNVS();
void guardarConfigNVS();
void cargarConfigNVS();
void migrarNVSsiNecesario(); // ✅ migración formato antiguo → nuevo

// ============================================================
// PROTOTIPOS — WatchdogPing.cpp
// ============================================================
void ejecutarCicloPing();
bool pingHTTP(const String& ip);
void actuarSobreDispositivo(int idx, const String& motivo);
void anadirLog(const String& dispositivo, const String& evento);
String macToString(const uint8_t* mac);
bool stringToMac(const String& str, uint8_t* mac);
String limpiarTexto(const String& s);    // quita caracteres que rompen NVS/JSON
String htmlEscape(const String& s);      // para meter texto dentro del HTML
String jsonEscape(const String& s);      // para meter texto dentro de un JSON
String formatUptime(unsigned long ms);
String formatHoraLog(const LogEntry& e); // fecha real si hay NTP, si no uptime

#endif
