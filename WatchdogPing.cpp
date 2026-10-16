#include "WatchdogWeb.h"
#include <esp_task_wdt.h>

// ============================================================
// CONVERTIR MAC uint8_t[6] → String "AA:BB:CC:DD:EE:FF"
// ============================================================
String macToString(const uint8_t* mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// ============================================================
// CONVERTIR String "AA:BB:CC:DD:EE:FF" → uint8_t[6]
// ============================================================
bool stringToMac(const String& str, uint8_t* mac) {
  if (str.length() != 17) return false;
  for (int i = 0; i < 6; i++) {
    String byte_str = str.substring(i * 3, i * 3 + 2);
    mac[i] = (uint8_t)strtoul(byte_str.c_str(), nullptr, 16);
  }
  return true;
}

// ============================================================
// LIMPIAR TEXTO — quita '|' (separador de la NVS) y caracteres
// de control (saltos de línea, tabuladores...)
// ============================================================
String limpiarTexto(const String& s) {
  String r;
  r.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '|' || (uint8_t)c < 0x20) continue;
    r += c;
  }
  r.trim();
  return r;
}

// ============================================================
// ESCAPAR PARA HTML — evita que un nombre con comillas o < >
// rompa la página
// ============================================================
String htmlEscape(const String& s) {
  String r;
  r.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&':  r += "&amp;";  break;
      case '<':  r += "&lt;";   break;
      case '>':  r += "&gt;";   break;
      case '"':  r += "&quot;"; break;
      case '\'': r += "&#39;";  break;
      default:   r += c;
    }
  }
  return r;
}

// ============================================================
// ESCAPAR PARA JSON — comillas y barras invertidas
// ============================================================
String jsonEscape(const String& s) {
  String r;
  r.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { r += '\\'; r += c; }
    else if ((uint8_t)c >= 0x20) r += c;  // los de control se descartan
  }
  return r;
}

// ============================================================
// HORA DE UNA ENTRADA DEL LOG — fecha real si ya había NTP,
// si no, el tiempo desde el arranque
// ============================================================
String formatHoraLog(const LogEntry& e) {
  if (e.hora > 1600000000) {  // fecha válida (posterior a 2020)
    struct tm t;
    localtime_r(&e.hora, &t);
    char buf[20];
    strftime(buf, sizeof(buf), "%d/%m %H:%M:%S", &t);
    return String(buf);
  }
  return formatUptime(e.timestamp);
}

// ============================================================
// AÑADIR ENTRADA AL LOG CIRCULAR — protegido con logMutex
// porque se llama desde Core 0 (web) y Core 1 (ping)
// ============================================================
void anadirLog(const String& dispositivo, const String& evento) {
  time_t ahora = time(nullptr);
  if (logMutex) xSemaphoreTake(logMutex, portMAX_DELAY);
  logEventos[logIndex].dispositivo = dispositivo;
  logEventos[logIndex].evento      = evento;
  logEventos[logIndex].timestamp   = millis();
  logEventos[logIndex].hora        = ahora;
  logEventos[logIndex].valido      = true;
  logIndex = (logIndex + 1) % MAX_LOG; // circular
  if (logMutex) xSemaphoreGive(logMutex);
  Serial.printf("📋 LOG: [%s] %s\n", dispositivo.c_str(), evento.c_str());
}

// ============================================================
// PING HTTP — hace GET a http://IP/status, devuelve true si OK
// ============================================================
bool pingHTTP(const String& ip) {
  if (ip.length() == 0) return false;

  HTTPClient http;
  String url = "http://" + ip + "/status";
  http.begin(url);
  http.setConnectTimeout(3000); // 3 segundos máximo para conectar
  http.setTimeout(3000);        // 3 segundos máximo de espera de respuesta

  int httpCode = http.GET();
  http.end();

  // Cualquier respuesta HTTP (200, 401, 404...) significa que está vivo
  // Solo falla si no hay conexión TCP (httpCode < 0)
  bool vivo = (httpCode > 0);

  if (!vivo) {
    Serial.printf("❌ Sin respuesta de %s (código: %d)\n", ip.c_str(), httpCode);
  }
  return vivo;
}

