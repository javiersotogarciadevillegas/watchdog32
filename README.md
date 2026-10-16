# 🐕 Watchdog ESP32

Vigilante de red autónomo. Su misión es comprobar que tus dispositivos (cámaras, pantallas, tableros...) siguen vivos y, si no lo están, reiniciarlos cortando y devolviendo la corriente al enchufe que los alimenta.

## Cómo funciona

1. Cada X segundos (30 por defecto) hace una petición HTTP a `http://IP/status` de cada dispositivo de la lista. Cualquier respuesta cuenta como "vivo".
2. Si no responde, suma un fallo. Al llegar a N fallos seguidos (3 por defecto) **actúa**: envía por **ESP-NOW** (broadcast) y/o **RF 433 MHz** la orden de apagar el relé.
3. Espera los segundos de apagado configurados (**T.Ap**) y envía la orden de encender.
4. Entra en **período de gracia** (**T.Gr**): no vigila ese dispositivo hasta que haya tenido tiempo de arrancar.
5. Si tras **3 reinicios seguidos** el dispositivo sigue sin responder, espera **30 minutos** antes de volver a intentarlo (para no estar cortando la corriente sin parar).

Todo se configura desde su página web (usuario `admin`). Hay un log con hora real (NTP) de los reinicios y recuperaciones.

## Primera instalación

1. Copia `secrets.h.example` como **`secrets.h`** y pon tu WiFi y la contraseña de la web.
   `secrets.h` está en `.gitignore`: **nunca se sube a GitHub**.
2. Librería necesaria: **rc-switch** (Gestor de librerías del IDE de Arduino).
3. Placa: **ESP32 Dev Module**, core **esp32 3.3.x**.
4. Compila y sube por USB. Entra en `http://192.168.1.250` (o la IP que hayas puesto).

> Si el ESP32 ya tenía redes guardadas en su memoria (NVS), primero prueba esas. Si ninguna funciona, prueba también la red de `secrets.h` y, si conecta, la guarda con la contraseña nueva.

## Si no conecta a ninguna red: MODO RESCATE 🆘

Si cambias la contraseña del router, las redes guardadas en el ESP32 dejan de funcionar. En ese caso el Watchdog:

1. Escribe en el monitor serie **por qué** falla cada red (por ejemplo: `CONTRASEÑA INCORRECTA` o `no se encuentra esa red`).
2. Crea su propia WiFi: **`Watchdog32-Config`**, con la clave **`watchdog32`**.
3. Conéctate a ella con el móvil y entra en **http://192.168.4.1** → **📶 WIFI**.
4. Vuelve a guardar tu red, en el mismo slot y con la contraseña nueva. En unos segundos lo vuelve a intentar.

Cuando consigue conectar al router, apaga su WiFi de rescate.

## Actualizar por WiFi (OTA)

Con el Watchdog encendido y en la misma red, en el IDE de Arduino:
**Herramientas → Puerto → `watchdog32 at 192.168.1.250`**. La contraseña es la misma que la de la web.

## Hardware

| Pin | Uso |
|---|---|
| GPIO 4 | Transmisor RF 433 MHz FS1000A |
| GPIO 2 | LED de estado |

- **Core 0**: servidor web y WiFi.
- **Core 1**: ciclo de ping y temporizadores de apagado/gracia.
- **Watchdog hardware**: si el programa se cuelga más de 2 minutos, el ESP32 se reinicia solo.

## Backup

En **⚙️ CONFIG → BACKUP** puedes descargar `watchdog_config.json` con toda la configuración (dispositivos, red y las redes WiFi guardadas) y volver a cargarlo, por ejemplo para clonar la configuración en otra placa.

⚠️ **Ese fichero contiene tus contraseñas.** No lo subas a GitHub (el `.gitignore` ya lo evita).

## Ficheros

| Fichero | Qué contiene |
|---|---|
| `watchdog32.ino` | `setup()`, `loop()`, tarea de Core 1, ESP-NOW, OTA |
| `WatchdogWeb.h` | Configuración, estructuras y prototipos |
| `WatchdogWeb.cpp` | Páginas web, backup e importación, WiFi multi-red |
| `WatchdogPing.cpp` | Ping HTTP, decisión de reiniciar, log |
| `WatchdogNVS.cpp` | Guardar/cargar la configuración en la memoria flash (NVS) |
| `watchdog_documentacion.html` | Documentación detallada |

## Avisos

- **ESP-NOW usa el mismo canal que el WiFi.** Si el Watchdog cambia a un router que está en otro canal, los enchufes pueden dejar de recibir las órdenes. Lo más fiable es que todos los routers guardados usen el mismo canal.
- Mientras escanea redes WiFi (página 📶 WIFI) la web tarda un par de segundos en responder.
