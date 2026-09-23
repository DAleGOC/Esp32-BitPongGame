# 🏓 PinPong Arcade V4 (ESP32)

Un clon avanzado y altamente optimizado del clásico arcade, diseñado para el microcontrolador **ESP32** y pantallas OLED **SH1106**. Este proyecto va mucho más allá de un simple juego retro: implementa físicas independientes del framerate, inteligencia artificial predictiva, motor de audio no bloqueante y persistencia de datos en memoria Flash.

## ✨ Características Principales

* **Físicas Avanzadas (Delta-Time):** El motor de juego utiliza escalado temporal (`physicsScale`) para asegurar que la velocidad del juego sea siempre consistente, independientemente de la carga del procesador.
* **Inteligencia Artificial Competitiva:** La CPU cuenta con 3 niveles de dificultad. Calcula trayectorias futuras, rebotes en las paredes y tiene factores de error humanos simulados.
* **Audio No Bloqueante:** Sistema de sonido impulsado por temporizadores (`millis()`) que permite reproducir música y efectos de sonido simultáneamente sin usar la función `delay()`, evitando tirones en el juego.
* **Persistencia NVS:** Tus configuraciones (brillo, dificultad, inversión de controles) y récords se guardan automáticamente en la memoria no volátil del ESP32 usando la librería `Preferences`.
* **Multibola y Portales:** Estructuras de datos dinámicas que permiten tener múltiples pelotas en pantalla y zonas de teletransporte.
* **Gestión de Energía:** Implementación de modo *Deep Sleep* que apaga los periféricos y reduce el consumo a microamperios cuando no está en uso, despertando con presionar un botón.

## 🛠️ Hardware Requerido

* **Placa:** ESP32 (cualquier variante estándar de 30 o 38 pines).
* **Pantalla:** Módulo OLED 1.3" I2C (Chipset **SH1106**).
* **Controles:** 
  * 2x Joysticks analógicos (o potenciómetros lineales para el eje Y).
  * 2x Pulsadores táctiles (con resistencias pull-up internas).
* **Audio:** 1x Zumbador pasivo (Piezo buzzer).

### 🔌 Pinout (Conexiones)

| Componente | Pin ESP32 | Notas |
| :--- | :--- | :--- |
| **OLED SDA** | `GPIO 21` | Bus de datos I2C |
| **OLED SCL** | `GPIO 22` | Bus de reloj I2C |
| **Zumbador** | `GPIO 5` | Salida PWM para audio |
| **P1 - Eje Y** | `GPIO 34` | Entrada Analógica (ADC1) |
| **P1 - Botón** | `GPIO 25` | `INPUT_PULLUP` (Pausa/Select) |
| **P2 - Eje Y** | `GPIO 35` | Entrada Analógica (ADC1) |
| **P2 - Botón** | `GPIO 26` | `INPUT_PULLUP` |

## 📚 Librerías Dependientes

Asegúrate de instalar las siguientes librerías desde el Gestor de Librerías de tu Arduino IDE:

1. **Adafruit GFX Library:** Motor base de gráficos y geometría.
2. **Adafruit SH1106:** Driver específico para la pantalla (ojo: *no* usar el driver SSD1306 a menos que cambies de pantalla).
3. `Preferences.h` y `Wire.h` (Vienen preinstaladas en el núcleo de ESP32 para Arduino).

## 🚀 Instalación y Uso

1. Clona este repositorio:
   ```bash
   git clone [https://github.com/DAleGOC/Esp32-BitPongGame.git)
