#include "WatchdogWeb.h"

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
// AÑADIR ENTRADA AL LOG CIRCULAR
// ============================================================
void añadirLog(const String& dispositivo, const String& evento) {
  logEventos[logIndex].dispositivo = dispositivo;
  logEventos[logIndex].evento      = evento;
  logEventos[logIndex].timestamp   = millis();
  logEventos[logIndex].valido      = true;
  logIndex = (logIndex + 1) % MAX_LOG; // circular
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
  http.setTimeout(3000); // 3 segundos máximo de espera

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
  Serial.printf("🚨 ACTUANDO sobre %s — %s\n",
    dispositivos[idx].nombre.c_str(), motivo.c_str());

  añadirLog(dispositivos[idx].nombre,
    "🚨 REINICIO: " + motivo + " (" + String(dispositivos[idx].maxFallos) + " fallos)");

  uint8_t boton = dispositivos[idx].numBoton;

  // ── APAGAR: enviamos estado CONTRARIO al normal ──────────────
  // estadoESP = estado con alimentación conectada (normal)
  // Para cortar alimentación enviamos !estadoESP
  // Ejemplo: relé normalmente ON → enviamos OFF para cortar
  bool estadoApagar = !dispositivos[idx].estadoESP;

  if (dispositivos[idx].usarESPNOW) {
    // ✅ BROADCAST: FF:FF:FF:FF:FF:FF — todos los enchufes escuchan
    // No necesitamos MAC del enchufe — igual que hace el tablero
    uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    registrarPeer(broadcast);
    struct_message msg;
    msg.controlValue = (boton * 2) - (estadoApagar ? 1 : 0);
    esp_err_t res = esp_now_send(broadcast, (uint8_t*)&msg, sizeof(msg));
    Serial.printf("📡 ESP-NOW BROADCAST APAGAR B%d→%s (controlValue=%d): %s\n",
      boton, estadoApagar ? "ON" : "OFF", msg.controlValue,
      res == ESP_OK ? "enviado" : "error");
  }

  if (dispositivos[idx].usarRF && dispositivos[idx].codigoRF != 0) {
    rf.send(dispositivos[idx].codigoRF, 24);
    Serial.printf("📻 RF apagado enviado: %lu\n", dispositivos[idx].codigoRF);
  }

  if (xSemaphoreTake(watchdogMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    esperandoReencender[idx] = true;
    tiempoApagado[idx]       = millis();
    xSemaphoreGive(watchdogMutex);
  }
}

// ============================================================
// CICLO PRINCIPAL DE PING — se llama desde loop() cada N segundos
// ============================================================
void ejecutarCicloPing() {
  Serial.println("🔍 === CICLO PING ===");

  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {

    // Saltar si no está configurado o no está activo
    if (!dispositivos[i].activo || dispositivos[i].ip.length() == 0) continue;

    // Saltar si está esperando reencenderse
    if (esperandoReencender[i]) {
      Serial.printf("⏳ %s — esperando reencender (%ds)\n",
        dispositivos[i].nombre.c_str(), dispositivos[i].tApagado);
      continue;
    }

    // Saltar si está en período de gracia
    if (enGracia[i]) {
      unsigned long restante = dispositivos[i].tGracia -
        ((millis() - tiempoReinicio[i]) / 1000);
      Serial.printf("🛡️ %s — en gracia (%lus restantes)\n",
        dispositivos[i].nombre.c_str(), restante);
      continue;
    }

    // Hacer ping HTTP
    bool vivo = pingHTTP(dispositivos[i].ip);

    if (vivo) {
      // Responde — resetear contador de fallos
      if (fallosActuales[i] > 0) {
        Serial.printf("✅ %s — recuperado (tenía %d fallos)\n",
          dispositivos[i].nombre.c_str(), fallosActuales[i]);
        añadirLog(dispositivos[i].nombre, "✅ Recuperado tras " +
          String(fallosActuales[i]) + " fallos");
      }
      fallosActuales[i]  = 0;
      tiempoUltimoOK[i]  = millis();
      Serial.printf("✅ %s — OK\n", dispositivos[i].nombre.c_str());

    } else {
      // No responde — incrementar contador
      fallosActuales[i]++;
      Serial.printf("⚠️ %s — fallo %d/%d\n",
        dispositivos[i].nombre.c_str(),
        fallosActuales[i],
        dispositivos[i].maxFallos);

      añadirLog(dispositivos[i].nombre,
        "⚠️ Fallo " + String(fallosActuales[i]) +
        "/" + String(dispositivos[i].maxFallos));

      // ¿Hemos llegado al límite de fallos?
      if (fallosActuales[i] >= dispositivos[i].maxFallos) {
        actuarSobreDispositivo(i, String(dispositivos[i].maxFallos) + " fallos consecutivos");
      }
    }

    // Pequeña pausa entre pings para no saturar la red
    delay(500);
  }

  Serial.println("=== FIN CICLO PING ===");
}
