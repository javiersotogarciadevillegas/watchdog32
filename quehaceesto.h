 ¿Qué hace el Watchdog ESP32?
Es un vigilante de red autónomo. Su única misión es asegurarse de que todos tus dispositivos están vivos, y si no lo están, reiniciarlos automáticamente.
El ciclo de vida es este:
Cada X segundos (configurable, por defecto 30) hace una petición HTTP a /status de cada dispositivo de la lista. Si responde, perfecto — resetea el contador. Si no responde, suma un fallo. Cuando llega a N fallos consecutivos (configurable, por defecto 3), actúa: envía por ESP-NOW y/o RF la señal de apagado del relé que alimenta ese dispositivo. Espera los segundos de apagado configurados, luego envía la señal de encendido. Después entra en período de gracia — no vuelve a vigilar ese dispositivo hasta que haya tenido tiempo de arrancar completamente.
Todo esto se configura desde su página web:

Qué dispositivos vigilar, con su IP, MAC, códigos ESP-NOW y RF
Cuántos fallos antes de actuar, cuánto tiempo de apagado, cuánto de gracia
El intervalo global de ping
Su propia IP fija, WiFi y contraseña de acceso

Y tiene un log de todos los eventos — cuándo reinició cada dispositivo, cuándo se recuperó, cuándo arrancó el sistema.
En resumen: lo enchufas, lo configuras una vez desde el navegador, y se olvida de él. Trabaja solo. 🔧