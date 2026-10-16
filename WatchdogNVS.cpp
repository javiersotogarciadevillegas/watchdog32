#include "WatchdogWeb.h"

// ============================================================
// CARGAR CONFIGURACIÓN GLOBAL DESDE NVS
// ============================================================
void cargarConfigNVS() {
  prefs.begin(NVS_NAMESPACE, true); // solo lectura
  cfgSSID      = prefs.getString("ssid",      DEF_SSID);
  cfgPass      = prefs.getString("pass",      DEF_PASS);
  cfgWebPass   = prefs.getString("webpass",   DEF_WEBPASS);
  cfgIP        = prefs.getString("ip",        DEF_IP);
  cfgGW        = prefs.getString("gw",        DEF_GW);
  cfgSN        = prefs.getString("sn",        DEF_SN);
  cfgDNS       = prefs.getString("dns",       DEF_DNS);
  cfgIntervalo = constrain(prefs.getInt("intervalo", DEF_INTERVALO), 5, 300);
  prefs.end();

  Serial.println("✅ Config global NVS: SSID=" + cfgSSID + " IP=" + cfgIP);
}

// ============================================================
// GUARDAR CONFIGURACIÓN GLOBAL EN NVS
// ============================================================
void guardarConfigNVS() {
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    Serial.println("❌ NVS: no se pudo abrir namespace para escritura");
    return;
  }
  bool ok = true;
  ok &= prefs.putString("ssid",      cfgSSID)      > 0;
  ok &= prefs.putString("pass",      cfgPass)       > 0;
  ok &= prefs.putString("webpass",   cfgWebPass)    > 0;
  ok &= prefs.putString("ip",        cfgIP)         > 0;
  ok &= prefs.putString("gw",        cfgGW)         > 0;
  ok &= prefs.putString("sn",        cfgSN)         > 0;
  ok &= prefs.putString("dns",       cfgDNS)        > 0;
  ok &= prefs.putInt("intervalo",    cfgIntervalo)  > 0;
  prefs.end();

  if (ok) Serial.println("✅ Config global guardada en NVS");
  else    Serial.println("⚠️ NVS: alguna clave no se pudo guardar (NVS lleno?)");
}

// ============================================================
// GUARDAR UN DISPOSITIVO — UNA SOLA CLAVE POR DISPOSITIVO
// Formato: "nombre|ip|mac|numBoton|estadoESP|codigoRF|usarESP|usarRF|activo|maxFallos|tApagado|tGracia"
// Esto reduce de 11 claves a 1 — de 176 entradas a 16 en total
// ============================================================
void guardarDispositivoNVS(int idx) {
  if (idx < 0 || idx >= MAX_DISPOSITIVOS) return;

  // Serializar todo el dispositivo en un string delimitado por |
  String data = dispositivos[idx].nombre                              + "|";
  data +=       dispositivos[idx].ip                                  + "|";
  data +=       macToString(dispositivos[idx].mac)                    + "|";
  data +=       String(dispositivos[idx].numBoton)                    + "|";
  data +=       String(dispositivos[idx].estadoESP  ? "1" : "0")     + "|";
  data +=       String(dispositivos[idx].codigoRF)                   + "|";
  data +=       String(dispositivos[idx].usarESPNOW ? "1" : "0")     + "|";
  data +=       String(dispositivos[idx].usarRF     ? "1" : "0")     + "|";
  data +=       String(dispositivos[idx].activo     ? "1" : "0")     + "|";
  data +=       String(dispositivos[idx].maxFallos)                  + "|";
  data +=       String(dispositivos[idx].tApagado)                   + "|";
  data +=       String(dispositivos[idx].tGracia);

  String key = "dev" + String(idx); // clave: dev0, dev1 ... dev15

  if (!prefs.begin(NVS_NAMESPACE, false)) {
    Serial.printf("❌ NVS: no se pudo abrir para dispositivo %d\n", idx);
    return;
  }
  bool ok = prefs.putString(key.c_str(), data) > 0;
  prefs.end();

  if (ok) Serial.printf("✅ Dispositivo %d guardado en NVS (%d bytes)\n", idx, data.length());
  else    Serial.printf("⚠️ NVS: dispositivo %d no se pudo guardar\n", idx);
}

// ============================================================
// GUARDAR UNA RED WIFI (slot 0-4) Y LA RED PREFERIDA
// ============================================================
void guardarRedWiFiNVS(int slot) {
  if (slot < 0 || slot >= WIFI_MAX_SLOTS) return;
  if (!prefs.begin(NVS_NAMESPACE, false)) return;
  prefs.putString(("wssid" + String(slot)).c_str(), wifiSSIDs[slot]);
  prefs.putString(("wpass" + String(slot)).c_str(), wifiPasses[slot]);
  prefs.end();
}

void guardarWiFiPrefNVS() {
  if (!prefs.begin(NVS_NAMESPACE, false)) return;
  prefs.putInt("wifiPref", wifiPref);
  prefs.end();
}

