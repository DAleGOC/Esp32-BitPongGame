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




# PINPONG ARCADE — README técnico y guía completa de funcionamiento

**Plataforma:** ESP32 · **Pantalla:** OLED SH1106 128 × 64 por I²C · **Entradas:** dos joysticks con pulsador · **Audio:** zumbador pasivo (el encabezado del programa indica transistor 2N2222).  
**Fuente examinada:** `Texto pegado.txt`, versión comentada de `PinPong_Arcade.ino` (aproximadamente 2662 líneas). Esta documentación describe **lo que hace esa versión del código**, no mejoras que todavía no existan. Las ubicaciones de referencia son líneas aproximadas del archivo suministrado. 

> **Lectura recomendada:** empieza por «Cómo jugar», continúa por «Qué significa A 2.1 en el HUD», y después revisa la sección de arquitectura y el catálogo de funciones. Los números 2.0, 2.1, etc. son **unidades internas de movimiento del juego**, no km/h ni una medición de velocidad real.

## Índice

1. [Descripción general](#1-descripción-general)
2. [Hardware, conexiones y bibliotecas](#2-hardware-conexiones-y-bibliotecas)
3. [Cómo jugar y menús](#3-cómo-jugar-y-menús)
4. [Arquitectura y estados](#4-arquitectura-y-estados)
5. [Arranque, bucle principal y temporización](#5-arranque-bucle-principal-y-temporización)
6. [Controles: calibración, filtrado y movimiento](#6-controles-calibración-filtrado-y-movimiento)
7. [Física: pelota, rebotes, efecto y calidad de golpe](#7-física-pelota-rebotes-efecto-y-calidad-de-golpe)
8. [Velocidad progresiva y número A 2.1 / C 2.1 del HUD](#8-velocidad-progresiva-y-número-a-21--c-21-del-hud)
9. [Modo CLÁSICO frente a modo ARCADE](#9-modo-clásico-frente-a-modo-arcade)
10. [Los cinco poderes (ítems): aparición, obtención y duración](#10-los-cinco-poderes-ítems-aparición-obtención-y-duración)
11. [Portales: aparición y efecto](#11-portales-aparición-y-efecto)
12. [Multibola: cómo se activa y cómo se puntúa](#12-multibola-cómo-se-activa-y-cómo-se-puntúa)
13. [CPU / IA y niveles de dificultad](#13-cpu--ia-y-niveles-de-dificultad)
14. [Marcador, saque, pausa y fin de partida](#14-marcador-saque-pausa-y-fin-de-partida)
15. [Qué muestra la pantalla y cómo funciona el HUD](#15-qué-muestra-la-pantalla-y-cómo-funciona-el-hud)
16. [Animaciones, audio y efectos visuales](#16-animaciones-audio-y-efectos-visuales)
17. [Ajustes y almacenamiento permanente](#17-ajustes-y-almacenamiento-permanente)
18. [Variables y constantes importantes](#18-variables-y-constantes-importantes)
19. [Catálogo de todas las funciones](#19-catálogo-de-todas-las-funciones)
20. [Ejemplo narrado de una partida ARCADE](#20-ejemplo-narrado-de-una-partida-arcade)
21. [Qué modificar si deseas personalizar el juego](#21-qué-modificar-si-deseas-personalizar-el-juego)
22. [Advertencias y puntos a tener presentes](#22-advertencias-y-puntos-a-tener-presentes)

---

## 1. Descripción general

PINPONG es un juego tipo *Pong* integrado en un ESP32: hay dos paletas verticales, una o dos pelotas y una cancha en una OLED monocromática. Se puede jugar una persona contra la CPU o dos personas, en estilo CLÁSICO o ARCADE. Cada vez que una paleta devuelve la pelota, avanza el contador de golpes consecutivos de ese intercambio (*rally*); ese contador alimenta una velocidad base que crece progresivamente hasta un máximo. Los contactos con distinta zona y movimiento de la paleta pueden producir golpes normales, angulados y perfectos.

En **ARCADE** se activan adicionalmente cajas de poder, un portal y la posibilidad de una segunda pelota. Los elementos especiales funcionan mediante temporizadores basados en `millis()`. El juego también incluye presentación de inicio, efectos musicales, menú de ajustes, estadísticas persistentes, demostración por inactividad y modo de sueño del ESP32.

**Distinción importante:** CLÁSICO y ARCADE comparten el motor de física progresiva, los tipos de golpe y el indicador de velocidad del HUD. ARCADE **añade** los elementos especiales; no es el único modo donde aumenta la velocidad.

## 2. Hardware, conexiones y bibliotecas

### 2.1 Pines declarados

| Elemento | Pin ESP32 / dato | Papel |
|---|---:|---|
| OLED SDA | GPIO 21 | Datos I²C |
| OLED SCL | GPIO 22 | Reloj I²C |
| OLED RESET | -1 | Sin pin de reset dedicado |
| OLED ADDRESS | `0x3C` | Dirección I²C configurada |
| OLED | 128 × 64 | Resolución que emplea el dibujo |
| Zumbador | GPIO 5 | Salida de tono para música y efectos |
| Joystick 1, eje Y | GPIO 34 | Mover paleta 1 y desplazarse por menús |
| Joystick 1, eje X | GPIO 32 | Cambiar opciones de submenús |
| Joystick 1, botón | GPIO 25 | Confirmar, pausar, despertar del sueño |
| Joystick 2, eje Y | GPIO 35 | Mover paleta 2 en modo dos jugadores |
| Joystick 2, botón | GPIO 26 | Se declara y configura, pero el flujo revisado no le asigna la misma navegación principal del botón 1 |

**No confundir pines de señal con alimentación.** Este archivo no constituye un plano eléctrico: consulta las especificaciones de los módulos para 3,3 V, GND y la conexión correcta del zumbador mediante transistor. No conectes una carga de corriente desconocida directamente al GPIO.

### 2.2 Bibliotecas

- `Arduino.h`: funciones básicas del entorno Arduino/ESP32.
- `Wire.h`: bus I²C para la pantalla.
- `Adafruit_GFX.h`: primitivas gráficas, texto y bitmaps.
- `Adafruit_SH1106.h`: controlador específico de la OLED usada en este programa.
- `Preferences.h`: lectura y escritura de configuración en la memoria no volátil (NVS) del ESP32.
- `Fonts/Picopixel.h`: recurso de tipografía incluido por el archivo.

La biblioteca SH1106 concreta tiene que proporcionar la interfaz utilizada por el código (`Adafruit_SH1106`, `begin`, `display`, etc.); otras bibliotecas SH1106 con el mismo propósito no siempre comparten la API.

## 3. Cómo jugar y menús

### 3.1 Menú principal

El menú muestra `PINPONG`, el récord `HI:`, iconos y cinco opciones: **JUGAR, CONFIGURACIÓN, PUNTAJES, CRÉDITOS y SALIR**. El joystick 1 en Y mueve la selección; para no saltar varias opciones, el código bloquea un segundo movimiento hasta recentrar el mando (`joystickLocked`). El botón del joystick 1 confirma. Las estrellas animadas decoran el fondo.

Tras **15 segundos de inactividad** en el menú, se activa `STATE_ATTRACT`: una demostración de Pong que no es una partida real. Mover el joystick o pulsar el botón permite regresar.

### 3.2 Configurar una nueva partida

Hay seis renglones de configuración:

| Opción | Selecciones / efecto |
|---|---|
| ESTILO | `CLASICO` o `ARCADE` |
| JUEGO | `1P IA` o `2P VS` |
| DIFICULTAD | `FACIL`, `NORMAL`, `DIFICIL` (la pantalla muestra `---` al jugar 2P) |
| PUNTOS | 3, 5 o 10 puntos para terminar |
| EMPEZAR JUEGO | Reinicia puntuaciones y elementos temporales e inicia cuenta regresiva |
| CANCELAR | Regresa al menú principal |

Se navega principalmente con eje Y; X cambia los parámetros de las primeras opciones. El botón también permite alternar o confirmar. **La dificultad solo determina el movimiento de la CPU cuando se elige 1P.**

### 3.3 Durante la partida

El joystick 1 controla la paleta izquierda. En `2P VS`, el joystick 2 controla la derecha. En `1P IA`, el motor `updateCpuPaddle()` mueve la derecha. Mantener presionado el **botón del joystick 1 más de 1,6 segundos** abre la pausa. La pausa permite continuar, entrar en configuración o salir. Al alcanzar el límite de puntos aparece la pantalla de ganador y las opciones nueva partida o menú principal.

## 4. Arquitectura y estados

La variable `currentState`, de tipo `GameState`, es la máquina de estados. La función `loop()` actualiza el audio y, mediante `switch`, llama al controlador adecuado. Esto separa menú, partida, pausa, presentación y demás vistas.

| Estado | Función principal | Responsabilidad |
|---|---|---|
| `STATE_SPLASH` | `handleSplashScreen` | Pantalla de presentación alternativa |
| `STATE_MENU` | `handleMenu` | Menú y acceso al resto de opciones |
| `STATE_MATCH_SETUP` | `handleMatchSetup` | Ajustar parámetros e iniciar partida |
| `STATE_PLAY` | `handlePlay` | Actualizar y dibujar juego, colisiones y especiales |
| `STATE_POINT_SCORED` | `handlePointScored` | Cuenta 3–2–1 y espera de saque |
| `STATE_PAUSE` | `handlePause` | Menú de pausa |
| `STATE_GAME_OVER` | `handleGameOver` | Resultado y próxima acción |
| `STATE_SETTINGS` | `handleSettings` | Audio, controles y contraste |
| `STATE_CREDITS` | `handleCredits` | Información de créditos |
| `STATE_SCORES` | `handleScores` | Estadísticas guardadas |
| `STATE_SLEEP` | `handleSleep` | Animación de apagado y sueño profundo |
| `STATE_ATTRACT` | `handleAttractMode` | Demostración automática |

La variable inicial `currentState = STATE_MENU` significa que, al terminar las animaciones del `setup()`, se entra al menú. El estado `STATE_SPLASH` existe de manera independiente en el programa, pero las animaciones visibles de arranque ya son invocadas explícitamente desde `setup()`.

## 5. Arranque, bucle principal y temporización

### `setup()`

1. Inicia el bus I²C en los GPIO 21/22 y configura la OLED en `0x3C`.
2. Limpia y actualiza su pantalla; prepara el zumbador y los botones con `INPUT_PULLUP`.
3. Configura la lectura analógica a 12 bits.
4. Recupera preferencias con `loadSettings()` y calibra los joysticks con `calibrateJoysticks()`.
5. Ejecuta secuencialmente las cuatro animaciones de presentación.
6. Guarda el tiempo inicial de actividad y programa los temporizadores de ARCADE.

### `loop()`

Se repite continuamente: primero `updateAudio()` y después el controlador del estado activo. Mientras una función de arranque o un menú introduce `delay()`, su propia animación o manejo puede bloquear temporalmente ese camino; en la partida, las mecánicas principales se organizan alrededor de marcas de tiempo de `millis()`.

### Ajuste temporal de movimiento

`FRAME_REFERENCE_MS = 16.6667` representa **60 fotogramas por segundo de referencia**. La función de partida calcula:

```cpp
physicsScale = constrain(elapsed / FRAME_REFERENCE_MS, 0.45f, 1.35f);
```

Si transcurre más tiempo entre actualizaciones, la escala corrige parte del desplazamiento de pelotas y paletas. El límite de `0.45` a `1.35` evita cambios extremos si un fotograma tarda demasiado. **Esto no significa que la OLED garantice exactamente 60 FPS**, solo que el código usa esa cadencia como referencia.

## 6. Controles: calibración, filtrado y movimiento

### Al encender

`calibrateJoysticks()` hace **80 lecturas** de los ejes Y/X del joystick 1 e Y del joystick 2, con una espera de 2 ms por muestra. Calcula el centro de reposo físico de cada eje. Si alguno queda fuera de **1200 a 2900**, emplea el centro nominal `2048` para no aceptar una palanca sostenida en un extremo como posición neutral.

### Durante el juego

`readFilteredAxis()` aplica un filtro exponencial para reducir ruido:

```cpp
filteredState += (raw - filteredState) * 0.42f;
```

Luego `normalizeJoystickReading()` traduce el valor filtrado a la escala común **0–4095**, conservando un centro lógico de `2048`. `readJoystickY`, `readJoystickX` y `readJoystick2Y` facilitan la lectura de cada mando; los ejes Y pueden invertirse por la preferencia global.

`updatePlayerPaddle()` descarta movimientos dentro de la **zona muerta de ±160** alrededor del centro. Fuera de esa zona normaliza la intensidad de la orden y aplica una curva de respuesta con componente lineal (20 %) y cúbica (80 %): un desplazamiento pequeño da control fino y uno grande da movimiento más rápido. Multiplica por la velocidad de paleta `2.5`, ganancia `1.70` y `physicsScale`; respeta los límites de la cancha y la inversión temporal por poder.

La **velocidad vertical medida de la paleta** (`paddle1Velocity`, `paddle2Velocity`) también influye en la dirección del rebote. Por tanto, no solo importa dónde está la paleta, sino también hacia dónde y con qué rapidez se desplaza al golpear.

## 7. Física: pelota, rebotes, efecto y calidad de golpe

### Datos de cada pelota

`struct Ball` contiene posición `x, y`, desplazamiento horizontal/vertical `dx, dy`, efecto `spin`, `lastHitBy` (último jugador que la golpeó) y `active`. El arreglo `balls[2]` admite dos pelotas como máximo. El tamaño visual `BALL_SIZE` es **2 × 2 píxeles**.

Al comenzar un saque, `resetMainBall()` coloca la pelota principal en **(64, 32)**, horizontal a **±2.0** según el saque y vertical a **±1.5** elegido al azar. Desactiva la segunda pelota y elimina el efecto inicial. La cancha empieza verticalmente en `COURT_TOP_Y = 13` y tiene suelo en `COURT_BOTTOM_Y = 63`.

### Física de un fotograma

En `handlePlay()`, para cada pelota activa:

```cpp
balls[i].dy += balls[i].spin * physicsScale;
balls[i].dy = constrain(balls[i].dy, -3.2, 3.2);
balls[i].x += balls[i].dx * physicsScale;
balls[i].y += balls[i].dy * physicsScale;
```

Primero actúa el efecto, se limita la componente vertical y luego se actualiza la posición. Si la pelota rebota en techo o suelo, cambia el signo apropiado de `dy`; el `spin` se invierte y reduce a la mitad. Se muestra un pequeño destello local durante dos fotogramas. **Chocar con una pared superior o inferior no aumenta `consecutiveHits`**: el contador se incrementa en los golpes de paleta.

### Ángulo del rebote

`calculateBounceY()` calcula qué tan lejos del centro de la paleta impactó la pelota. El desplazamiento relativo (entre -1 y 1) se multiplica por `MAX_VERTICAL_SPEED = 3.2`. Además suma `paddleVelocity * PADDLE_INFLUENCE`, con `PADDLE_INFLUENCE = 0.38`. Se vuelve a limitar el resultado al intervalo -3.2 a 3.2. Golpear arriba y abajo produce trayectorias distintas; mover la paleta durante el golpe también modifica el resultado.

Si el eje del joystick está claramente dirigido arriba o abajo al impactar, puede asignarse `spin` de **-0.025 o +0.025**. En CPU difícil, el código también puede generar un pequeño efecto aleatorio. Un golpe perfecto añade un efecto extra con límite global de **±0.05**. El efecto va curvando la trayectoria vertical en actualizaciones sucesivas.

### Golpes NORMAL, ANGULADO y PERFECTO

`classifyPaddleHit()` estudia la distancia del punto de contacto al centro y la velocidad de la paleta. La variable `zone` va de 0 (centro) a 1 (borde):

| Tipo | Condición en el código | Velocidad horizontal del impacto | Componente vertical |
|---|---|---:|---:|
| `HIT_NORMAL` | Contacto que no cumple las condiciones siguientes | × 1.00 | × 1.00 |
| `HIT_ANGLED` | `zone >= 0.48`, salvo los que califican como perfecto | × 1.04 | × 1.08 |
| `HIT_PERFECT` | `0.55 ≤ zone ≤ 0.92`, paleta moviéndose en el mismo sentido que el lado de contacto y `abs(paddleVelocity) >= 0.55` | × 1.08 | × 1.15 |

El multiplicador horizontal se aplica **al devolver la pelota**, sobre la velocidad calculada del rally, y después se limita a **4.8**. El multiplicador vertical se aplica al componente del ángulo y luego se limita a **±3.2**. La diferencia entre ambas componentes es importante: un golpe perfecto no multiplica indiscriminadamente toda la física.

`registerHitFeedback()` presenta durante **220 ms** la etiqueta `NORM`, `ANG` o `PERF` en el HUD y emite un sonido adecuado. Con seis golpes consecutivos, o si el efecto absoluto de la pelota supera `0.05`, el código puede representar una estela de tres muestras; dado que los valores de `spin` se limitan normalmente a `0.05`, en la práctica la condición de seis golpes es una vía importante para activar la estela.

## 8. Velocidad progresiva y número A 2.1 / C 2.1 del HUD

**Esta es la cifra que aparece arriba, aproximadamente en el centro de la pantalla, y va creciendo a medida que las paletas devuelven la pelota.** El HUD dibuja la letra **`A`** para ARCADE o **`C`** para CLÁSICO y, a continuación, muestra `getCurrentBallSpeed()` con **un decimal**:

```cpp
oledMonitor.print(playStyle == 0 ? "C " : "A ");
oledMonitor.print(getCurrentBallSpeed(), 1);
```

La fórmula exacta de la velocidad **objetivo del rally** es:

```cpp
float speed = BALL_BASE_SPEED + (consecutiveHits * BALL_SPEED_PER_HIT);
return constrain(speed, BALL_BASE_SPEED, BALL_MAX_SPEED);
```

Con los valores de este archivo:

- `BALL_BASE_SPEED = 2.0`: velocidad horizontal objetivo al comenzar el intercambio.
- `BALL_SPEED_PER_HIT = 0.09`: aumento **por cada devolución de paleta**.
- `BALL_MAX_SPEED = 4.8`: techo de la velocidad objetivo, antes de considerar multiplicadores de golpe; el movimiento horizontal de las pelotas también se limita a este valor después de los golpes.
- `consecutiveHits`: número acumulado de impactos consecutivos de paleta en el intercambio.

**Fórmula matemática:**

```text
velocidad_objetivo = mínimo(2.0 + 0.09 × golpes_consecutivos, 4.8)
```

**Ejemplo sin poderes:**

| Golpes consecutivos de paleta | Valor interno objetivo | Visualización aproximada en ARCADE |
|---:|---:|---|
| 0 | 2.00 | `A 2.0` |
| 1 | 2.09 | `A 2.1` |
| 2 | 2.18 | `A 2.2` |
| 3 | 2.27 | `A 2.3` |
| 4 | 2.36 | `A 2.4` |
| 5 | 2.45 | `A 2.5` (por formato a un decimal) |
| 6 | 2.54 | `A 2.5` |
| 10 | 2.90 | `A 2.9` |
| 12 | 3.08 | `A 3.1` |
| 20 | 3.80 | `A 3.8` |
| 30 | 4.70 | `A 4.7` |
| 32 | límite 4.80 | `A 4.8` |

La representación exacta del último decimal depende del redondeo de `print(..., 1)` y de números en punto flotante. Por eso **puede repetirse un número del HUD en dos golpes sucesivos** aunque internamente sí aumente `0.09`.

### ¿Por qué puede mostrar A 2.1 después de un solo rebote?

Parte de `2.00`. La **primera devolución de una paleta** eleva `consecutiveHits` de 0 a 1; la nueva velocidad objetivo es `2.00 + 1 × 0.09 = 2.09`. Dibujada con un decimal, se ve como **`A 2.1`**. Otro rebote contra techo/suelo no eleva ese contador. La segunda devolución de paleta da `2.18`, que aparece como `A 2.2`.

### ¿La cifra es la velocidad exacta de la pelota que estás viendo?

**No siempre.** Es el valor **base objetivo** que calcula el rally; no lee `balls[0].dx` en tiempo real ni calcula una magnitud física total a partir de `dx` y `dy`. Una devolución ANGULADA usa ×1.04 y una PERFECTA ×1.08, y el portal puede alterar `dx` ×1.20; dichos efectos pueden hacer que el movimiento horizontal real difiera temporalmente de la cifra del HUD. Además, el HUD muestra un objetivo en escala lógica interna, no una velocidad en unidades físicas, y `physicsScale` adapta el desplazamiento al tiempo de fotograma.

### ¿Qué pasa al llegar al límite?

El objetivo queda limitado a **4.8**. Con 31 golpes la fórmula da `4.79` y el formato de un decimal puede ya mostrar `4.8`; con 32 golpes daría `4.88`, pero `constrain` la deja en `4.8`. Los golpes de calidad y el portal también tienen límites explícitos para la velocidad horizontal. Al finalizar el intercambio normal, `consecutiveHits` vuelve a 0 y el siguiente saque comienza a `2.0`.

### ¿Por qué a veces no se ve A 2.1?

La zona central superior es compartida: si hay un poder activo en ARCADE, en vez de velocidad se escribe `GIGANTE`, `ENCOGER`, `FUEGO`, `FANTASMA` o `INVERTIR`. Si acaba de ocurrir un impacto, durante 220 ms aparece `NORM`, `ANG` o `PERF`; cuando termina ese aviso y no hay poder activo reaparece `A X.X` o `C X.X`.

## 9. Modo CLÁSICO frente a modo ARCADE

| Aspecto | CLÁSICO (`playStyle == 0`) | ARCADE (`playStyle == 1`) |
|---|---|---|
| Uno o dos jugadores | Sí | Sí |
| IA según dificultad | Sí, en 1P | Sí, en 1P |
| Marcador y límite a 3, 5 o 10 | Sí | Sí |
| Aumento +0.09 por devolución | Sí | Sí |
| Golpes NORMAL / ANG / PERF | Sí | Sí |
| Curvatura / efecto y estela | Sí | Sí |
| Cinco poderes / ítems | No | Sí |
| Portal | No | Sí |
| Oportunidad de segunda pelota | No | Sí |
| Prefijo de velocidad en HUD | `C` | `A` |

Los elementos ARCADE son **adiciones a una física de base compartida**. Cambiar entre estilos no significa que haya que subir la velocidad con otra fórmula: la función `getCurrentBallSpeed()` es la misma.

## 10. Los cinco poderes (ítems): aparición, obtención y duración

### 10.1 Cuándo y dónde sale una caja

En ARCADE, `scheduleArcadeTimers()` programa la primera aparición con un intervalo aleatorio de **10 a 15 segundos**. `handlePlay()` solo crea una nueva caja si **no hay ninguna caja en pantalla y tampoco hay un poder activo**. La ubica aleatoriamente en la región central aproximada (`x` entre 30 y 89; `y` dentro de la cancha) y elige un tipo con `random(0, 5)`. Se dibuja como **cuadrado de 6 × 6 píxeles con un punto**, intermitente cada 150 ms.

### 10.2 Cómo se recoge y quién lo recibe

La caja se activa al **colisionar con una pelota** (principal o secundaria). No se obtiene desplazando la paleta hasta ella. El juego consulta `balls[b].lastHitBy`: **si la pelota todavía no ha sido golpeada por nadie (`0`), la colisión no concede el poder**; si fue golpeada por P1 o P2, se asigna al último jugador que la tocó. El poder entra en vigor de inmediato, deja de dibujarse la caja y se programa la próxima aparición entre **10 y 15 segundos** (una vez liberado el estado de poder).

### 10.3 Tabla detallada de ítems

| ID | Nombre mostrado | Quién se beneficia / quién resulta afectado | Acción real |
|---:|---|---|---|
| 0 | `GIGANTE` | Jugador que recogió el poder | Su propia paleta duplica altura: `PADDLE_H * 2`, de **14 a 28 píxeles**. |
| 1 | `ENCOGER` | Rival de quien lo recogió | Reduce a la mitad la paleta **del rival**: `PADDLE_H / 2`, de **14 a 7 píxeles**. |
| 2 | `FUEGO` | Pelota que recogió el objeto y jugador que la golpeó | Cambia **inmediatamente** la componente horizontal `dx` de **esa pelota** a `+4.8` si recogió P1 o `-4.8` si recogió P2, enviándola hacia el rival. No vuelve a imponer 4.8 continuamente en fotogramas posteriores. |
| 3 | `FANTASMA` | Efecto visual sobre la pelota, para dificultar el seguimiento | Activa `ghostBallActive`: el renderizado de pelotas y estela se alterna con una comprobación cada **200 ms**. **No** atraviesa paletas ni desactiva colisiones. |
| 4 | `INVERTIR` | Rival de quien lo recogió | Activa la inversión de controles de la **paleta del rival**. Si el rival es CPU, el código también invierte la dirección de su desplazamiento automático. |

**Duración:** para los ítems se registra `powerUpDurationTimer` y, transcurridos **más de 5000 ms**, se restablecen alturas normales y controles, se desactiva fantasma y se borra la etiqueta de poder. El HUD muestra el nombre mientras el estado está activo.

**Detalle especial de FUEGO:** su modificación de `dx` sucede en el instante de recogerlo, pero el juego conserva `activePowerUpType == 2` y la etiqueta `FUEGO` hasta que vence el temporizador de cinco segundos. La etiqueta no garantiza que `dx` permanezca forzado durante esos cinco segundos: otros golpes o el portal pueden volver a cambiarlo.

**Detalle de FANTASMA:** altera el dibujo, no la física. Que la pelota sea invisible en un instante de parpadeo no significa que deje de poder chocar con paredes, paletas, cajas o portales.

### 10.4 Fin de poderes cuando hay punto

Al terminar el intercambio normal, se restablecen ambas alturas, se quitan inversiones y fantasma, se eliminan cajas/poder activo y se reprograman los tiempos. El código también restablece estas condiciones al comenzar una partida nueva. En la lógica de multibola, el primer tanto mientras sigue viva otra pelota **no fuerza todavía el reinicio completo del rally**; consulta la sección correspondiente.

## 11. Portales: aparición y efecto

El portal es un objeto **independiente** de las cajas de poder: puede aparecer aunque no se obtenga una caja. Solo funciona en ARCADE. Se programa con un tiempo aleatorio inicial de **8 a 15 segundos**; al aparecer mide **6 × 16 píxeles**, se posiciona cerca del centro (`x` entre 52 y 71) y parpadea visualmente cada 100 ms.

Si transcurren **más de 6 segundos** sin que ninguna pelota lo toque, el portal desaparece y se programa el siguiente. Si una pelota atraviesa su zona:

1. Su velocidad horizontal `dx` se multiplica por **1.20**, conservando el signo y limitándose a **±4.8**.
2. Su coordenada vertical `y` se reasigna aleatoriamente dentro de la cancha: parece un desplazamiento/teletransporte vertical.
3. El portal se desactiva, programa otra aparición entre 8 y 15 segundos y reproduce un sonido agudo.

**No es un túnel entre dos portales:** en esta versión hay un único `activePortal` que cambia la trayectoria vertical de la pelota que lo toca. El aumento de velocidad del portal no suma golpes al contador `consecutiveHits`, por lo que la cifra `A X.X` puede no reflejar ese aumento instantáneo.

## 12. Multibola: cómo se activa y cómo se puntúa

El código admite `balls[0]` y `balls[1]` (máximo **dos pelotas**). La segunda pelota está reservada al modo ARCADE.

**Condiciones exactas del intento:** al sumar **12 golpes consecutivos de paleta**, `maybeTriggerMultiball()` realiza **una sola prueba aleatoria por rally**. Con `MULTIBALL_CHANCE = 35` y `random(0, 100) < 35`, la oportunidad programada es del **35 %**. No significa que haya 35 % en cada golpe después del duodécimo: el indicador `multiballAttemptedThisRally` impide repetir la prueba durante ese mismo intercambio.

Si tiene éxito, `spawnSecondaryBall()` crea una segunda pelota justo donde está la principal. La hace salir con `dx` horizontal opuesto y `dy` vertical opuesto, multiplicado por 0.9, sin efecto inicial. Ambas son procesadas por el mismo sistema de movimiento y colisiones y pueden recoger ítems o atravesar el portal.

**Puntuación con dos pelotas:** cuando una pelota sale de la cancha, se suma un punto al lado correspondiente incluso si la otra sigue activa. En ese caso se desactiva solamente la pelota que salió y continúa la restante, salvo que alguien haya alcanzado el límite de puntuación y termine el encuentro. Cuando sale la **última** pelota se restablece el rally, los poderes y el portal, se pone `consecutiveHits = 0` y empieza el nuevo saque. Así, en un mismo rally multibola pueden anotarse tantos independientes.

Como los dos proyectiles comparten el contador `consecutiveHits`, los golpes de cualquiera de ellos pueden seguir elevando la velocidad objetivo general mientras dura el intercambio.

## 13. CPU / IA y niveles de dificultad

La CPU sustituye a la segunda persona en `gameMode == 0`. Su controlador, `updateCpuPaddle()`, **no conoce ni corrige su objetivo en cada fotograma**: renueva el objetivo tras cierto número de milisegundos. Busca entre las pelotas activas que avanzan hacia la derecha (`dx > 0`) y elige la próxima a llegar.

| Parámetro | FÁCIL (`0`) | NORMAL (`1`) | DIFÍCIL (`2`) |
|---|---:|---:|---:|
| Tiempo entre decisiones | 180 ms | 100 ms | 55 ms |
| Factor de predicción de trayectoria | 0.00 | 0.45 | 1.00 |
| Rango de error añadido | aprox. ±3.0 px | aprox. ±1.5 px | aprox. ±0.7 px |
| Multiplicador del avance de paleta | 0.48 | 0.72 | 0.90 |
| Zona de tolerancia al objetivo | 3.0 px | 1.8 px | 1.2 px |

En el modo difícil, cuando la CPU devuelve la pelota, también puede asignar un pequeño `spin` aleatorio entre aproximadamente `-0.025` y `+0.025`. La CPU predice la posición futura en función de la distancia horizontal restante, la velocidad y una reflexión aproximada en techo/suelo. El error aleatorio y los límites de desplazamiento mantienen diferencias entre los tres niveles.

**INVERTIR contra CPU:** el poder invierte el signo de su movimiento calculado; no modifica el objetivo ni elimina su lógica predictiva.

## 14. Marcador, saque, pausa y fin de partida

**Marcador:** P1 y P2 aparecen en la cabecera, respectivamente a la izquierda y la derecha. Si la pelota sale por la **derecha**, puntúa P1; si sale por la **izquierda**, puntúa P2. `scoreLimit` puede ser **3, 5 o 10**.

**Saque:** `beginServeSequence()` alterna `nextServeDirection` entre `+1` y `-1`, reinicia la pelota en el centro y cambia a `STATE_POINT_SCORED`. Ese estado enseña **3–2–1** con intervalos `SERVE_STEP_MS = 300 ms`, por lo que los tres pasos abarcan nominalmente **900 ms**. La pelota se mantiene quieta mientras la pantalla muestra la cuenta. Al finalizar, cambia a `STATE_PLAY` y el movimiento empieza sin una espera bloqueante de 3 segundos.

**Pausa:** mantener botón 1 más de 1600 ms durante la partida abre el menú. Se puede continuar, cambiar configuración o salir al menú. La lectura de movimiento de la partida deja de ejecutarse mientras el estado activo es `STATE_PAUSE`.

**Fin del encuentro:** `checkAndEnterGameOver()` se activa cuando P1 o P2 llega a `scoreLimit`; determina ganador, actualiza las estadísticas y el récord cuando corresponda, guarda y pasa a la pantalla de fin. Puede reiniciarse con una nueva partida o volver al menú.

## 15. Qué muestra la pantalla y cómo funciona el HUD

La OLED mide **128 × 64** píxeles. Durante una partida, las primeras filas forman la **cabecera HUD**; una línea horizontal en `y = 12` separa la zona informativa de la cancha, cuyo límite superior lógico es `y = 13`.

```text
┌────────────────────────────────────┐
│  P1      A 2.1 / PERF          P2   │ ← cabecera / HUD
├────────────────────────────────────┤
│                :                   │
│  █             :               █   │
│                :   ■               │
│                :                   │
│                :                   │
└────────────────────────────────────┘
```

*Esquema explicativo, no representación pixel-perfect de los 128 × 64 píxeles.*

El HUD de partida incluye:

- **Puntuación del jugador 1** desde `setCursor(20, 2)`.
- **Puntuación del jugador 2** desde `setCursor(102, 2)`.
- **Zona central variable:** nombre del poder activo (`GIGANTE`, `ENCOGER`, `FUEGO`, `FANTASMA` o `INVERTIR`), o aviso `NORM` / `ANG` / `PERF` durante los 220 ms posteriores a un golpe, o indicador normal `C X.X` / `A X.X`.

**Orden de prioridad del texto central:** poder activo → aviso de impacto reciente → velocidad progresiva. Los iconos de música/efectos de `headerHud()` corresponden a las cabeceras de los menús; **no debe confundirse esa función con el indicador central de velocidad dibujado directamente por `handlePlay()`**.

En cancha se ven las paletas, la división central punteada, pelota(s), los cuadros parpadeantes de ítems, el portal y la estela cuando corresponde. El juego emplea una pantalla fija: los golpes no desplazan toda la cancha ni provocan sacudidas de cámara.

## 16. Animaciones, audio y efectos visuales

**Presentación inicial:** `showStudioScreen()` muestra el nombre del estudio letra por letra y sonidos, `showBouncingBallAnimation()` dibuja una pelota rebotando y dejando rastro, `showCenterExplosion()` genera una explosión radial y `showLoadingBarAnimation()` presenta PINPONG, barra de progreso y mensaje de bienvenida. Son animaciones del arranque; sus valores de velocidad no pertenecen al motor de física de la partida.

**Audio:** `menuMusic[]` y `gameMusic[]` almacenan pares de frecuencia (Hz) y duración (ms), con frecuencia cero para silencios. `updateAudio()` avanza notas según `nextNoteTime`, usando `millis()` para no detener innecesariamente el ciclo principal. `playSFX()` hace sonar eventos y da prioridad temporal al efecto mediante `sfxEndTime`; `stopAudio()` silencia el zumbador. Los efectos incluyen navegación de menú, golpe de paleta, pared, obtención de poder, portal, multibola, punto y fin del juego. Hay música de menú/configuración de partida y música durante `STATE_PLAY` si la preferencia está habilitada.

**Transiciones:** `animateScreenWipe()` realiza un barrido gráfico para cambios entre ciertas pantallas. **Attract mode** usa su propia pelota y paletas demostrativas en vez de depender de `balls[0]` o de puntos reales.

## 17. Ajustes y almacenamiento permanente

`Preferences prefs` usa el espacio NVS llamado **`arcade`** para conservar preferencias aunque se reinicie el ESP32. `loadSettings()` abre en modo lectura y obtiene valores o sus valores predeterminados; `saveSettings()` abre en modo escritura y guarda las variables configuradas. `checkAndSaveHighScore()` escribe específicamente `hscore` cuando se supera el valor histórico.

| Clave NVS | Variable | Valor inicial |
|---|---|---|
| `music` | `musicEnabled` | `true` |
| `sfx` | `sfxEnabled` | `true` |
| `invert` | `invertControls` | `false` |
| `diff` | `gameDifficulty` | `1` (NORMAL) |
| `style` | `playStyle` | `0` (CLÁSICO) |
| `hscore` | `highScore` | `0` |
| `bright` | `screenBrightness` | `255` |
| `w_1pia` | `wins_1p_ia` | `0` |
| `w_ia` | `wins_ia` | `0` |
| `w_p1vs` | `wins_p1_vs` | `0` |
| `w_p2vs` | `wins_p2_vs` | `0` |

En el código revisado, `gameMode` y `scoreLimit` **no** están en la lista almacenada por `saveSettings()`; sus valores iniciales en ejecución son 1P y 5 puntos. El brillo se aplica mediante una orden de contraste SH1106 enviada por I²C. El récord mostrado `HI` se actualiza si la puntuación que se compara al terminar un juego supera el anterior; no representa necesariamente la mayor cantidad de intercambios seguidos.

**Modo dormir:** desde SALIR, `handleSleep()` hace una animación de apagado, configura despertar por nivel bajo en el GPIO del botón 1 y llama a sueño profundo del ESP32. Despertar desde sueño profundo implica reiniciar el programa y volver a ejecutar `setup()`.

## 18. Variables y constantes importantes

| Símbolo | Valor en esta versión | Para qué sirve |
|---|---:|---|
| `SCREEN_WIDTH`, `SCREEN_HEIGHT` | 128, 64 | Dimensiones OLED |
| `COURT_TOP_Y` | 13 | Límite superior jugable |
| `COURT_BOTTOM_Y` | 63 | Límite inferior |
| `PADDLE_H`, `PADDLE_W` | 14, 2 | Dimensiones normales de paleta |
| `paddleSpeed` | 2.5 | Velocidad de referencia de paleta |
| `PADDLE_RESPONSE_GAIN` | 1.70 | Amplifica el mando de la paleta |
| `PADDLE_INFLUENCE` | 0.38 | Peso del movimiento de paleta en el rebote |
| `BALL_SIZE` | 2 | Tamaño visual de pelota |
| `BALL_BASE_SPEED` | 2.0 | Base horizontal al iniciar rally |
| `BALL_SPEED_PER_HIT` | 0.09 | Incremento por golpe de paleta |
| `BALL_MAX_SPEED` | 4.8 | Máximo de velocidad horizontal objetivo |
| `MIN_VERTICAL_SPEED` | 0.45 | Mínimo vertical impuesto después de un golpe |
| `MAX_VERTICAL_SPEED` | 3.2 | Máximo del componente vertical |
| `FRAME_REFERENCE_MS` | 16.6667 | Escala temporal respecto de ~60 FPS |
| `JOY_CENTER` | 2048 | Centro lógico del joystick |
| `JOY_DEADZONE` | 160 | Banda central de no movimiento |
| `JOY_FILTER_ALPHA` | 0.42 | Proporción de entrada nueva en filtro |
| `JOY_CALIBRATION_SAMPLES` | 80 | Muestras de calibración inicial |
| `SERVE_STEP_MS` | 300 ms | Tiempo de cada número de la cuenta |
| `INACTIVITY_TIMEOUT` | 15000 ms | Paso a demostración |
| `HIT_FEEDBACK_MS` | 220 ms | Duración del texto PERF/ANG/NORM |
| `MULTIBALL_TRIGGER_HITS` | 12 | Golpes necesarios para probar multibola |
| `MULTIBALL_CHANCE` | 35 | Porcentaje de oportunidad en único intento |

**Estado que cambia mientras se juega:** `consecutiveHits`, posiciones y velocidades de `balls[]` y paletas, `lastHitBy`, `lastPlayerToHit`, tipos y temporizadores de poder, objeto `activePortal`, `currentState`, `scoreP1`, `scoreP2`, `lastPhysicsUpdate` y objetivo de CPU. Estas variables permiten que el juego continúe entre fotogramas sin reinicializarse cada vez.

## 19. Catálogo de todas las funciones

La siguiente relación corresponde a las **definiciones de funciones** presentes en el archivo entregado (además de los prototipos de la parte superior). Los números de línea sirven para encontrar el bloque en `Texto pegado.txt`.

| Función (línea aprox.) | Explicación funcional |
|---|---|
| `calibrateJoysticks` (371) | Promedia entradas analógicas y determina centros físicos válidos. |
| `normalizeJoystickReading` (406) | Compensa el centro físico para obtener escala lógica 0–4095. |
| `readFilteredAxis` (428) | Lee eje, filtra exponencialmente y normaliza. |
| `readJoystickY` (440) | Lee Y de J1 y aplica inversión global. |
| `readJoystickX` (451) | Lee X de J1. |
| `readJoystick2Y` (461) | Lee Y de J2 y aplica inversión global. |
| `setup` (474) | Inicializa hardware, ajustes, joysticks, animaciones y temporizadores. |
| `loop` (502) | Actualiza audio y despacha el estado actual del juego. |
| `playSFX` (553) | Reproduce un sonido y le asigna prioridad sobre música. |
| `stopAudio` (564) | Detiene la generación de tono. |
| `updateAudio` (573) | Programa notas de menú o partida sin bloquear el ciclo principal. |
| `showStudioScreen` (611) | Animación del logotipo inicial. |
| `showBouncingBallAnimation` (659) | Demostración de pelota rebotando durante el arranque. |
| `showCenterExplosion` (717) | Efecto radial de presentación. |
| `showLoadingBarAnimation` (781) | Animación de título, barra de carga y bienvenida. |
| `handleMenu` (906) | Dibuja el menú, acepta selección, anima estrellas e inicia attract. |
| `handleScores` (1027) | Presenta estadísticas de victorias. |
| `handleCredits` (1077) | Presenta los créditos del juego. |
| `handleMatchSetup` (1116) | Selecciona estilo, jugadores, dificultad, límite e inicio. |
| `handleSettings` (1307) | Ajusta música, SFX, inversión y contraste; permite regresar. |
| `handleSleep` (1424) | Apagado visual y sueño profundo con despertar por botón. |
| `handleAttractMode` (1464) | Demostración independiente, cancelable por interacción. |
| `handleSplashScreen` (1559) | Estado de presentación alternativo. |
| `animateScreenWipe` (1603) | Barrido animado al transicionar. |
| `loadSettings` (1623) | Recupera preferencias y estadísticas de NVS. |
| `saveSettings` (1648) | Guarda preferencias y estadísticas en NVS. |
| `checkAndSaveHighScore` (1671) | Actualiza `hscore` cuando se supera el récord. |
| `setOledBrightness` (1685) | Envía orden de contraste de pantalla por I²C. |
| `headerHud` (1698) | Dibuja indicadores de música y SFX en cabeceras de menús. |
| `handlePlay` (1720) | Motor del fotograma: HUD, controles, física, especiales, impactos y puntos. |
| `handlePointScored` (2125) | Presenta cuenta regresiva y libera el siguiente saque. |
| `handleGameOver` (2195) | Presenta ganador y permite nueva partida o menú. |
| `handlePause` (2253) | Gestiona continuar, configuración o salida. |
| `getCurrentBallSpeed` (2336) | Calcula `2.0 + 0.09 × golpes`, con tope 4.8. |
| `calculateBounceY` (2348) | Combina zona de contacto y velocidad de paleta en el rebote. |
| `classifyPaddleHit` (2372) | Clasifica NORMAL, ANGULADO y PERFECTO. |
| `hitSpeedMultiplier` (2396) | Da factor horizontal ×1.00, ×1.04 o ×1.08. |
| `applyHitVerticalFeel` (2408) | Da factor vertical ×1.00, ×1.08 o ×1.15 y limita el resultado. |
| `registerHitFeedback` (2420) | Muestra etiqueta temporal y emite efecto sonoro. |
| `updatePlayerPaddle` (2440) | Actualiza movimiento de paleta mediante entrada de joystick. |
| `updateCpuPaddle` (2469) | Selecciona objetivo, predice y desplaza paleta de IA. |
| `scheduleArcadeTimers` (2540) | Limpia especiales y programa siguientes tiempos en ARCADE. |
| `beginServeSequence` (2563) | Alterna sentido de saque e inicia cuenta visual. |
| `checkAndEnterGameOver` (2580) | Comprueba límite, guarda resultado y muestra ganador. |
| `resetMainBall` (2613) | Centra pelota principal y desactiva la secundaria. |
| `maybeTriggerMultiball` (2633) | Al golpe 12, prueba una sola vez oportunidad de multibola. |
| `spawnSecondaryBall` (2650) | Activa una segunda pelota con dirección inicial opuesta. |

## 20. Ejemplo narrado de una partida ARCADE

1. Desde JUGAR seleccionas **ARCADE, 1P IA, NORMAL y 5 puntos**, y pulsas EMPEZAR.
2. Se muestra el saque **3, 2, 1**; la pelota arranca en el centro con componente horizontal 2.0.
3. P1 la devuelve una vez: `consecutiveHits = 1`; el objetivo pasa a **2.09**, y el HUD normalmente redondea a **`A 2.1`** después del aviso fugaz de calidad.
4. La CPU la devuelve: contador 2, objetivo **2.18**, HUD **`A 2.2`**. Si el impacto fue ANGULADO, el motor multiplica temporalmente la velocidad horizontal de salida por **1.04**.
5. Empieza a verse la estela al llegar a seis devoluciones, si se cumple la condición de renderizado indicada en el código.
6. Pasado el intervalo aleatorio, puede aparecer la **caja intermitente**. Si una pelota tocada por P1 recoge `GIGANTE`, la paleta izquierda pasa de 14 a 28 píxeles y el HUD muestra `GIGANTE` en vez de `A X.X` mientras esté activo.
7. Puede aparecer un portal de forma independiente. Si la pelota lo cruza, se reubica verticalmente y acelera su componente horizontal **×1.20**, con máximo 4.8.
8. Si ambos jugadores acumulan **12 devoluciones** dentro del mismo rally, se realiza una única prueba de **35 %** para generar la segunda pelota.
9. Si la pelota cruza el borde derecho, P1 anota. Si existían dos pelotas, la segunda puede seguir viva; si se pierde la última, se restablecen los especiales, el contador vuelve a cero y se prepara otro saque.
10. Cuando alguien llega a **5 puntos**, se muestran ganador y opciones de nueva partida o menú; las victorias se guardan.

## 21. Qué modificar si deseas personalizar el juego

Estos son **lugares del código actual** que gobiernan el comportamiento; la tabla no implica que ya se hayan realizado cambios:

| Quiero cambiar… | Dónde revisar | Consecuencia |
|---|---|---|
| Que el HUD pase de A 2.1 a A 2.2 más rápidamente | `BALL_SPEED_PER_HIT` | Cambia la aceleración base por devolución en ambos estilos. |
| Velocidad inicial | `BALL_BASE_SPEED` | Afecta primer saque, objetivo de rally y reinicio de pelota. |
| Velocidad máxima | `BALL_MAX_SPEED` | Limita objetivo y salidas horizontales de golpes y portal. |
| Mostrar más decimales | `oledMonitor.print(getCurrentBallSpeed(), 1)` | Cambiar `1` a `2` permite visualizar centésimas; hay poco espacio disponible en HUD. |
| Cuándo se permite multibola | `MULTIBALL_TRIGGER_HITS` | Número de devoluciones para intentar su activación. |
| Probabilidad de multibola | `MULTIBALL_CHANCE` | Porcentaje del único intento por rally. |
| Frecuencia de ítems | `powerUpSpawnDelay` y sus asignaciones `random(10000,15001)` | Intervalo de aparición de cajas. |
| Duración de ítems | condición `> 5000` en `handlePlay` | Duración temporal compartida de poderes. |
| Qué hace cada poder | ramas `powerUpType == 0..4` de `handlePlay` | Cambia el efecto mecánico al recoger ítem. |
| Portales | `portalSpawnDelay`, `activePortal` y colisión en `handlePlay` | Aparición, forma y reacción al atravesar. |
| Velocidad de los controles | `paddleSpeed`, `PADDLE_RESPONSE_GAIN`, `JOY_FILTER_ALPHA` | Sensibilidad/respuesta de paletas. |
| Dificultad CPU | `updateCpuPaddle` | Tiempo de reacción, error, predicción y desplazamiento. |
| Tipo de golpe | `classifyPaddleHit`, `hitSpeedMultiplier`, `applyHitVerticalFeel` | Zonas perfectas/anguladas y sus efectos. |

**Cuidado:** incrementar una constante sin revisar las otras puede producir trayectorias difíciles de seguir en 128 × 64 píxeles. Para comparar resultados, prueba por separado primero el cálculo del HUD y luego la velocidad real de `balls[i].dx`.

## 22. Advertencias y puntos a tener presentes

- **Documentación, no certificación de compilación:** se inspeccionó el texto proporcionado. No se ha compilado ni probado físicamente esta versión en una placa y pantalla concretas.
- **HUD ≠ velocímetro real:** el valor `A 2.1` / `C 2.1` procede de `getCurrentBallSpeed()` y muestra un decimal. No incluye directamente velocidad vertical, cambio ×1.20 del portal ni el multiplicador del último golpe.
- **El contador cuenta devoluciones de paleta, no rebotes contra la pared.** Si el HUD crece, debe ser por aumento de `consecutiveHits` tras impactos de paleta.
- **Duración y estado de FUEGO:** la modificación inmediata de `dx` no está sostenida durante los cinco segundos que la etiqueta puede permanecer visible.
- **Un único intento de multibola:** 35 % al alcanzar al menos 12 golpes, una vez por rally; no una tirada en cada nuevo golpe.
- **El encabezado del archivo mezcla etiquetas históricas V3–V8:** algunas secciones llevan número de versión local aunque el encabezado principal diga V6. La documentación describe los valores y ramas realmente presentes, no asume una cronología exacta de versiones.
- **No está implementada una pareja de portales.** `activePortal` reubica una pelota que colisiona con él, la acelera horizontalmente y desaparece.
- **Doble botón:** el botón de J2 se configura, pero no tiene el mismo conjunto de acciones en los controladores de interfaz mostrados; los menús y la pausa giran alrededor de J1.
- **Visualización y física pueden diferir:** algunos indicadores visuales se muestran por tiempo y no reevalúan toda la física en tiempo real.
- **Si ves un número que no coincide exactamente con una tabla:** puede ser efecto del redondeo, del momento del fotograma en que se dibuja el HUD o de un elemento especial que cambia momentáneamente el movimiento.

---

**Resumen clave:** el valor **`A 2.1`** significa que estás en **ARCADE** y la velocidad **base objetivo del rally** es aproximadamente **2.1 unidades internas**. Nace en **2.0**, aumenta **0.09 por cada devolución de paleta**, se muestra redondeado a **un decimal** y se limita a **4.8**. La diferencia entre lo mostrado y el movimiento que observas puede provenir del tipo de golpe, un portal y el ajuste temporal del fotograma.

*README elaborado específicamente a partir del código compartido. No modifica el `.ino`.*











   