// ============================================================
// ACTUAR SOBRE DISPOSITIVO CAÍDO — apagar (toggle OFF)
// ============================================================
void actuarSobreDispositivo(int idx, const String& motivo) {
  // Copia del dispositivo para no leer Strings mientras la web los cambia
  xSemaphoreTake(watchdogMutex, portMAX_DELAY);
  Dispositivo d = dispositivos[idx];
  xSemaphoreGive(watchdogMutex);

  Serial.printf("🚨 ACTUANDO sobre %s — %s\n", d.nombre.c_str(), motivo.c_str());
  anadirLog(d.nombre, "🚨 REINICIO: " + motivo);

  // ── APAGAR: enviamos estado CONTRARIO al normal ──────────────
  // estadoESP = estado con alimentación conectada (normal)
  // Para cortar alimentación enviamos !estadoESP
  // Ejemplo: relé normalmente ON → enviamos OFF para cortar
  bool estadoApagar = !d.estadoESP;

  if (d.usarESPNOW) {
    int cv = (d.numBoton * 2) - (estadoApagar ? 1 : 0);
    bool ok = enviarESPNOW(cv);
    Serial.printf("📡 ESP-NOW BROADCAST APAGAR B%d→%s (controlValue=%d): %s\n",
      d.numBoton, estadoApagar ? "ON" : "OFF", cv, ok ? "enviado" : "error");
  }

  if (d.usarRF && d.codigoRF != 0) {
    rf.send(d.codigoRF, 24);
    Serial.printf("📻 RF apagado enviado: %lu\n", (unsigned long)d.codigoRF);
  }

  // Esperamos lo que haga falta: si no se marcase, el enchufe se quedaría apagado
  xSemaphoreTake(watchdogMutex, portMAX_DELAY);
  esperandoReencender[idx] = true;
  tiempoApagado[idx]       = millis();
  if (reiniciosSeguidos[idx] < 255) reiniciosSeguidos[idx]++;
  xSemaphoreGive(watchdogMutex);
}

// ============================================================
// CICLO PRINCIPAL DE PING — se llama desde la tarea de Core 1
// cada cfgIntervalo segundos
// ============================================================
void ejecutarCicloPing() {
  Serial.println("🔍 === CICLO PING ===");

  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    // El ciclo puede durar bastante: mantener vivos watchdog y temporizadores
    esp_task_wdt_reset();
    revisarTemporizadores();

    // Copia de los datos que necesitamos, protegida con el mutex
    if (xSemaphoreTake(watchdogMutex, pdMS_TO_TICKS(100)) != pdTRUE) continue;
    Dispositivo   d          = dispositivos[i];
    bool          esperando  = esperandoReencender[i];
    bool          gracia     = enGracia[i];
    unsigned long tReinicio  = tiempoReinicio[i];
    uint8_t       reinicios  = reiniciosSeguidos[i];
    xSemaphoreGive(watchdogMutex);

    // Saltar si no está configurado o no está activo
    if (!d.activo || d.ip.length() == 0) continue;

    // Saltar si está esperando reencenderse
    if (esperando) {
      Serial.printf("⏳ %s — esperando reencender (%ds)\n", d.nombre.c_str(), d.tApagado);
      continue;
    }

    // Saltar si está en período de gracia
    if (gracia) {
      unsigned long pasado  = (millis() - tReinicio) / 1000;
      unsigned long restante = (pasado < d.tGracia) ? d.tGracia - pasado : 0;
      Serial.printf("🛡️ %s — en gracia (%lus restantes)\n", d.nombre.c_str(), restante);
      continue;
    }

    // Hacer ping HTTP
    bool vivo = pingHTTP(d.ip);

    if (vivo) {
      // Responde — resetear contadores
      xSemaphoreTake(watchdogMutex, portMAX_DELAY);
      uint8_t teniaFallos  = fallosActuales[i];
      fallosActuales[i]    = 0;
      reiniciosSeguidos[i] = 0;
      tiempoUltimoOK[i]    = millis();
      xSemaphoreGive(watchdogMutex);

      if (teniaFallos > 0) {
        Serial.printf("✅ %s — recuperado (tenía %d fallos)\n", d.nombre.c_str(), teniaFallos);
        anadirLog(d.nombre, "✅ Recuperado tras " + String(teniaFallos) + " fallos");
      }
      Serial.printf("✅ %s — OK\n", d.nombre.c_str());

    } else {
      // No responde — incrementar contador
      xSemaphoreTake(watchdogMutex, portMAX_DELAY);
      if (fallosActuales[i] < 255) fallosActuales[i]++;
      uint8_t fallos = fallosActuales[i];
      xSemaphoreGive(watchdogMutex);

      Serial.printf("⚠️ %s — fallo %d/%d\n", d.nombre.c_str(), fallos, d.maxFallos);

      // Solo se apunta en el log el primer fallo, para no llenarlo
      if (fallos == 1) {
        anadirLog(d.nombre, "⚠️ Sin respuesta (1/" + String(d.maxFallos) + ")");
      }

      // ¿Hemos llegado al límite de fallos?
      if (fallos >= d.maxFallos) {
        bool enPausa = reinicios >= MAX_REINICIOS_SEGUIDOS &&
                       millis() - tReinicio < PAUSA_TRAS_REINICIOS_MS;
        if (enPausa) {
          // Ya lo hemos reiniciado varias veces y no se recupera:
          // esperamos antes de volver a cortarle la corriente
          if (fallos == d.maxFallos) {
            anadirLog(d.nombre, "⏸️ Sigue caído tras " + String(reinicios) +
              " reinicios — pausa de " + String(PAUSA_TRAS_REINICIOS_MS / 60000UL) + " min");
          }
        } else {
          actuarSobreDispositivo(i, String(d.maxFallos) + " fallos consecutivos");
        }
      }
    }

    // Pequeña pausa entre pings para no saturar la red
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  Serial.println("=== FIN CICLO PING ===");
}