// ============================================================
// CARGAR UN DISPOSITIVO INDIVIDUAL DESDE NVS
// ============================================================
void cargarDispositivoNVS(int idx) {
  if (idx < 0 || idx >= MAX_DISPOSITIVOS) return;

  String key = "dev" + String(idx);

  prefs.begin(NVS_NAMESPACE, true);
  String data = prefs.getString(key.c_str(), "");
  prefs.end();

  if (data.length() == 0) return; // sin datos guardados — mantener defaults

  // Deserializar — separar por |
  String campos[12];
  int campo = 0;
  int start = 0;
  for (int i = 0; i <= (int)data.length() && campo < 12; i++) {
    if (i == (int)data.length() || data[i] == '|') {
      campos[campo++] = data.substring(start, i);
      start = i + 1;
    }
  }

  if (campo < 12) {
    Serial.printf("⚠️ Dispositivo %d: datos incompletos en NVS (%d campos)\n", idx, campo);
    return;
  }

  dispositivos[idx].nombre    = campos[0];
  dispositivos[idx].ip        = campos[1];
  if (campos[2].length() > 0) stringToMac(campos[2], dispositivos[idx].mac);
  dispositivos[idx].numBoton  = (uint8_t)constrain((int)campos[3].toInt(), 1, MAX_BOTON);
  dispositivos[idx].estadoESP = (campos[4] == "1");
  dispositivos[idx].codigoRF  = (uint32_t)campos[5].toInt();
  dispositivos[idx].usarESPNOW= (campos[6] == "1");
  dispositivos[idx].usarRF    = (campos[7] == "1");
  dispositivos[idx].activo    = (campos[8] == "1");
  dispositivos[idx].maxFallos = (uint8_t) constrain((int)campos[9].toInt(),  1, MAX_FALLOS_LIM);
  dispositivos[idx].tApagado  = (uint8_t) constrain((int)campos[10].toInt(), 1, MAX_T_APAGADO);
  dispositivos[idx].tGracia   = (uint16_t)constrain((int)campos[11].toInt(), MIN_T_GRACIA, MAX_T_GRACIA);
}

// ============================================================
// MIGRACIÓN NVS — formato antiguo (11 claves) → nuevo (1 clave)
// Se llama al arrancar ANTES de cargarTodoNVS()
// Si encuentra claves antiguas las migra y las borra
// Si no encuentra claves antiguas no hace nada
// ============================================================
void migrarNVSsiNecesario() {
  prefs.begin(NVS_NAMESPACE, true); // solo lectura
  // Comprobamos si existe el formato antiguo buscando la clave "d0_nom"
  bool formatoAntiguo = prefs.isKey("d0_nom");
  prefs.end();

  if (!formatoAntiguo) {
    Serial.println("ℹ️ NVS: formato actual OK, no necesita migración");
    return;
  }

  Serial.println("🔄 NVS: detectado formato antiguo — iniciando migración...");

  // ── Leer y migrar cada dispositivo ───────────────────────
  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    String pre = "d" + String(i) + "_";

    prefs.begin(NVS_NAMESPACE, true);
    // Solo migramos si existe la clave de nombre (mínimo indispensable)
    bool existe = prefs.isKey((pre + "nom").c_str());
    if (!existe) {
      prefs.end();
      continue;
    }

    // Leer con claves antiguas
    dispositivos[i].nombre    = prefs.getString((pre + "nom").c_str(), "");
    dispositivos[i].ip        = prefs.getString((pre + "ip").c_str(),  "");
    String macStr             = prefs.getString((pre + "mac").c_str(), "");
    dispositivos[i].numBoton  = prefs.getUChar((pre + "bot").c_str(),  1);
    dispositivos[i].estadoESP = prefs.getBool((pre + "estESP").c_str(),false);
    dispositivos[i].codigoRF  = prefs.getULong((pre + "rf").c_str(),  0);
    dispositivos[i].usarESPNOW= prefs.getBool((pre + "uESP").c_str(), false);
    dispositivos[i].usarRF    = prefs.getBool((pre + "uRF").c_str(),  false);
    dispositivos[i].activo    = prefs.getBool((pre + "act").c_str(),  false);
    dispositivos[i].maxFallos = prefs.getUChar((pre + "mFal").c_str(),DEF_MAX_FALLOS);
    dispositivos[i].tApagado  = prefs.getUChar((pre + "tAp").c_str(), DEF_T_APAGADO);
    dispositivos[i].tGracia   = prefs.getUChar((pre + "tGr").c_str(), DEF_T_GRACIA);
    if (macStr.length() > 0) stringToMac(macStr, dispositivos[i].mac);
    prefs.end();

    // Guardar en nuevo formato (1 clave)
    guardarDispositivoNVS(i);

    // Borrar claves antiguas
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove((pre + "nom").c_str());
    prefs.remove((pre + "ip").c_str());
    prefs.remove((pre + "mac").c_str());
    prefs.remove((pre + "bot").c_str());
    prefs.remove((pre + "estESP").c_str());
    prefs.remove((pre + "rf").c_str());
    prefs.remove((pre + "uESP").c_str());
    prefs.remove((pre + "uRF").c_str());
    prefs.remove((pre + "act").c_str());
    prefs.remove((pre + "mFal").c_str());
    prefs.remove((pre + "tAp").c_str());
    prefs.remove((pre + "tGr").c_str());
    prefs.end();

    Serial.printf("✅ Dispositivo %d migrado: %s\n", i, dispositivos[i].nombre.c_str());
  }

  Serial.println("✅ Migración NVS completada — formato nuevo activo");
}

// ============================================================
// CARGAR TODO DESDE NVS (config + 16 dispositivos)
// ============================================================
void cargarTodoNVS() {
  cargarConfigNVS();
  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    cargarDispositivoNVS(i);
  }
  Serial.println("✅ Todos los dispositivos cargados desde NVS");
}
