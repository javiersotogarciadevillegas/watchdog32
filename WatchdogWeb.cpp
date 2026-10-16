#include "WatchdogWeb.h"

// ============================================================
// HELPER: verificar autenticación
// ============================================================
bool checkAuth() {
  if (!server.authenticate("admin", cfgWebPass.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

// ============================================================
// HELPER: formatear timestamp millis() → "Xh Xm Xs"
// ============================================================
String formatUptime(unsigned long ms) {
  unsigned long seg = ms / 1000;
  unsigned long min = seg / 60;
  unsigned long hor = min / 60;
  seg %= 60; min %= 60;
  return String(hor) + "h " + String(min) + "m " + String(seg) + "s";
}

// ============================================================
// ESTILOS CSS COMUNES
// ============================================================
String getCSS() {
  String css = "<style>";
  css += "*{box-sizing:border-box;margin:0;padding:0;}";
  css += "body{font-family:'Segoe UI',sans-serif;background:#0f172a;color:#e2e8f0;padding:10px;}";
  css += "h1{color:#38bdf8;text-align:center;font-size:1.3rem;margin-bottom:6px;}";
  css += ".sub{text-align:center;color:#64748b;font-size:0.78rem;margin-bottom:12px;}";
  css += ".card{background:#1e293b;border:1px solid #334155;border-radius:14px;padding:14px;margin-bottom:14px;}";
  css += "h2{color:#fbbf24;font-size:0.95rem;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid #334155;display:flex;align-items:center;gap:6px;}";

  // Tabla desktop — scroll horizontal en móvil
  css += ".tbl-wrap{overflow-x:auto;-webkit-overflow-scrolling:touch;}";
  css += "table{width:100%;border-collapse:collapse;font-size:0.72rem;min-width:700px;}";
  css += "th{background:#0f172a;color:#38bdf8;padding:6px 4px;text-align:center;border-bottom:2px solid #334155;white-space:nowrap;}";
  css += "td{padding:4px 3px;border-bottom:1px solid #1a2744;vertical-align:middle;text-align:center;}";
  css += "tr:hover td{background:rgba(56,189,248,0.04);}";

  // Inputs de tabla — tamaño ajustado al contenido
  css += ".inp{background:#0f172a;border:1px solid #334155;border-radius:4px;color:#e2e8f0;padding:3px 4px;font-size:0.7rem;width:100%;}";
  css += ".inp-nom{width:130px;}";
  css += ".inp-ip{width:90px;font-family:monospace;}";
  css += ".inp-mac{width:108px;font-family:monospace;font-size:0.62rem;}";
  css += ".inp-bot{width:44px;text-align:center;}";
  css += ".inp-num{width:36px;text-align:center;}";
  css += ".inp-rf{width:80px;font-family:monospace;}";
  css += ".sel-est{width:58px;background:#0f172a;border:1px solid #334155;border-radius:4px;color:#e2e8f0;padding:2px;font-size:0.7rem;}";
  css += "input[type=checkbox]{width:15px;height:15px;cursor:pointer;accent-color:#38bdf8;}";

  // Botones
  css += ".btn{padding:10px 18px;border:none;border-radius:10px;font-weight:bold;cursor:pointer;font-size:0.82rem;}";
  css += ".btn-blue{background:#38bdf8;color:#0f172a;}";
  css += ".btn-green{background:#22c55e;color:white;}";
  css += ".btn-gray{background:#475569;color:white;}";
  css += ".btn-orange{background:#f97316;color:white;}";

  // Estados
  css += ".estado-ok{color:#22c55e;font-weight:bold;}";
  css += ".estado-fallo{color:#ef4444;font-weight:bold;}";
  css += ".estado-gracia{color:#fbbf24;font-weight:bold;}";
  css += ".estado-espera{color:#f97316;font-weight:bold;}";
  css += ".estado-inactivo{color:#475569;}";

  // Info IP
  css += ".ip-bar{text-align:center;background:#0f172a;border:1px solid #334155;border-radius:8px;padding:7px;margin-bottom:12px;font-size:0.78rem;}";
  css += ".ip-bar span{color:#22c55e;font-weight:bold;font-family:monospace;}";

  // Nav
  css += ".nav{display:flex;gap:8px;justify-content:center;flex-wrap:wrap;margin-bottom:14px;}";

  // Leyenda
  css += ".leyenda{background:#0f172a;border:1px solid #334155;border-radius:8px;padding:10px;margin-bottom:12px;font-size:0.76rem;color:#94a3b8;line-height:1.7;}";
  css += ".leyenda b{color:#e2e8f0;}";

  // Panel test
  css += ".test-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;}";
  css += "@media(max-width:480px){.test-grid{grid-template-columns:1fr;}}";
  css += ".test-box{background:#0f172a;border:1px solid #334155;border-radius:10px;padding:12px;}";
  css += ".test-title{font-weight:bold;font-size:0.82rem;margin-bottom:8px;}";
  css += ".test-inp{width:100%;padding:7px;background:#1e293b;border:1px solid #334155;border-radius:6px;color:#e2e8f0;margin-bottom:7px;font-size:0.8rem;box-sizing:border-box;}";
  css += ".test-btn{width:100%;padding:9px;border:none;border-radius:7px;font-weight:bold;cursor:pointer;font-size:0.8rem;}";

  // Tabla estado tiempo real — responsive cards en móvil
  css += ".estado-table{display:table;width:100%;}";
  css += "@media(max-width:600px){";
  css += ".estado-table,.estado-table thead,.estado-table tbody,.estado-table th,.estado-table td,.estado-table tr{display:block;}";
  css += ".estado-table thead{display:none;}";
  css += ".estado-table tr{background:#0f172a;border-radius:10px;padding:10px;margin-bottom:8px;border:1px solid #334155;}";
  css += ".estado-table td{text-align:left;padding:3px 0;font-size:0.8rem;}";
  css += ".estado-table td::before{content:attr(data-label)': ';color:#64748b;font-size:0.72rem;}}";

  // Config form
  css += "label{display:block;font-size:0.75rem;color:#94a3b8;margin-bottom:3px;margin-top:10px;}";
  css += "input.cfg,select.cfg{width:100%;padding:9px;background:#0f172a;border:1px solid #334155;border-radius:8px;color:#e2e8f0;font-size:0.85rem;}";
  css += ".form2{display:grid;grid-template-columns:1fr 1fr;gap:10px;}";
  css += "@media(max-width:480px){.form2{grid-template-columns:1fr;}}";

  // OK msg
  css += ".ok-msg{background:#052e16;border:1px solid #22c55e;color:#22c55e;padding:10px;border-radius:8px;text-align:center;margin-bottom:12px;font-weight:bold;}";
  css += "code{background:#0f172a;border:1px solid #334155;border-radius:3px;padding:1px 5px;font-size:0.8rem;color:#38bdf8;}";
  css += "</style>";
  return css;
}

// ============================================================
// PÁGINA PRINCIPAL
// ============================================================
void handleRaiz() {
  if (!checkAuth()) return;

  String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>🐕 Watchdog</title>";
  html += getCSS();
  html += "</head><body>";

  html += "<h1>🐕 WATCHDOG ESP32</h1>";
  html += "<div class='sub'>Monitor de red — Reinicio automático de dispositivos</div>";

  // Barra de estado
  html += "<div class='ip-bar'>IP: <span>" + WiFi.localIP().toString() + "</span>";
  html += " &nbsp;|&nbsp; Uptime: <span id='uptimeTxt'>" + formatUptime(millis()) + "</span>";
  html += " &nbsp;|&nbsp; WiFi: <span>" + WiFi.SSID() + " (" + String(WiFi.RSSI()) + " dBm)</span></div>";

  // Navegación
  html += "<div class='nav'>";
  html += "<a href='/log'><button class='btn btn-gray'>📋 LOG</button></a>";
  html += "<a href='/configWiFi'><button class='btn btn-blue'>📶 WIFI</button></a>";
  html += "<a href='/config'><button class='btn btn-blue'>⚙️ CONFIG</button></a>";
  html += "<a href='/ayuda' target='_blank'><button class='btn btn-gray'>📖 AYUDA</button></a>";
  html += "</div>";

  // ── PANEL TEST MANUAL ──────────────────────────────────────
  html += "<div class='card'><h2>🧪 TEST MANUAL</h2>";
  html += "<div class='test-grid'>";

  // Test RF
  html += "<div class='test-box'>";
  html += "<div class='test-title' style='color:#fbbf24'>📻 Enviar RF</div>";
  html += "<form action='/testRF' method='POST'>";
  html += "<input class='test-inp' type='number' name='codigoRF' placeholder='Código RF (ej: 12345678)'>";
  html += "<button class='test-btn' style='background:#f97316;color:white'>📻 ENVIAR RF</button>";
  html += "</form></div>";

  // Test ESP-NOW broadcast
  html += "<div class='test-box'>";
  html += "<div class='test-title' style='color:#38bdf8'>📡 Enviar ESP-NOW</div>";
  html += "<form action='/testESP' method='POST'>";
  html += "<input class='test-inp' type='number' name='codigo' placeholder='Código (ej: 9 = B5 ON)'>";
  html += "<div style='font-size:0.68rem;color:#64748b;margin-bottom:7px'>Se envía en broadcast a todos los enchufes</div>";
  html += "<button class='test-btn' style='background:#38bdf8;color:#0f172a'>📡 ENVIAR ESP-NOW</button>";
  html += "</form></div>";

  html += "</div></div>"; // fin test

  // ── LEYENDA ────────────────────────────────────────────────
  html += "<div class='card'><h2>📡 DISPOSITIVOS VIGILADOS</h2>";
  html += "<div class='leyenda'>";
  html += "<b>ℹ️ Cómo funciona:</b><br>";
  html += "• <b>IP</b> — obligatoria. Se hace ping HTTP a <code>/status</code> del dispositivo vigilado (cámara, LCD...).<br>";
  html += "• <b>Activo ☑</b> — debe estar marcado para que se vigile.<br>";
  html += "• <b>Botón</b> — nº de relé (1-18) del enchufe que alimenta este dispositivo.<br>";
  html += "• <b>St.N</b> (Estado Normal) — estado del relé cuando el dispositivo funciona: <b>ON</b> si el relé está encendido para alimentar, <b>OFF</b> si está apagado para alimentar. El Watchdog enviará el estado contrario para cortar y el normal para restaurar.<br>";
  html += "• <b>ESP☑ / RF☑</b> — activa el envío por ESP-NOW (broadcast) y/o RF al detectar fallo.<br>";
  html += "• <b>MAC</b> — solo informativa. El ESP-NOW se envía en broadcast, no necesita MAC.";
  html += "</div>";

  // ── TABLA DISPOSITIVOS ─────────────────────────────────────
  html += "<form action='/guardarDispositivos' method='POST'>";
  html += "<div class='tbl-wrap'>";
  html += "<table><thead><tr>";
  html += "<th>#</th><th>Nombre</th><th>IP vigilada</th>";
  html += "<th>Botón</th><th>St.N</th><th>ESP</th>";
  html += "<th>Código RF</th><th>RF</th>";
  html += "<th>Fallos</th><th>T.Ap(s)</th><th>T.Gr(s)</th>";
  html += "<th>Act</th><th>MAC (info)</th><th>Estado</th>";
  html += "</tr></thead><tbody>";

  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    String estadoTxt, estadoCls;
    if (!dispositivos[i].activo)       { estadoTxt="—";          estadoCls="estado-inactivo"; }
    else if (esperandoReencender[i])   { estadoTxt="⏳";          estadoCls="estado-espera";  }
    else if (enGracia[i])              { estadoTxt="🛡️";          estadoCls="estado-gracia";  }
    else if (fallosActuales[i] > 0)    { estadoTxt="⚠️"+String(fallosActuales[i])+"/"+String(dispositivos[i].maxFallos); estadoCls="estado-fallo"; }
    else if (dispositivos[i].ip.length()>0) { estadoTxt="✅ OK"; estadoCls="estado-ok";    }
    else                               { estadoTxt="Sin IP";      estadoCls="estado-inactivo"; }

    String pre = "d" + String(i) + "_";
    html += "<tr>";
    html += "<td><b style='color:#64748b'>" + String(i+1) + "</b></td>";
    html += "<td><input class='inp inp-nom' type='text' name='" + pre + "nom' value='" + dispositivos[i].nombre + "' placeholder='Nombre'></td>";
    html += "<td><input class='inp inp-ip' type='text' name='" + pre + "ip' value='" + dispositivos[i].ip + "' placeholder='192.168.1.x'></td>";
    html += "<td><input class='inp inp-bot' type='number' name='" + pre + "bot' value='" + String(dispositivos[i].numBoton) + "' min='1' max='18'></td>";
    html += "<td><select class='sel-est' name='" + pre + "est' title='Estado normal del relé cuando el dispositivo funciona'>";
    html += "<option value='1'" + String(dispositivos[i].estadoESP?" selected":"") + ">🟢ON</option>";
    html += "<option value='0'" + String(!dispositivos[i].estadoESP?" selected":"") + ">🔴OFF</option>";
    html += "</select></td>";
    html += "<td><input type='checkbox' name='" + pre + "uESP'" + String(dispositivos[i].usarESPNOW?" checked":"") + "></td>";
    html += "<td><input class='inp inp-rf' type='text' name='" + pre + "rf' value='" + String(dispositivos[i].codigoRF) + "' placeholder='0'></td>";
    html += "<td><input type='checkbox' name='" + pre + "uRF'" + String(dispositivos[i].usarRF?" checked":"") + "></td>";
    html += "<td><input class='inp inp-num' type='number' name='" + pre + "mFal' value='" + String(dispositivos[i].maxFallos) + "' min='1' max='10'></td>";
    html += "<td><input class='inp inp-num' type='number' name='" + pre + "tAp' value='" + String(dispositivos[i].tApagado) + "' min='1' max='60'></td>";
    html += "<td><input class='inp inp-num' type='number' name='" + pre + "tGr' value='" + String(dispositivos[i].tGracia) + "' min='10' max='300'></td>";
    html += "<td><input type='checkbox' name='" + pre + "act'" + String(dispositivos[i].activo?" checked":"") + "></td>";
    html += "<td><input class='inp inp-mac' type='text' name='" + pre + "mac' value='" + macToString(dispositivos[i].mac) + "' placeholder='AA:BB:CC:DD:EE:FF'></td>";
    html += "<td class='" + estadoCls + "'>" + estadoTxt + "</td>";
    html += "</tr>";
  }

  html += "</tbody></table></div>";
  html += "<div style='text-align:center;margin-top:12px'>";
  html += "<button type='submit' class='btn btn-green'>💾 GUARDAR DISPOSITIVOS</button></div>";
  html += "</form></div>";

  // ── ESTADO TIEMPO REAL ─────────────────────────────────────
  html += "<div class='card'>";
  html += "<h2>📊 ESTADO <span style='color:#64748b;font-size:0.72rem;font-weight:normal'>(refresco cada 5s)</span></h2>";
  html += "<table class='estado-table'><thead><tr>";
  html += "<th>Dispositivo</th><th>IP</th><th>Fallos</th><th>Estado</th><th>Último OK</th>";
  html += "</tr></thead><tbody>";

  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    if (dispositivos[i].nombre.length()==0 && dispositivos[i].ip.length()==0) continue;
    String estado, cls;
    if (!dispositivos[i].activo)     { estado="Inactivo";     cls="estado-inactivo"; }
    else if (esperandoReencender[i]) { estado="⏳ Apagando"; cls="estado-espera";   }
    else if (enGracia[i])            { estado="🛡️ Gracia";   cls="estado-gracia";   }
    else if (fallosActuales[i]>0)    { estado="⚠️ "+String(fallosActuales[i])+"/"+String(dispositivos[i].maxFallos); cls="estado-fallo"; }
    else                             { estado="✅ OK";         cls="estado-ok";       }

    String ultimoOK = tiempoUltimoOK[i]==0 ? "—" : formatUptime(millis()-tiempoUltimoOK[i])+" atrás";
    html += "<tr>";
    html += "<td data-label='Dispositivo'>" + dispositivos[i].nombre + "</td>";
    html += "<td data-label='IP'>" + dispositivos[i].ip + "</td>";
    html += "<td data-label='Fallos' id='fal"+String(i)+"'>" + String(fallosActuales[i]) + "</td>";
    html += "<td data-label='Estado' id='est"+String(i)+"' class='"+cls+"'>" + estado + "</td>";
    html += "<td data-label='Último OK' id='ok"+String(i)+"' style='color:#64748b'>" + ultimoOK + "</td>";
    html += "</tr>";
  }
  html += "</tbody></table></div>";

  // Auto-refresco
  html += "<script>";
  html += "function refrescarEstado(){fetch('/estado').then(r=>r.json()).then(data=>{";
  html += "let u=document.getElementById('uptimeTxt');if(u)u.innerText=data.uptime;";
  html += "for(let i=0;i<data.estados.length;i++){";
  html += "let e=document.getElementById('est'+i);if(e&&e.innerText!==data.estados[i].txt){e.innerText=data.estados[i].txt;e.className=data.estados[i].cls;}";
  html += "let f=document.getElementById('fal'+i);if(f&&f.innerText!==String(data.estados[i].fallos))f.innerText=data.estados[i].fallos;";
  html += "let o=document.getElementById('ok'+i);if(o&&o.innerText!==data.estados[i].ultimoOK)o.innerText=data.estados[i].ultimoOK;";
  html += "}}).catch(()=>{});}";
  html += "setInterval(refrescarEstado,5000);";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

// ============================================================
// GUARDAR DISPOSITIVOS — POST
// ============================================================
void handleGuardarDispositivos() {
  if (!checkAuth()) return;

  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    String pre = "d" + String(i) + "_";

    if (server.hasArg(pre + "nom")) dispositivos[i].nombre = server.arg(pre + "nom");
    if (server.hasArg(pre + "ip"))  dispositivos[i].ip     = server.arg(pre + "ip");

    // MAC — solo informativa, no crítica
    String macStr = server.arg(pre + "mac");
    if (macStr.length() > 0) stringToMac(macStr, dispositivos[i].mac);

    dispositivos[i].numBoton   = server.arg(pre + "bot").toInt();
    dispositivos[i].estadoESP  = (server.arg(pre + "est") == "1");
    dispositivos[i].codigoRF   = server.arg(pre + "rf").toInt();
    dispositivos[i].usarESPNOW = server.hasArg(pre + "uESP");
    dispositivos[i].usarRF     = server.hasArg(pre + "uRF");
    dispositivos[i].activo     = server.hasArg(pre + "act");
    dispositivos[i].maxFallos  = max(1, (int)server.arg(pre + "mFal").toInt());
    dispositivos[i].tApagado   = max(1, (int)server.arg(pre + "tAp").toInt());
    dispositivos[i].tGracia    = max(10,(int)server.arg(pre + "tGr").toInt());

    guardarDispositivoNVS(i);

    if (dispositivos[i].activo && dispositivos[i].usarESPNOW)
      registrarPeer(dispositivos[i].mac);
  }

  añadirLog("SISTEMA", "Dispositivos actualizados desde web");
  server.sendHeader("Location", "/?ok=1");
  server.send(303);
}

// ============================================================
// PÁGINA CONFIGURACIÓN GLOBAL
// ============================================================
void handleConfiguracion() {
  if (!checkAuth()) return;
  bool guardado = server.hasArg("ok");

  String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>⚙️ Config Watchdog</title>" + getCSS() + "</head><body>";
  html += "<div style='max-width:480px;margin:0 auto'>";
  html += "<h1>⚙️ CONFIGURACIÓN</h1>";
  if (guardado) html += "<div class='ok-msg'>✅ Guardado — reiniciando...</div>";

  html += "<div class='card'><form action='/guardarConfig' method='POST'>";

  html += "<h2>💾 BACKUP / RESTAURACIÓN</h2>";
  html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:15px'>";

  // Exportar
  html += "<div style='background:#0f172a;border:1px solid #334155;border-radius:10px;padding:12px;text-align:center'>";
  html += "<div style='color:#22c55e;font-weight:bold;font-size:0.85rem;margin-bottom:6px'>📥 EXPORTAR</div>";
  html += "<div style='color:#64748b;font-size:0.72rem;margin-bottom:10px'>Descarga un fichero .json con toda la configuración actual</div>";
  html += "<a href='/exportNVS'><button type='button' class='btn btn-green' style='width:100%'>💾 Descargar config.json</button></a>";
  html += "</div>";

  // Importar
  html += "<div style='background:#0f172a;border:1px solid #334155;border-radius:10px;padding:12px;text-align:center'>";
  html += "<div style='color:#38bdf8;font-weight:bold;font-size:0.85rem;margin-bottom:6px'>📤 IMPORTAR</div>";
  html += "<div style='color:#64748b;font-size:0.72rem;margin-bottom:10px'>Sube un fichero .json exportado anteriormente</div>";
  html += "<form action='/importNVS' method='POST' enctype='multipart/form-data'>";
  html += "<input type='file' name='nvs' accept='.json' style='width:100%;background:#1e293b;border:1px solid #334155;border-radius:6px;color:#e2e8f0;padding:5px;margin-bottom:8px;font-size:0.72rem;box-sizing:border-box'>";
  html += "<button type='submit' class='btn btn-blue' style='width:100%'>📤 Importar y reiniciar</button>";
  html += "</form></div>";

  html += "</div>";

  html += "<h2>📶 WiFi</h2>";
  html += "<div class='form2'>";
  html += "<div><label>SSID</label><input class='cfg' type='text' name='ssid' value='" + cfgSSID + "'></div>";
  html += "<div><label>Contraseña WiFi</label><input class='cfg' type='password' name='pass' value='" + cfgPass + "'></div>";
  html += "</div>";

  html += "<h2>🌐 Red (IP fija del Watchdog)</h2>";
  html += "<div class='form2'>";
  html += "<div><label>IP Watchdog</label><input class='cfg' type='text' name='ip' value='" + cfgIP + "'></div>";
  html += "<div><label>Gateway</label><input class='cfg' type='text' name='gw' value='" + cfgGW + "'></div>";
  html += "<div><label>Máscara</label><input class='cfg' type='text' name='sn' value='" + cfgSN + "'></div>";
  html += "<div><label>DNS</label><input class='cfg' type='text' name='dns' value='" + cfgDNS + "'></div>";
  html += "</div>";

  html += "<h2>🐕 Watchdog</h2>";
  html += "<label>Intervalo entre pings (seg, 5-300)</label>";
  html += "<input class='cfg' type='number' name='intervalo' value='" + String(cfgIntervalo) + "' min='5' max='300'>";

  html += "<h2>🔐 Acceso web</h2>";
  html += "<label>Contraseña (usuario: admin)</label>";
  html += "<input class='cfg' type='password' name='webpass' value='" + cfgWebPass + "'>";

  html += "<br><br><button type='submit' class='btn btn-green' style='width:100%'>💾 GUARDAR Y REINICIAR</button>";
  html += "</form></div>";
  html += "<div style='text-align:center;margin-top:10px'><a href='/' style='color:#38bdf8'>🏠 Volver</a></div>";
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

// ============================================================
// GUARDAR CONFIGURACIÓN GLOBAL
// ============================================================
void handleGuardarConfig() {
  if (!checkAuth()) return;
  if (server.hasArg("ssid"))      cfgSSID      = server.arg("ssid");
  if (server.hasArg("pass"))      cfgPass      = server.arg("pass");
  if (server.hasArg("ip"))        cfgIP        = server.arg("ip");
  if (server.hasArg("gw"))        cfgGW        = server.arg("gw");
  if (server.hasArg("sn"))        cfgSN        = server.arg("sn");
  if (server.hasArg("dns"))       cfgDNS       = server.arg("dns");
  if (server.hasArg("webpass"))   cfgWebPass   = server.arg("webpass");
  if (server.hasArg("intervalo")) cfgIntervalo = max(5, (int)server.arg("intervalo").toInt());
  guardarConfigNVS();
  añadirLog("SISTEMA", "Configuración global actualizada — SSID:" + cfgSSID + " IP:" + cfgIP);
  Serial.println("💾 Config guardada: SSID=" + cfgSSID + " IP=" + cfgIP);
  server.send(200,"text/html; charset=utf-8",
    "<html><body style='background:#0f172a;color:#22c55e;text-align:center;padding-top:60px'>"
    "<h2>✅ Guardado</h2><p style='color:#94a3b8'>Reiniciando...</p></body></html>");
  delay(2000); ESP.restart();
}

// ============================================================
// PÁGINA LOG
// ============================================================
void handleLog() {
  if (!checkAuth()) return;

  String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>📋 Log Watchdog</title>" + getCSS() + "</head><body>";
  html += "<h1>📋 LOG DE EVENTOS</h1>";
  html += "<div class='nav'>";
  html += "<a href='/'><button class='btn btn-gray'>🏠 Volver</button></a>";
  html += "<a href='/borrarLog'><button class='btn' style='background:#ef4444;color:white'>🗑️ Borrar</button></a>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<table style='min-width:300px'><thead><tr><th>Uptime</th><th>Dispositivo</th><th>Evento</th></tr></thead><tbody>";

  int count = 0;
  for (int i = 0; i < MAX_LOG; i++) {
    int idx = ((logIndex - 1 - i) + MAX_LOG) % MAX_LOG;
    if (!logEventos[idx].valido) continue;
    html += "<tr>";
    html += "<td style='color:#64748b;white-space:nowrap'>" + formatUptime(logEventos[idx].timestamp) + "</td>";
    html += "<td style='color:#fbbf24'>" + logEventos[idx].dispositivo + "</td>";
    html += "<td>" + logEventos[idx].evento + "</td>";
    html += "</tr>";
    count++;
  }
  if (count == 0)
    html += "<tr><td colspan='3' style='color:#475569;text-align:center'>Sin eventos</td></tr>";

  html += "</tbody></table></div></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

// ============================================================
// BORRAR LOG
// ============================================================
void handleBorrarLog() {
  if (!checkAuth()) return;
  for (int i = 0; i < MAX_LOG; i++) logEventos[i].valido = false;
  logIndex = 0;
  server.sendHeader("Location", "/log");
  server.send(303);
}

// ============================================================
// STATUS JSON — para que otros vigilen al Watchdog
// ============================================================
void handleStatus() {
  String json = "{\"status\":\"ok\",\"uptime\":" + String(millis()/1000) + "}";
  server.send(200, "application/json", json);
}

// ============================================================
// TEST MANUAL RF
// ============================================================
void handleTestRF() {
  if (!checkAuth()) return;
  if (server.hasArg("codigoRF")) {
    unsigned long cod = strtoul(server.arg("codigoRF").c_str(), nullptr, 10);
    if (cod > 0) {
      rf.send(cod, 24);
      Serial.printf("🧪 Test RF: %lu\n", cod);
      añadirLog("TEST", "📻 RF enviado: " + String(cod));
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================================================
// TEST MANUAL ESP-NOW — broadcast sin MAC
// ============================================================
void handleTestESP() {
  if (!checkAuth()) return;
  if (server.hasArg("codigo")) {
    int cod = server.arg("codigo").toInt();
    // ✅ Broadcast: FF:FF:FF:FF:FF:FF — todos los enchufes escuchan
    uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    registrarPeer(broadcast);
    struct_message msg;
    msg.controlValue = cod;
    esp_err_t res = esp_now_send(broadcast, (uint8_t*)&msg, sizeof(msg));
    String resultado = (res == ESP_OK) ? "✅ Enviado" : "❌ Error";
    Serial.printf("🧪 Test ESP-NOW broadcast cod=%d → %s\n", cod, resultado.c_str());
    añadirLog("TEST", "📡 ESP-NOW broadcast cod=" + String(cod) + " → " + resultado);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================================================
// ESTADO JSON — auto-refresco
// ============================================================
void handleEstado() {
  String json = "{";
  json += "\"uptime\":\"" + formatUptime(millis()) + "\"";
  json += ",\"estados\":[";
  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    String txt, cls;
    if (!dispositivos[i].activo)     { txt="Inactivo";  cls="estado-inactivo"; }
    else if (esperandoReencender[i]) { txt="⏳";         cls="estado-espera";  }
    else if (enGracia[i])            { txt="🛡️ Gracia"; cls="estado-gracia";  }
    else if (fallosActuales[i]>0) {
      txt="⚠️ "+String(fallosActuales[i])+"/"+String(dispositivos[i].maxFallos);
      cls="estado-fallo";
    } else { txt="✅ OK"; cls="estado-ok"; }

    String ultimoOK = tiempoUltimoOK[i]==0 ? "—" :
      formatUptime(millis()-tiempoUltimoOK[i])+" atrás";

    if (i>0) json += ",";
    json += "{\"txt\":\""+txt+"\",\"cls\":\""+cls+"\"";
    json += ",\"fallos\":"+String(fallosActuales[i]);
    json += ",\"ultimoOK\":\""+ultimoOK+"\"}";
  }
  json += "]}";
  server.send(200, "application/json; charset=utf-8", json);
}

// ============================================================
// EXPORTAR NVS → JSON descargable
// ============================================================
void handleExportNVS() {
  if (!checkAuth()) return;

  String json = "{\n";

  // ── Config global ─────────────────────────────────────────
  json += "  \"config\": {\n";
  json += "    \"ssid\": \""     + cfgSSID      + "\",\n";
  json += "    \"pass\": \""     + cfgPass      + "\",\n";
  json += "    \"webpass\": \""  + cfgWebPass   + "\",\n";
  json += "    \"ip\": \""       + cfgIP        + "\",\n";
  json += "    \"gw\": \""       + cfgGW        + "\",\n";
  json += "    \"sn\": \""       + cfgSN        + "\",\n";
  json += "    \"dns\": \""      + cfgDNS       + "\",\n";
  json += "    \"intervalo\": "  + String(cfgIntervalo) + "\n";
  json += "  },\n";

  // ── Dispositivos ──────────────────────────────────────────
  json += "  \"dispositivos\": [\n";
  for (int i = 0; i < MAX_DISPOSITIVOS; i++) {
    json += "    {\n";
    json += "      \"nombre\": \""    + dispositivos[i].nombre + "\",\n";
    json += "      \"ip\": \""        + dispositivos[i].ip     + "\",\n";
    json += "      \"mac\": \""       + macToString(dispositivos[i].mac) + "\",\n";
    json += "      \"numBoton\": "    + String(dispositivos[i].numBoton)  + ",\n";
    json += "      \"estadoESP\": "   + String(dispositivos[i].estadoESP  ? "true" : "false") + ",\n";
    json += "      \"codigoRF\": "    + String(dispositivos[i].codigoRF)  + ",\n";
    json += "      \"usarESPNOW\": "  + String(dispositivos[i].usarESPNOW ? "true" : "false") + ",\n";
    json += "      \"usarRF\": "      + String(dispositivos[i].usarRF     ? "true" : "false") + ",\n";
    json += "      \"activo\": "      + String(dispositivos[i].activo     ? "true" : "false") + ",\n";
    json += "      \"maxFallos\": "   + String(dispositivos[i].maxFallos) + ",\n";
    json += "      \"tApagado\": "    + String(dispositivos[i].tApagado)  + ",\n";
    json += "      \"tGracia\": "     + String(dispositivos[i].tGracia)   + "\n";
    json += "    }";
    if (i < MAX_DISPOSITIVOS - 1) json += ",";
    json += "\n";
  }
  json += "  ]\n";
  json += "}\n";

  // Forzar descarga como fichero
  server.sendHeader("Content-Disposition", "attachment; filename=\"watchdog_config.json\"");
  server.sendHeader("Content-Type", "application/json");
  server.sendHeader("Content-Length", String(json.length()));
  server.send(200, "application/json", json);

  Serial.println("📥 Config NVS exportada como JSON");
}

// ============================================================
// IMPORTAR NVS ← JSON subido
// ============================================================
void handleImportNVS() {
  // ✅ FIX: NO llamar checkAuth() aquí
  // El handler de upload se llama múltiples veces (START, WRITE, END)
  // y checkAuth() falla en las llamadas intermedias cortando el upload
  // La autenticación se hace en el handler de respuesta (lambda vacía del server.on)

  HTTPUpload& upload = server.upload();
  static String jsonBuffer = "";

  if (upload.status == UPLOAD_FILE_START) {
    jsonBuffer = "";
    Serial.println("📤 Importando config NVS...");
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    for (size_t i = 0; i < upload.currentSize; i++) {
      jsonBuffer += (char)upload.buf[i];
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    Serial.printf("📤 JSON recibido: %d bytes\n", jsonBuffer.length());

    // ── Helpers de parser JSON ────────────────────────────
    auto getStr = [](const String& json, const String& key) -> String {
      String search = "\"" + key + "\": \"";
      int idx = json.indexOf(search);
      if (idx < 0) return "";
      idx += search.length();
      int end = json.indexOf("\"", idx);
      return (end > idx) ? json.substring(idx, end) : "";
    };

    auto getInt = [](const String& json, const String& key) -> int {
      String search = "\"" + key + "\": ";
      int idx = json.indexOf(search);
      if (idx < 0) return 0;
      idx += search.length();
      int end = idx;
      while (end < (int)json.length() &&
             (isDigit(json[end]) || json[end]=='-')) end++;
      return json.substring(idx, end).toInt();
    };

    auto getBool = [](const String& json, const String& key) -> bool {
      String search = "\"" + key + "\": ";
      int idx = json.indexOf(search);
      if (idx < 0) return false;
      idx += search.length();
      return json.substring(idx, idx + 4) == "true";
    };

    // ── Config global ─────────────────────────────────────
    int cfgStart = jsonBuffer.indexOf("\"config\":");
    int cfgEnd   = jsonBuffer.indexOf("\"dispositivos\":");
    if (cfgStart >= 0 && cfgEnd > cfgStart) {
      String cfgBlock = jsonBuffer.substring(cfgStart, cfgEnd);
      String v;
      v = getStr(cfgBlock, "ssid");     if (v.length()>0) cfgSSID    = v;
      v = getStr(cfgBlock, "pass");     if (v.length()>0) cfgPass    = v;
      v = getStr(cfgBlock, "webpass");  if (v.length()>0) cfgWebPass = v;
      v = getStr(cfgBlock, "ip");       if (v.length()>0) cfgIP      = v;
      v = getStr(cfgBlock, "gw");       if (v.length()>0) cfgGW      = v;
      v = getStr(cfgBlock, "sn");       if (v.length()>0) cfgSN      = v;
      v = getStr(cfgBlock, "dns");      if (v.length()>0) cfgDNS     = v;
      int iv = getInt(cfgBlock, "intervalo");
      if (iv >= 5) cfgIntervalo = iv;
      guardarConfigNVS();
      Serial.println("✅ Config global importada");
    }

    // ── Dispositivos ──────────────────────────────────────
    // ✅ FIX: buscar el array "dispositivos" y procesar cada objeto
    // El bug anterior: indexOf("{") encontraba el { de "config" primero
    // Ahora buscamos el "[" del array y procesamos objeto a objeto
    int arrStart = jsonBuffer.indexOf("\"dispositivos\": [");
    if (arrStart < 0) arrStart = jsonBuffer.indexOf("\"dispositivos\":[");
    if (arrStart >= 0) {
      // Saltar hasta el primer [ del array
      int pos = jsonBuffer.indexOf("[", arrStart);
      if (pos >= 0) pos++; // saltar el [

      for (int i = 0; i < MAX_DISPOSITIVOS && pos >= 0; i++) {
        // Buscar el siguiente { que abre un objeto del array
        int open = jsonBuffer.indexOf("{", pos);
        if (open < 0) break;

        // Buscar el } de cierre de ESTE objeto
        // Tenemos que contar llaves para no confundirnos si hubiera anidado
        int depth = 0;
        int close = -1;
        for (int j = open; j < (int)jsonBuffer.length(); j++) {
          if (jsonBuffer[j] == '{') depth++;
          else if (jsonBuffer[j] == '}') {
            depth--;
            if (depth == 0) { close = j; break; }
          }
        }
        if (close < 0) break;

        String obj = jsonBuffer.substring(open, close + 1);
        pos = close + 1;

        dispositivos[i].nombre    = getStr(obj, "nombre");
        dispositivos[i].ip        = getStr(obj, "ip");
        String macStr             = getStr(obj, "mac");
        if (macStr.length() > 0) stringToMac(macStr, dispositivos[i].mac);
        dispositivos[i].numBoton  = (uint8_t)getInt(obj, "numBoton");
        dispositivos[i].estadoESP = getBool(obj, "estadoESP");
        dispositivos[i].codigoRF  = (uint32_t)getInt(obj, "codigoRF");
        dispositivos[i].usarESPNOW= getBool(obj, "usarESPNOW");
        dispositivos[i].usarRF    = getBool(obj, "usarRF");
        dispositivos[i].activo    = getBool(obj, "activo");
        dispositivos[i].maxFallos = (uint8_t)max(1, getInt(obj, "maxFallos"));
        dispositivos[i].tApagado  = (uint8_t)max(1, getInt(obj, "tApagado"));
        dispositivos[i].tGracia   = (uint8_t)max(10,getInt(obj, "tGracia"));

        guardarDispositivoNVS(i);
        Serial.printf("  ✅ Disp %d: %s\n", i, dispositivos[i].nombre.c_str());
      }
      Serial.println("✅ Dispositivos importados");
    } else {
      Serial.println("❌ No se encontró array dispositivos en el JSON");
    }

    añadirLog("SISTEMA", "Config importada desde JSON — reiniciando");
    server.send(200, "text/html; charset=utf-8",
      "<html><body style='background:#0f172a;color:#22c55e;text-align:center;padding-top:60px'>"
      "<h2>✅ Config importada</h2>"
      "<p style='color:#94a3b8'>Reiniciando para aplicar...</p>"
      "</body></html>");
    delay(2000);
    ESP.restart();
  }
}

// ============================================================
// WIFI MULTI-RED — variables internas
// ============================================================
static bool    wifiReconectarPendiente = false;
static String  wifiSsidPendiente       = "";
static String  wifiPassPendiente       = "";
static unsigned long ultimoCheckWiFi   = 0;
static bool    wifiAutoSwitchActivo    = true;

// ============================================================
// CONECTAR WIFI AL ARRANQUE — intenta preferida primero
// ============================================================
void conectarWiFiMultiRed() {
  // Aplicar IP estática antes de conectar
  IPAddress ip, gw, sn, dns;
  if (ip.fromString(cfgIP) && gw.fromString(cfgGW) &&
      sn.fromString(cfgSN) && dns.fromString(cfgDNS)) {
    WiFi.config(ip, gw, sn, dns);
  } else {
    IPAddress dIP(192,168,1,250), dGW(192,168,1,1);
    IPAddress dSN(255,255,255,0), dDNS(8,8,8,8);
    WiFi.config(dIP, dGW, dSN, dDNS);
  }

  // Cargar redes guardadas desde NVS
  prefs.begin(NVS_NAMESPACE, true);
  for (int i = 0; i < WIFI_MAX_SLOTS; i++) {
    wifiSSIDs[i]  = prefs.getString(("wssid" + String(i)).c_str(), "");
    wifiPasses[i] = prefs.getString(("wpass" + String(i)).c_str(), "");
  }
  wifiPref = prefs.getInt("wifiPref", 0);
  prefs.end();

  // Si no hay redes guardadas usar las del código
  bool hayRedes = false;
  for (int i = 0; i < WIFI_MAX_SLOTS; i++)
    if (wifiSSIDs[i].length() > 0) { hayRedes = true; break; }
  if (!hayRedes) {
    wifiSSIDs[0]  = cfgSSID;
    wifiPasses[0] = cfgPass;
    Serial.println("ℹ️ Sin redes guardadas, usando SSID por defecto: " + cfgSSID);
  }

  // Orden: preferida primero, luego el resto
  int orden[WIFI_MAX_SLOTS];
  orden[0] = wifiPref;
  int k = 1;
  for (int i = 0; i < WIFI_MAX_SLOTS; i++)
    if (i != wifiPref) orden[k++] = i;

  WiFi.mode(WIFI_STA);
  for (int t = 0; t < WIFI_MAX_SLOTS; t++) {
    int idx = orden[t];
    if (wifiSSIDs[idx].length() == 0) continue;
    Serial.printf("📶 Intentando slot %d: %s\n", idx, wifiSSIDs[idx].c_str());
    WiFi.disconnect(false);
    delay(100);
    WiFi.begin(wifiSSIDs[idx].c_str(), wifiPasses[idx].c_str());
    unsigned long inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < 8000) {
      delay(300); Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("✅ Conectado a: %s  IP: %s\n",
        wifiSSIDs[idx].c_str(), WiFi.localIP().toString().c_str());
      return;
    }
    Serial.printf("❌ No conectó a %s\n", wifiSSIDs[idx].c_str());
  }
  Serial.println("⚠️ No se pudo conectar a ninguna red.");
}

// ============================================================
// CHEQUEO PERIÓDICO WIFI — llamar desde loop()
// ============================================================
void chequearCambioWiFi() {
  if (wifiReconectarPendiente) {
    wifiReconectarPendiente = false;
    Serial.printf("🔀 Reconectando a: %s\n", wifiSsidPendiente.c_str());
    WiFi.disconnect(false); delay(200);
    WiFi.begin(wifiSsidPendiente.c_str(), wifiPassPendiente.c_str());
    unsigned long ini = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - ini < 12000) {
      delay(300); Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() != WL_CONNECTED) conectarWiFiMultiRed();
    return;
  }
  if (!wifiAutoSwitchActivo) return;
  if (millis() - ultimoCheckWiFi < WIFI_CHECK_INTERVAL) return;
  ultimoCheckWiFi = millis();
  if (WiFi.status() != WL_CONNECTED) { conectarWiFiMultiRed(); return; }
  int rssiActual = WiFi.RSSI();
  if (rssiActual > WIFI_SWITCH_THRESHOLD) return;
  Serial.printf("📉 Señal débil (%d dBm), buscando mejor...\n", rssiActual);
  int n = WiFi.scanNetworks(false, true);
  int mejorSlot = -1, mejorRSSI = rssiActual + WIFI_MIN_IMPROVEMENT;
  String ssidActual = WiFi.SSID();
  for (int i = 0; i < n; i++) {
    String sc = WiFi.SSID(i);
    if (sc == ssidActual) continue;
    for (int s = 0; s < WIFI_MAX_SLOTS; s++)
      if (wifiSSIDs[s] == sc && WiFi.RSSI(i) > mejorRSSI)
        { mejorSlot = s; mejorRSSI = WiFi.RSSI(i); }
  }
  WiFi.scanDelete();
  if (mejorSlot >= 0) {
    wifiSsidPendiente = wifiSSIDs[mejorSlot];
    wifiPassPendiente = wifiPasses[mejorSlot];
    wifiReconectarPendiente = true;
  }
}

// ============================================================
// PÁGINA WIFI MULTI-RED
// ============================================================
void handleConfigWiFi() {
  if (!checkAuth()) return;
  String ssidActual = WiFi.SSID();
  int    rssiActual = WiFi.RSSI();
  bool   conectado  = (WiFi.status() == WL_CONNECTED);

  int rssiSlot[WIFI_MAX_SLOTS];
  for (int i = 0; i < WIFI_MAX_SLOTS; i++) rssiSlot[i] = -100;
  if (conectado) {
    for (int i = 0; i < WIFI_MAX_SLOTS; i++)
      if (wifiSSIDs[i] == ssidActual) rssiSlot[i] = rssiActual;
    int n = WiFi.scanNetworks(false, true);
    for (int i = 0; i < n; i++) {
      String sc = WiFi.SSID(i);
      for (int s = 0; s < WIFI_MAX_SLOTS; s++)
        if (wifiSSIDs[s].length()>0 && wifiSSIDs[s]==sc && wifiSSIDs[s]!=ssidActual)
          rssiSlot[s] = WiFi.RSSI(i);
    }
    WiFi.scanDelete();
  }

  String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>📶 WiFi Watchdog</title>" + getCSS();
  html += "<style>";
  html += ".red-card{background:#0f172a;border:2px solid #334155;border-radius:12px;padding:12px;margin-bottom:10px;}";
  html += ".red-card.conectada{border-color:#22c55e;background:rgba(34,197,94,0.05);}";
  html += ".red-card.preferida{border-color:#fbbf24;}";
  html += ".badge{display:inline-block;padding:2px 8px;border-radius:6px;font-size:0.65rem;font-weight:bold;margin-left:6px;}";
  html += ".badge-ok{background:#052e16;color:#22c55e;} .badge-pref{background:#451a03;color:#fbbf24;}";
  html += "</style></head><body>";
  html += "<h1>📶 WIFI MANAGER</h1><div class='sub'>Watchdog · Multi-Red</div>";

  html += "<div class='ip-bar'>Estado: <span>" + String(conectado?"✅ Conectado":"❌ Sin conexión") + "</span>";
  if (conectado) {
    html += " &nbsp;|&nbsp; Red: <span>" + ssidActual + "</span>";
    html += " &nbsp;|&nbsp; IP: <span>" + WiFi.localIP().toString() + "</span>";
    html += " &nbsp;|&nbsp; <span>" + String(rssiActual) + " dBm</span>";
  }
  html += "</div>";
  html += "<div class='nav'><a href='/'><button class='btn btn-gray'>🏠 VOLVER</button></a></div>";

  html += "<div class='card'><h2>📡 REDES GUARDADAS</h2>";
  for (int i = 0; i < WIFI_MAX_SLOTS; i++) {
    bool esCon  = (wifiSSIDs[i]==ssidActual && conectado && wifiSSIDs[i].length()>0);
    bool esPref = (i==wifiPref && wifiSSIDs[i].length()>0);
    String cls  = String("red-card") + (esCon?" conectada":(esPref?" preferida":""));
    html += "<div class='" + cls + "'>";
    html += "<div style='display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:6px'><div>";
    html += "<div style='font-size:0.7rem;color:#64748b'>Slot " + String(i+1) + "</div>";
    if (wifiSSIDs[i].length()>0) {
      html += "<div style='font-size:0.9rem;font-weight:bold'>" + wifiSSIDs[i];
      if (esCon)  html += "<span class='badge badge-ok'>✅ CONECTADA</span>";
      if (esPref) html += "<span class='badge badge-pref'>⭐ PREFERIDA</span>";
      html += "</div>";
      int r=rssiSlot[i], barras=(r>-50)?4:(r>-65)?3:(r>-75)?2:(r>-85)?1:0;
      html += "<div style='font-size:0.72rem;color:#64748b;margin-top:3px'>" + String(r) + " dBm ";
      for (int b=0;b<4;b++) {
        int h2=4+b*3; String on=(b<barras)?" on":"";
        html += "<span style='display:inline-block;width:4px;height:"+String(h2)+"px;background:"+(b<barras?"#22c55e":"#334155")+";border-radius:1px;margin-left:2px;vertical-align:bottom'></span>";
      }
      html += "</div>";
    } else { html += "<div style='color:#475569'>— Vacío —</div>"; }
    html += "</div><div style='display:flex;gap:6px;flex-wrap:wrap'>";
    if (wifiSSIDs[i].length()>0 && !esPref) {
      html += "<form action='/setPreferida' method='POST' style='margin:0'>";
      html += "<input type='hidden' name='slot' value='" + String(i) + "'>";
      html += "<button type='submit' class='btn' style='background:#451a03;color:#fbbf24;padding:6px 10px;font-size:0.72rem'>⭐</button></form>";
    }
    if (wifiSSIDs[i].length()>0)
      html += "<a href='/borrarWiFi?slot="+String(i)+"'><button class='btn' style='background:#7f1d1d;color:#fca5a5;padding:6px 10px;font-size:0.72rem'>🗑️</button></a>";
    html += "</div></div></div>";
  }
  html += "</div>";

  html += "<div class='card'><h2>➕ AÑADIR / MODIFICAR RED</h2>";
  html += "<form action='/guardarWiFi' method='POST'>";
  html += "<label>Slot</label><select name='slot' class='cfg'>";
  for (int i=0;i<WIFI_MAX_SLOTS;i++) {
    html += "<option value='"+String(i)+"'>Slot "+String(i+1);
    if (wifiSSIDs[i].length()>0) html += " — "+wifiSSIDs[i];
    html += "</option>";
  }
  html += "</select>";
  html += "<label>SSID</label><input type='text' name='ssid' class='cfg' placeholder='NombreRed'>";
  html += "<div style='margin:8px 0'><button type='button' class='btn btn-gray' style='width:100%;margin-bottom:6px' onclick='escanear()'>🔍 ESCANEAR</button>";
  html += "<div id='scanResult' style='font-size:0.75rem;color:#64748b'></div></div>";
  html += "<label>Contraseña</label><input type='password' name='pass' class='cfg' placeholder='Contraseña'>";
  html += "<br><br><button type='submit' class='btn btn-green' style='width:100%'>💾 GUARDAR RED</button></form></div>";

  html += "<script>";
  html += "function escanear(){document.getElementById('scanResult').innerHTML='🔍 Escaneando...';";
  html += "fetch('/scanWiFi').then(r=>r.json()).then(data=>{let h='<div style=\"margin-top:6px\">';";
  html += "data.redes.forEach(r=>{let bars=r.rssi>-50?4:r.rssi>-65?3:r.rssi>-75?2:r.rssi>-85?1:0;";
  html += "let seg=r.secure?'🔒':'🔓';";
  html += "h+='<div onclick=\"selRed(\\\"'+r.ssid+'\\\")\" style=\"cursor:pointer;padding:6px 10px;margin:3px 0;background:#1e293b;border-radius:8px;display:flex;justify-content:space-between;align-items:center\">';";
  html += "h+='<span>'+seg+' '+r.ssid+'</span><span style=\"color:#64748b;font-size:0.7rem\">'+r.rssi+' dBm</span></div>';});";
  html += "h+='</div>';document.getElementById('scanResult').innerHTML=h;";
  html += "}).catch(()=>{document.getElementById('scanResult').innerHTML='❌ Error';});}";
  html += "function selRed(s){document.querySelector('input[name=ssid]').value=s;}";
  html += "</script></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleGuardarWiFi() {
  if (!checkAuth()) return;
  if (server.hasArg("ssid") && server.hasArg("pass") && server.hasArg("slot")) {
    int slot = server.arg("slot").toInt();
    if (slot>=0 && slot<WIFI_MAX_SLOTS) {
      wifiSSIDs[slot]  = server.arg("ssid");
      wifiPasses[slot] = server.arg("pass");
      prefs.begin(NVS_NAMESPACE, false);
      prefs.putString(("wssid"+String(slot)).c_str(), wifiSSIDs[slot]);
      prefs.putString(("wpass"+String(slot)).c_str(), wifiPasses[slot]);
      prefs.end();
      añadirLog("WIFI","Red guardada slot "+String(slot+1)+": "+wifiSSIDs[slot]);
    }
  }
  server.sendHeader("Location","/configWiFi"); server.send(303);
}

void handleBorrarWiFi() {
  if (!checkAuth()) return;
  int slot = server.arg("slot").toInt();
  if (slot>=0 && slot<WIFI_MAX_SLOTS) {
    wifiSSIDs[slot]=""; wifiPasses[slot]="";
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(("wssid"+String(slot)).c_str(),"");
    prefs.putString(("wpass"+String(slot)).c_str(),"");
    prefs.end();
    añadirLog("WIFI","Red slot "+String(slot+1)+" borrada");
  }
  server.sendHeader("Location","/configWiFi"); server.send(303);
}

void handleSetPreferidaWiFi() {
  if (!checkAuth()) return;
  int slot = server.arg("slot").toInt();
  if (slot>=0 && slot<WIFI_MAX_SLOTS) {
    wifiPref = slot;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putInt("wifiPref", slot);
    prefs.end();
    añadirLog("WIFI","Preferida slot "+String(slot+1)+" — "+wifiSSIDs[slot]);
  }
  server.sendHeader("Location","/configWiFi"); server.send(303);
}

void handleScanWiFi() {
  if (!checkAuth()) return;
  int n = WiFi.scanNetworks();
  String json = "{\"redes\":[";
  for (int i=0;i<n&&i<20;i++) {
    if(i>0) json+=",";
    json+="{\"ssid\":\""+WiFi.SSID(i)+"\",\"rssi\":"+String(WiFi.RSSI(i))+",\"secure\":"+String(WiFi.encryptionType(i)!=WIFI_AUTH_OPEN?1:0)+"}";
  }
  json+="]}";
  WiFi.scanDelete();
  server.send(200,"application/json",json);
}

void handleAyuda() {
  if (!checkAuth()) return;
  String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>📖 Ayuda Watchdog</title>" + getCSS() + "</head><body>";
  html += "<h1>📖 AYUDA — WATCHDOG ESP32</h1>";
  html += "<div class='nav'><a href='/'><button class='btn btn-gray'>🏠 VOLVER</button></a></div>";

  html += "<div class='card'><h2>🐕 ¿Qué es?</h2>";
  html += "<p style='color:#94a3b8;font-size:0.85rem;line-height:1.8'>Vigila dispositivos de red (cámaras, tableros...). Si no responden, corta y repone su alimentación enviando señales por <b>ESP-NOW broadcast</b> y/o <b>RF 433MHz</b> a los enchufes inteligentes.</p></div>";

  html += "<div class='card'><h2>📋 Campos obligatorios</h2>";
  html += "<table style='min-width:300px;font-size:0.8rem'><thead><tr><th>Campo</th><th>¿Obligatorio?</th><th>Descripción</th></tr></thead><tbody>";
  html += "<tr><td style='color:#38bdf8'>IP</td><td style='color:#22c55e'>✅ SÍ</td><td>IP del dispositivo vigilado</td></tr>";
  html += "<tr><td style='color:#38bdf8'>Activo ☑</td><td style='color:#22c55e'>✅ SÍ</td><td>Debe estar marcado para vigilar</td></tr>";
  html += "<tr><td style='color:#38bdf8'>Botón</td><td style='color:#64748b'>Solo ESP-NOW</td><td>Nº relé (1-18) del enchufe</td></tr>";
  html += "<tr><td style='color:#38bdf8'>St.N</td><td style='color:#64748b'>Solo ESP-NOW</td><td>Estado normal del relé (ON/OFF cuando alimenta)</td></tr>";
  html += "<tr><td style='color:#38bdf8'>ESP ☑ / RF ☑</td><td style='color:#64748b'>Al menos uno</td><td>Método de actuación</td></tr>";
  html += "<tr><td style='color:#38bdf8'>Fallos</td><td style='color:#64748b'>Default 3</td><td>Pings fallidos antes de actuar</td></tr>";
  html += "<tr><td style='color:#38bdf8'>T.Ap</td><td style='color:#64748b'>Default 5s</td><td>Segundos sin corriente</td></tr>";
  html += "<tr><td style='color:#38bdf8'>T.Gr</td><td style='color:#64748b'>Default 20s</td><td>Segundos de gracia tras reinicio</td></tr>";
  html += "</tbody></table></div>";

  html += "<div class='card'><h2>🔄 Flujo de reinicio</h2>";
  html += "<div style='font-size:0.82rem;color:#94a3b8;line-height:2'>";
  html += "1️⃣ Ping HTTP a <code>/status</code> cada N segundos<br>";
  html += "2️⃣ Fallo → suma contador<br>";
  html += "3️⃣ Contador = máximo → envía <b>APAGADO</b> (estado contrario al normal)<br>";
  html += "4️⃣ Espera <b>T.Apagado</b> segundos<br>";
  html += "5️⃣ Envía <b>ENCENDIDO</b> (estado normal)<br>";
  html += "6️⃣ Período de gracia <b>T.Gracia</b> segundos<br>";
  html += "7️⃣ Reanuda vigilancia normal";
  html += "</div></div>";

  html += "<div class='card'><h2>📶 WiFi Multi-red</h2>";
  html += "<p style='color:#94a3b8;font-size:0.82rem;line-height:1.8'>Hasta <b>5 redes</b>. Conecta a la preferida ⭐ primero. Auto-switch si señal baja de -75 dBm.</p></div>";

  html += "<div class='card'><h2>💾 Backup NVS</h2>";
  html += "<p style='color:#94a3b8;font-size:0.82rem;line-height:1.8'>En <b>⚙️ CONFIG → BACKUP</b>: exportar/importar <code>watchdog_config.json</code> para clonar configuración entre placas.</p></div>";

  html += "<div class='card'><h2>🔧 Hardware</h2>";
  html += "<p style='color:#94a3b8;font-size:0.82rem;line-height:2'>GPIO 4 → FS1000A RF 433MHz<br>GPIO 2 → LED estado<br>Core 0 → servidor web<br>Core 1 → ciclo ping HTTP<br>ESP-NOW → broadcast FF:FF:FF:FF:FF:FF</p></div>";
  html += "</body></html>";
  server.send(200,"text/html; charset=utf-8",html);
}

// ============================================================
// INICIAR SERVIDOR WEB
// ============================================================
void iniciarServidorWeb() {
  server.on("/",                    HTTP_GET,  handleRaiz);
  server.on("/guardarDispositivos", HTTP_POST, handleGuardarDispositivos);
  server.on("/config",              HTTP_GET,  handleConfiguracion);
  server.on("/guardarConfig",       HTTP_POST, handleGuardarConfig);
  server.on("/log",                 HTTP_GET,  handleLog);
  server.on("/borrarLog",           HTTP_GET,  handleBorrarLog);
  server.on("/status",              HTTP_GET,  handleStatus);
  server.on("/estado",              HTTP_GET,  handleEstado);
  server.on("/testRF",              HTTP_POST, handleTestRF);
  server.on("/testESP",             HTTP_POST, handleTestESP);
  server.on("/exportNVS",           HTTP_GET,  handleExportNVS);
  server.on("/importNVS",           HTTP_POST,
    []() {
      // ✅ Handler de respuesta final — aquí sí va la autenticación
      if (!server.authenticate("admin", cfgWebPass.c_str())) {
        server.requestAuthentication();
      }
    },
    handleImportNVS
  );
  server.on("/configWiFi",          HTTP_GET,  handleConfigWiFi);
  server.on("/guardarWiFi",         HTTP_POST, handleGuardarWiFi);
  server.on("/borrarWiFi",          HTTP_GET,  handleBorrarWiFi);
  server.on("/scanWiFi",            HTTP_GET,  handleScanWiFi);
  server.on("/setPreferida",        HTTP_POST, handleSetPreferidaWiFi);
  server.on("/ayuda",               HTTP_GET,  handleAyuda);
  server.onNotFound([](){
    server.send(404, "text/plain", "No encontrado");
  });
  server.begin();
}

// ============================================================
// PROCESAR WEB
// ============================================================
void procesarWeb() {
  server.handleClient();
}
