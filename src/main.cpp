/* PinPong Arcade - V4 */
/**
 * @file      PinPong_Arcade.ino
 * @author    Arcade Studio
 * @brief     Secuencia de inicio, menús y juego Pong retro para consola ESP32.
 *            Incluye persistencia NVS (Flash), física con aceleración y rebote por inercia,
 *            motor de audio no bloqueante, submenú de configuración, IA para la CPU,
 *            modo demostración (Attract Mode), multibola, portales y power-ups.
 * @hardware  ESP32, Pantalla OLED SH1106 (I2C), Zumbador Pasivo + Transistor 2N2222.
 */

// ==========================================
//   INCLUSIÓN DE LIBRERÍAS
// ==========================================
#include <Arduino.h>           // Librería base del framework Arduino para ESP32
#include <Wire.h>              // Comunicación protocolo I2C (para la pantalla OLED)
#include <Adafruit_GFX.h>      // Librería de gráficos primarios (puntos, líneas, texto)
#include <Adafruit_SH1106.h>   // Controlador específico del display OLED SH1106 128x64
#include <Preferences.h>       // Gestión de memoria no volátil NVS (Flash) del ESP32
#include <Fonts/Picopixel.h>   // Fuente de texto de tamaño micro para la pantalla de puntuaciones

// Objeto para manipular el almacenamiento en memoria Flash NVS
Preferences prefs;

// ==========================================
//   CONFIGURACIÓN DE HARDWARE Y PINES
// ==========================================
#define OLED_SDA 21            // Pin GPIO21 asignado a la línea de datos I2C (SDA)
#define OLED_SCL 22            // Pin GPIO22 asignado a la línea de reloj I2C (SCL)
#define OLED_RESET -1          // Pin de Reset del OLED (-1 si comparte el reset con la placa)
#define SCREEN_WIDTH 128       // Ancho físico de la pantalla OLED en píxeles
#define SCREEN_HEIGHT 64       // Alto físico de la pantalla OLED en píxeles
#define OLED_ADDRESS 0x3C      // Dirección I2C predeterminada del controlador SH1106

// Configuración del pin del zumbador (Buzzer)
#define BUZZER_PIN 5           // Pin GPIO5 dedicado a emitir tonos de audio PWM

// ==========================================
//   CONFIGURACIÓN DE CONTROLES (JOYSTICKS)
// ==========================================
#define J1_Y_PIN 34            // Entrada analógica GPIO34: Eje Y del Joystick 1 (Jugador 1)
#define J1_BTN_PIN 25          // Entrada digital GPIO25: Botón del Joystick 1 (Selección / Pausa)
#define J1_X_PIN 32            // Entrada analógica GPIO32: Eje X del Joystick 1 (Navegación en menús)
#define J2_Y_PIN 35            // Entrada analógica GPIO35: Eje Y del Joystick 2 (Jugador 2)
#define J2_BTN_PIN 26          // Entrada digital GPIO26: Botón del Joystick 2

// ==========================================
//   MÁQUINA DE ESTADOS DEL JUEGO
// ==========================================
// Enumeración con todos los estados posibles en la lógica del juego
enum GameState {
  STATE_SPLASH,                // Pantalla de presentación del estudio y bienvenida
  STATE_MENU,                  // Menú principal interactivo
  STATE_MATCH_SETUP,           // Submenú para configurar la partida (Modo, Dificultad, Puntos)
  STATE_PLAY,                  // Bucle principal de juego activo
  STATE_POINT_SCORED,          // Pausa temporal tras marcar un punto
  STATE_PAUSE,                 // Menú de pausa durante la partida
  STATE_GAME_OVER,             // Pantalla de fin de juego y proclamación del ganador
  STATE_SETTINGS,              // Submenú de ajustes (Audio, Inversión, Brillo)
  STATE_CREDITS,               // Pantalla con la información de los desarrolladores
  STATE_SCORES,                // Pantalla con las estadísticas acumuladas de victorias
  STATE_SLEEP,                 // Modo de suspensión de bajo consumo (Deep Sleep)
  STATE_ATTRACT                // Modo demostración automático por inactividad
};

// Variable global que almacena el estado actual de la máquina de estados
GameState currentState = STATE_MENU;

// ==========================================
//   ESTRUCTURA Y MOTOR DE AUDIO NO BLOQUEANTE
// ==========================================
// Estructura para almacenar las notas musicales y su duración
struct Note {
  int frequency;               // Frecuencia en Hertz (0 representa un silencio)
  int duration;                // Duración de la nota en milisegundos
};

// Melodía para los menús principales y de preparación
const Note menuMusic[] = {
  {262, 120}, {330, 120}, {392, 120}, {523, 200}, {0, 60},
  {349, 120}, {440, 120}, {523, 120}, {659, 200}, {0, 60},
  {294, 120}, {392, 120}, {494, 120}, {587, 200}, {0, 60},
  {392, 120}, {494, 120}, {587, 120}, {784, 240}, {0, 100}
};
// Cantidad de notas que componen la melodía del menú
const int menuMusicLength = sizeof(menuMusic) / sizeof(menuMusic[0]);

// Melodía durante la partida (Gameplay)
const Note gameMusic[] = {
  {523, 80}, {587, 80}, {659, 80}, {698, 80}, {784, 120}, {0, 40},
  {659, 80}, {523, 80}, {587, 80}, {392, 160}, {0, 40},
  {440, 80}, {494, 80}, {523, 80}, {587, 80}, {659, 120}, {0, 40},
  {587, 80}, {494, 80}, {523, 80}, {392, 160}, {0, 40}
};
// Cantidad de notas que componen la melodía del juego
const int gameMusicLength = sizeof(gameMusic) / sizeof(gameMusic[0]);

int musicIndex = 0;             // Índice actual de la nota reproduciéndose
unsigned long nextNoteTime = 0; // Tiempo (millis) programado para tocar la siguiente nota
unsigned long sfxEndTime = 0;   // Marca de tiempo hasta la cual un efecto de sonido (SFX) bloquea la música

// ==========================================
//   INSTANCIAS Y VARIABLES GLOBALES
// ==========================================
// Instanciación del objeto para controlar la pantalla OLED SH1106
Adafruit_SH1106 oledMonitor(OLED_RESET);

int currentMenuOption = 0;       // Opción seleccionada en el menú principal
const int totalMenuOptions = 5;  // Cantidad total de opciones del menú principal
bool joystickLocked = false;     // Antirebote por software para la lectura del joystick
unsigned long lastBtnPress = 0;  // Marca de tiempo de la última pulsación de botón registrada

// Variables para la animación del fondo estrellado en menús
const int MENU_STARS = 12;
float mStarX[MENU_STARS];        // Posiciones X de las estrellas
int mStarY[MENU_STARS];          // Posiciones Y de las estrellas
float mStarSpeed[MENU_STARS];    // Velocidad de desplazamiento de cada estrella
bool starsInitialized = false;   // Bandera para inicializar las estrellas una sola vez

// Control de tiempo para entrar al modo demostración (Attract Mode)
unsigned long lastActivityTime = 0; 
const unsigned long INACTIVITY_TIMEOUT = 15000; // Entra a Attract Mode tras 15s sin interacción

// VARIABLES DE CONFIGURACIÓN GLOBAL (Persistentes)
bool musicEnabled   = true;      // Control general de música activada/desactivada
bool sfxEnabled     = true;      // Control general de efectos de sonido activados/desactivados
bool invertControls = false;     // Inversión global del eje Y del Joystick
int screenBrightness = 255;      // Nivel de brillo de la pantalla OLED (0-255)

// CONFIGURACIÓN DE LA PARTIDA
int gameDifficulty = 1;          // Nivel de dificultad de la CPU: 0 (Fácil), 1 (Normal), 2 (Difícil)
int gameMode       = 0;          // Modo de juego: 0 = 1P vs CPU, 1 = 1P vs 2P
int scoreLimit     = 5;          // Límite de puntos para ganar la partida (3, 5 o 10)

int currentSetupOption   = 0;    // Opción seleccionada en la pantalla de preparación
int currentSettingOption = 0;    // Opción seleccionada en la pantalla de configuración

// Registros de puntuaciones e historial de victorias
int highScore = 0;               // Máximo puntaje registrado
int wins_1p_ia = 0;             // Victorias del Jugador 1 contra la IA
int wins_ia = 0;                // Victorias de la IA contra el Jugador 1
int wins_p1_vs = 0;             // Victorias del Jugador 1 en Modo Versus (2P)
int wins_p2_vs = 0;             // Victorias del Jugador 2 en Modo Versus (2P)

// ==========================================
//   MOTOR FÍSICO
// ==========================================
float paddle1Y = 24;             // Posición Y de la paleta izquierda (Jugador 1)
float paddle2Y = 24;             // Posición Y de la paleta derecha (Jugador 2 / CPU)
const int PADDLE_H = 14;         // Altura estándar de las paletas en píxeles
const int PADDLE_W = 2;          // Ancho estándar de las paletas en píxeles
float paddleSpeed = 2.5;         // Velocidad base de desplazamiento de las paletas

// Variables de inercia y velocidad de paleta para transferir impulso al rebotar
float paddle1Velocity = 0.0;
float paddle2Velocity = 0.0;
float lastPaddle1Y = 24.0;
float lastPaddle2Y = 24.0;

// Constantes físicas del motor de juego
const float FRAME_REFERENCE_MS = 16.6667; // Referencia de tiempo por frame (equivale a 60 FPS)
const float BALL_MAX_SPEED = 5.5;          // Límite máximo absoluto de velocidad de la pelota
const float RALLY_SPEED_BONUS = 0.018;     // Incremento gradual de velocidad por cada rebote consecutivo
const int RALLY_BONUS_CAP = 20;            // Límite máximo de rebotes para aplicar el bono de velocidad
const float PADDLE_INFLUENCE = 0.46;       // Factor de influencia del movimiento de la paleta en el rebote
const int JOY_CENTER = 2048;               // Valor medio del conversor analógico-digital (ADC) del ESP32
const int JOY_DEADZONE = 220;              // Zona muerta central del Joystick para prevenir derivas
const int JOY_MIN = 0;                     // Lectura mínima del potenciómetro
const int JOY_MAX = 4095;                  // Lectura máxima del potenciómetro
const float MIN_VERTICAL_SPEED = 0.55;     // Velocidad vertical mínima para evitar trayectorias totalmente horizontales
const float MAX_VERTICAL_SPEED = 3.2;      // Velocidad vertical máxima de la pelota
const int COURT_TOP_Y = 13;                // Límite superior de la cancha (debajo del marco HUD)
const int COURT_BOTTOM_Y = SCREEN_HEIGHT - 1; // Límite inferior de la cancha

unsigned long lastPhysicsUpdate = 0;       // Marca de tiempo del último cálculo físico
unsigned long powerUpSpawnDelay = 12000;   // Tiempo de espera para generar cajas de habilidades
unsigned long portalSpawnDelay = 10000;    // Tiempo de espera para la aparición de portales

const int BALL_SIZE = 2;         // Dimensión del cuadrado que representa la pelota (2x2 px)
int scoreP1 = 0;                 // Puntuación del Jugador 1
int scoreP2 = 0;                 // Puntuación del Jugador 2 / CPU

const int HITS_FOR_NEXT_LEVEL = 10; // Rebotes necesarios para aumentar el nivel de velocidad general
const float BALL_SPEED_LVL1 = 2.0;  // Velocidad de pelota en Nivel 1
const float BALL_SPEED_LVL2 = 3.5;  // Velocidad de pelota en Nivel 2
const float BALL_SPEED_LVL3 = 5.0;  // Velocidad de pelota en Nivel 3

int consecutiveHits = 0;         // Contador de rebotes seguidos durante el intercambio activo
int currentSpeedLevel = 1;       // Nivel actual de velocidad del juego

// Temporizadores para retroalimentación visual en pantalla
unsigned long perfectHitTimer = 0; // Temporizador para el texto "PERFECT!" (Golpe en el centro)
unsigned long nearMissTimer = 0;   // Temporizador para el texto "CERCA!" (A pocos píxeles de perder)
int perfectHitPlayer = 0;          // Jugador que realizó el impacto perfecto
int nearMissPlayer = 0;            // Jugador que casi pierde la pelota
int wallImpactDirection = 0;       // Dirección de impacto con paredes horizontales
int wallImpactPower = 0;           // Magnitud de la colisión para efectos gráficos y vibración

unsigned long stateTimer = 0;      // Temporizador genérico de estados de transición
int gameOverOption = 0;            // Opción seleccionada en la pantalla de Game Over
unsigned long pauseButtonTimer = 0;// Temporizador para detectar si se mantiene pulsado el botón de pausa
bool isPauseButtonPressed = false; // Estado del botón de pausa
int pauseOption = 0;               // Opción elegida dentro del menú de pausa
GameState settingsReturnState = STATE_MENU; // Estado al cual regresar al salir de Ajustes

// Efectos de estela y sacudida de pantalla (Screen Shake)
float trailX[3], trailY[3];        // Coordenadas para renderizar las 3 posiciones anteriores de la pelota
int shakeFrames = 0;               // Cantidad de frames restantes de vibración de pantalla
int wallFlashFrames = 0;           // Frames del destello visual en colisiones con paredes
int wallFlashX = 0;                // Coordenada X del impacto con pared
int wallFlashY = 0;                // Coordenada Y del impacto con pared

// SISTEMA DE CAJAS MISTERIOSAS (POWER-UPS)
bool powerUpActive = false;        // Indica si hay una caja mística presente en el campo
float powerUpX = 0, powerUpY = 0;  // Coordenadas de la caja misteriosa
int powerUpType = 0;               // Tipo de poder: 0=Gigante, 1=Encoger, 2=Fuego, 3=Fantasma, 4=Invertir
unsigned long powerUpSpawnTimer = 0;    // Temporizador de aparición
unsigned long powerUpDurationTimer = 0; // Temporizador de duración de la habilidad activa

int lastPlayerToHit = 0;           // Único jugador con derecho a reclamar la caja (último en golpear)
int activePowerUpPlayer = 0;       // Jugador beneficiado/afectado por la habilidad activa
int activePowerUpType = -1;        // ID de la habilidad activa (-1 = Ninguna)

int currentPaddle1_H = PADDLE_H;   // Altura dinámica de la paleta P1
int currentPaddle2_H = PADDLE_H;   // Altura dinámica de la paleta P2
bool ghostBallActive = false;      // Bandera de pelota fantasma (parpadeante e invisible a ratos)
bool invertedControlsP1 = false;   // Estado de controles invertidos por castigo para P1
bool invertedControlsP2 = false;   // Estado de controles invertidos por castigo para P2

// ESTRUCTURA MULTIPELOTA
struct Ball {
  float x, y;                      // Posición X e Y de la pelota
  float dx, dy;                    // Vectores de velocidad X e Y
  float spin;                      // Efecto / Curva aplicada a la trayectoria
  int lastHitBy;                   // 0 = Nadie, 1 = P1, 2 = P2
  bool active;                     // Estado de activación en la pantalla
};

Ball balls[2];                     // Arreglo con la pelota principal [0] y la secundaria [1]

// ESTRUCTURA DE PORTALES DE TELETRANSPORTACIÓN
struct Portal {
  int x, y, w, h;                  // Posición y dimensiones del portal
  bool active;                     // Estado de activación del portal
  unsigned long spawnTime;         // Momento de creación
};

Portal activePortal = {0, 0, 8, 16, false, 0}; // Instancia del portal
unsigned long nextPortalTimer = 0;              // Tiempo para habilitar el siguiente portal

// ==========================================
//   BITMAPS PIXEL-ART (Almacenados en Flash / PROGMEM)
// ==========================================
const uint8_t PROGMEM icon_play[] = {
  0b01110000, 0b10001010, 0b10001000, 0b01110000, 0b00100000, 0b00100000, 0b00100000
};
const uint8_t PROGMEM icon_settings[] = {
  0b00111000, 0b01010100, 0b10000010, 0b11010110, 0b10000010, 0b01010100, 0b00111000
};
const uint8_t PROGMEM icon_credits[] = {
  0b11111110, 0b10111010, 0b01111100, 0b00111000, 0b00010000, 0b00010000, 0b00111000
};
const uint8_t PROGMEM icon_sleep[] = {
  0b00010000, 0b01111100, 0b11010010, 0b10010010, 0b10000010, 0b01000010, 0b00111000
};
const uint8_t PROGMEM icon_scores[] = {
  0b11111110, 0b01111100, 0b00111000, 0b00111000, 0b00010000, 0b00111000, 0b01111100
};
const uint8_t PROGMEM hud_music_on[] = {
  0b00111000, 0b00101000, 0b00101000, 0b00101000, 0b01101100, 0b11101110, 0b01100110
};
const uint8_t PROGMEM hud_sfx_on[] = {
  0b00010000, 0b00110000, 0b11110100, 0b11111010, 0b11110100, 0b00110000, 0b00010000
};
const uint8_t PROGMEM hud_off_mark[] = {
  0b00000000, 0b10000010, 0b01000100, 0b00101000, 0b01000100, 0b10000010, 0b00000000
};

// Arreglo de punteros hacia las imágenes bitmap para iterar fácilmente en los menús
const uint8_t* const menuIcons[] = {
  icon_play, icon_settings, icon_scores, icon_credits, icon_sleep 
};

// ==========================================
//   PROTOTIPOS DE FUNCIONES
// ==========================================
int readJoystickY();
void showStudioScreen();
void showBouncingBallAnimation();
void showCenterExplosion();
void showLoadingBarAnimation();

void loadSettings();
void saveSettings();
void checkAndSaveHighScore(int currentScore);
void setOledBrightness(uint8_t brightness);

void handleSplashScreen();
void handleMenu();
void handleMatchSetup();
void handleSettings();
void handleScores();
void handleCredits();
void handleSleep();
void handleAttractMode();
void headerHud();

void animateScreenWipe();
void playSFX(int frequency, int duration);
void updateAudio();
void stopAudio();

void handlePlay();
void handlePointScored();
void handleGameOver();
void handlePause();

void resetMainBall(float dirX);
void spawnSecondaryBall();
void scheduleArcadeTimers();
void updatePlayerPaddle(int rawY, float &paddleY, int paddleHeight, bool inverted, float physicsScale);
void updateCpuPaddle(float physicsScale);
float calculateBounceY(float ballY, float paddleY, int paddleHeight, float paddleVelocity);
float getCurrentBallSpeed();

// ==========================================
//   HELPER DE LECTURA CON INVERSIÓN DE AXIS
// ==========================================
/**
 * @brief Lee el valor analógico del eje Y del Joystick 1 y aplica la inversión si está activada.
 * @return Entero entre 0 y 4095 con el valor analógico ajustado.
 */
int readJoystickY() {
  int rawY = analogRead(J1_Y_PIN);
  return invertControls ? (4095 - rawY) : rawY;
}

// ==========================================
//   CONFIGURACIÓN INICIAL (SETUP) Y BUCLE PRINCIPAL (LOOP)
// ==========================================
void setup() {
  // Inicialización del bus I2C con los pines SDA y SCL definidos
  Wire.begin(OLED_SDA, OLED_SCL);
  
  // Inicializa la pantalla OLED con la dirección dada y habilitación del multiplicador de voltaje
  oledMonitor.begin(SH1106_SWITCHCAPVCC, OLED_ADDRESS);
  oledMonitor.clearDisplay();
  oledMonitor.display();

  // Configuración de modos de pin
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(J1_BTN_PIN, INPUT_PULLUP); // Resistencia de PULLUP interna para botones
  pinMode(J2_BTN_PIN, INPUT_PULLUP);

  // Carga configuraciones guardadas previamente en la Flash
  loadSettings();

  // Secuencia cinemática de animaciones de inicio
  showStudioScreen();         
  showBouncingBallAnimation(); 
  showCenterExplosion();       
  showLoadingBarAnimation();   
  
  // Registro de tiempo inicial para inactividad y temporizadores
  lastActivityTime = millis();
  scheduleArcadeTimers();
}

void loop() {
  // Actualizador no bloqueante del motor de sonido
  updateAudio();

  // Evaluación de la máquina de estados
  switch (currentState) {
    case STATE_SPLASH:
      handleSplashScreen();
      break;
    case STATE_MENU:
      handleMenu();
      break;
    case STATE_MATCH_SETUP:
      handleMatchSetup();
      break;
    case STATE_ATTRACT:
      handleAttractMode();
      break;
    case STATE_PLAY:
      handlePlay();
      break;
    case STATE_POINT_SCORED:
      handlePointScored();
      break;
    case STATE_PAUSE: 
      handlePause();
      break;
    case STATE_GAME_OVER:
      handleGameOver();
      break;
    case STATE_SETTINGS:
      handleSettings();
      break;
    case STATE_SCORES:
      handleScores();
      break;
    case STATE_CREDITS:
      handleCredits();
      break;
    case STATE_SLEEP:
      handleSleep();
      break;
  }
}

// ==========================================
//   SISTEMA DE AUDIO CENTRALIZADO
// ==========================================
/**
 * @brief Emite un efecto de sonido bloqueando temporalmente la música de fondo.
 * @param frequency Frecuencia del tono en Hertz.
 * @param duration Duración del sonido en milisegundos.
 */
void playSFX(int frequency, int duration) {
  if (!sfxEnabled) return;
  sfxEndTime = millis() + duration; // Establece el tiempo de ocupado por el efecto
  tone(BUZZER_PIN, frequency, duration);
}

/**
 * @brief Detiene la emisión actual de tono en el parlante.
 */
void stopAudio() {
  noTone(BUZZER_PIN);
}

/**
 * @brief Procesa el avance de la música de fondo en segundo plano sin detener la ejecución del programa.
 */
void updateAudio() {
  if (millis() < sfxEndTime) return; // Si hay un efecto reproduciéndose, cede la prioridad
  if (!musicEnabled) return;

  const Note* currentTrack = NULL;
  int trackLength = 0;

  // Selecciona la pista adecuada según el estado del juego
  if (currentState == STATE_MENU || currentState == STATE_MATCH_SETUP) {
    currentTrack = menuMusic;
    trackLength = menuMusicLength;
  } else if (currentState == STATE_PLAY) {  
    currentTrack = gameMusic;
    trackLength = gameMusicLength;
  } else {
    noTone(BUZZER_PIN);
    return;
  }

  // Reproduce la nota programada si se alcanzó el tiempo
  if (millis() >= nextNoteTime) {
    int freq = currentTrack[musicIndex % trackLength].frequency;
    int dur  = currentTrack[musicIndex % trackLength].duration;

    if (freq > 0) tone(BUZZER_PIN, freq, dur);
    else noTone(BUZZER_PIN);

    nextNoteTime = millis() + dur + 15; // 15ms de pausa entre notas para articulación
    musicIndex = (musicIndex + 1) % trackLength;
  }
}

// ==========================================
//   ANIMACIONES DE ENTRADA
// ==========================================
/**
 * @brief Renderiza la intro del estudio desarrollador letra por letra con efecto de sonido.
 */
void showStudioScreen() {
  oledMonitor.clearDisplay();
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);

  const char* linea1 = "ARCADE STUDIO";
  const char* linea2 = "PRESENTS";

  // Efecto máquina de escribir para la Línea 1
  oledMonitor.setCursor(26, 20); 
  for(int i = 0; i < strlen(linea1); i++) {
    oledMonitor.print(linea1[i]);
    oledMonitor.display();
    playSFX(1500 + random(0, 500), 20);
    delay(100); 
  }

  delay(400); 

  // Efecto máquina de escribir para la Línea 2
  oledMonitor.setCursor(40, 36);
  for(int i = 0; i < strlen(linea2); i++) {
    oledMonitor.print(linea2[i]);
    oledMonitor.display();
    playSFX(800 + random(0, 200), 20);
    delay(80);
  }

  delay(200);
  playSFX(880, 100);  delay(100);
  playSFX(1108, 100); delay(100);
  playSFX(1318, 400); delay(400);
  
  delay(800); 

  // Cierre tipo persiana hacia el centro
  for(int i = 0; i <= 32; i += 2) {
    oledMonitor.fillRect(0, 32 - i, 128, i * 2, BLACK);
    oledMonitor.display();
    delay(15);
  }
  
  stopAudio();
  delay(600); 
}

/**
 * @brief Demostración cinemática con una pelota rebotando con estela.
 */
void showBouncingBallAnimation() {
  oledMonitor.clearDisplay();
  oledMonitor.display();

  int bSize = 2; 
  float bX = 10, bY = 10; 
  float bDX = 3, bDY = 3; 
  float speedMult = 1.0;

  int tX[10], tY[10];
  for(int i = 0; i < 10; i++) { tX[i] = -1; tY[i] = -1; }
  int tIndex = 0;

  unsigned long startTime = millis();

  // Bucle de animación por 4 segundos
  while (millis() - startTime < 4000) {
    tX[tIndex] = (int)bX;
    tY[tIndex] = (int)bY;
    tIndex = (tIndex + 1) % 10;

    bX += bDX * speedMult;
    bY += bDY * speedMult;

    // Colisión bordes horizontales
    if (bX <= 0 || bX >= (SCREEN_WIDTH - bSize)) {
      bDX *= -1;
      speedMult *= 1.01; 
      playSFX(800, 15); 
    }
    // Colisión bordes verticales
    if (bY <= 0 || bY >= (SCREEN_HEIGHT - bSize)) {
      bDY *= -1;
      speedMult *= 1.01; 
      playSFX(1000, 15); 
    }

    if (millis() - startTime > 2000) { speedMult += 0.08; } 

    oledMonitor.clearDisplay(); 

    // Renderizado de la estela
    for (int i = 0; i < 10; i++) {
      if (tX[i] != -1) {
        oledMonitor.drawPixel(tX[i], tY[i], WHITE);
        oledMonitor.drawPixel(tX[i]+1, tY[i], WHITE);
        oledMonitor.drawPixel(tX[i], tY[i]+1, WHITE);
        oledMonitor.drawPixel(tX[i]+1, tY[i]+1, WHITE);
      }
    }

    oledMonitor.fillRect((int)bX, (int)bY, bSize, bSize, WHITE);
    oledMonitor.display();
    delay(10);
  }
}

/**
 * @brief Explosión gráfica radial desde el centro de la pantalla.
 */
void showCenterExplosion() {
  int centerX = SCREEN_WIDTH / 2;
  int centerY = SCREEN_HEIGHT / 2;

  oledMonitor.clearDisplay(); 
  oledMonitor.display();
  delay(100);

  oledMonitor.fillRect(centerX-1, centerY-1, 2, 2, WHITE);
  oledMonitor.display();
  delay(300); 

  // Flash blanco inicial
  oledMonitor.invertDisplay(true);
  playSFX(150, 60); 
  delay(60);
  oledMonitor.invertDisplay(false);

  int maxRadius = 45; 
  // Expansión de destellos radiales y partículas
  for (int r = 1; r < maxRadius; r += 3) {
    oledMonitor.clearDisplay(); 
    
    int offsetX = random(-3, 4); 
    int offsetY = random(-3, 4); 

    int numLines = 16;
    for (int i = 0; i < numLines; i++) {
      float angle = (i * 2 * PI) / numLines;
      int startX = centerX + offsetX + (r / 1.5) * cos(angle);
      int startY = centerY + offsetY + (r / 1.5) * sin(angle);
      int endX = centerX + offsetX + r * cos(angle);
      int endY = centerY + offsetY + r * sin(angle);
      oledMonitor.drawLine(startX, startY, endX, endY, WHITE);
    }
    
    for (int p = 0; p < 10; p++) {
      int px = centerX + offsetX + random(-r, r);
      int py = centerY + offsetY + random(-r, r);
      oledMonitor.drawPixel(px, py, WHITE);
    }

    oledMonitor.display();
    playSFX(random(40, 200), 15); 
    delay(15); 
  }

  stopAudio(); 

  // Parpadeo final post-explosión
  for(int i = 0; i < 2; i++) {
      oledMonitor.invertDisplay(true);
      delay(30);
      oledMonitor.invertDisplay(false);
      delay(30);
  }
  
  oledMonitor.clearDisplay();
  oledMonitor.display();
  delay(300);
}

/**
 * @brief Muestra el título con barra de carga retro graduada y mensaje de bienvenida.
 */
void showLoadingBarAnimation() {
  oledMonitor.setTextSize(2);
  oledMonitor.setTextColor(WHITE);
  
  // Caída vertical del título "PINPONG"
  for (int posY = -20; posY <= 4; posY += 4) {
    oledMonitor.clearDisplay();
    oledMonitor.setCursor(22, posY); 
    oledMonitor.print("PINPONG");
    oledMonitor.display();
    playSFX(400 + (posY * 10), 10);
    delay(15);
  }
  
  playSFX(120, 80);

  oledMonitor.setTextSize(1);
  oledMonitor.setCursor(34, 26);
  oledMonitor.print("LOADING...");
  oledMonitor.display();

  int barX = 14; int barY = 42;
  int barWidth = 100; int barHeight = 14;

  oledMonitor.drawRect(barX, barY, barWidth, barHeight, WHITE);
  oledMonitor.display();

  int innerPadding = 2;
  int totalFillWidth = barWidth - (innerPadding * 2);
  int numBlocks = 10;
  int blockGap = 2;
  int blockWidth = (totalFillWidth - ((numBlocks - 1) * blockGap)) / numBlocks;

  // Llenado de los bloques de la barra de carga
  for (int i = 0; i < numBlocks; i++) {
    int xPos = barX + innerPadding + i * (blockWidth + blockGap);
    int yPos = barY + innerPadding;
    int blockHeight = barHeight - (innerPadding * 2);

    oledMonitor.fillRect(xPos, yPos, blockWidth, blockHeight, WHITE);
    oledMonitor.display(); 
    playSFX(1800, 15);
    delay(100 + random(30, 150)); 
  }

  delay(300);

  playSFX(988, 100);  delay(120);
  playSFX(1319, 200); delay(200);

  // Inicialización de las estrellas del fondo
  const int NUM_STARS = 12; 
  float starX[NUM_STARS];
  int starY[NUM_STARS];
  float starSpeed[NUM_STARS];

  for(int i = 0; i < NUM_STARS; i++) {
    starX[i] = random(0, SCREEN_WIDTH);
    starY[i] = random(0, SCREEN_HEIGHT);
    starSpeed[i] = random(5, 26) / 10.0; 
  }

  unsigned long pressStartTimer = millis(); 
  unsigned long blinkTimer = millis();      
  bool showPressStart = true;               

  // Espera cinemática con estrellas en movimiento
  while (millis() - pressStartTimer < 6000) {
    oledMonitor.clearDisplay();

    for(int i = 0; i < NUM_STARS; i++) {
      starX[i] -= starSpeed[i]; 
      if (starX[i] < 0) { 
        starX[i] = SCREEN_WIDTH;
        starY[i] = random(0, SCREEN_HEIGHT);
      }
      oledMonitor.drawPixel((int)starX[i], starY[i], WHITE);
    }

    oledMonitor.setTextSize(2);
    oledMonitor.setCursor(22, 4);
    oledMonitor.print("PINPONG");

    // Parpadeo del texto
    if (millis() - blinkTimer > 400) {
      showPressStart = !showPressStart; 
      blinkTimer = millis();            
    }

    if (showPressStart) {
      oledMonitor.setTextSize(1);
      oledMonitor.setCursor(28, 40);
      oledMonitor.print("Welcome!");
    }

    oledMonitor.display();
    delay(20); 
  }

  playSFX(200, 100); delay(100);
  playSFX(150, 200); delay(100);

  // Transición de borrado estilo pixelado/mosaico
  for (int paso = 0; paso < 8; paso++) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
      for (int x = 0; x < SCREEN_WIDTH; x++) {
        if ((x + (y * 3)) % 8 == paso) {
          oledMonitor.drawPixel(x, y, BLACK);
        }
      }
    }
    oledMonitor.display();
    playSFX(random(30, 80), 30); 
    delay(40); 
  }
  
  stopAudio(); 

  oledMonitor.clearDisplay();
  oledMonitor.display();
  delay(300); 
}

// ==========================================
//   MENÚ PRINCIPAL
// ==========================================
/**
 * @brief Gestiona la navegación y renderizado del menú principal.
 */
void handleMenu() {
  // Inicializar posiciones de las estrellas si es la primera ejecución
  if (!starsInitialized) {
    for (int i = 0; i < MENU_STARS; i++) {
      mStarX[i] = random(0, SCREEN_WIDTH);
      mStarY[i] = random(12, SCREEN_HEIGHT);
      mStarSpeed[i] = random(3, 15) / 10.0;
    }
    starsInitialized = true;
  }

  int joyY = readJoystickY(); 
  
  // Navegación hacia Arriba
  if (joyY < 1000 && !joystickLocked) {
    currentMenuOption--;
    if (currentMenuOption < 0) currentMenuOption = totalMenuOptions - 1; 
    playSFX(1200, 25); 
    joystickLocked = true;
    lastActivityTime = millis(); 
  }
  // Navegación hacia Abajo
  else if (joyY > 3000 && !joystickLocked) {
    currentMenuOption++;
    if (currentMenuOption >= totalMenuOptions) currentMenuOption = 0; 
    playSFX(1000, 25); 
    joystickLocked = true;
    lastActivityTime = millis(); 
  }
  // Desbloqueo del joystick cuando vuelve al centro
  else if (joyY > 1500 && joyY < 2500) {
    joystickLocked = false; 
  }

  // Confirmación con el botón del Joystick 1
  if (digitalRead(J1_BTN_PIN) == LOW && millis() - lastBtnPress > 300) {
    lastBtnPress = millis();
    lastActivityTime = millis(); 

    playSFX(1500, 40);
    delay(40);
    playSFX(2000, 100);

    animateScreenWipe();

    // Redirección según la opción seleccionada
    if (currentMenuOption == 0) {
      currentState = STATE_MATCH_SETUP;
      currentSetupOption = 0;
    }
    if (currentMenuOption == 1) currentState = STATE_SETTINGS;
    if (currentMenuOption == 2) currentState = STATE_SCORES;
    if (currentMenuOption == 3) currentState = STATE_CREDITS;
    if (currentMenuOption == 4) currentState = STATE_SLEEP;
    
    oledMonitor.clearDisplay();
    oledMonitor.display();
    delay(150);
    return; 
  }

  oledMonitor.clearDisplay();
  headerHud(); // Dibuja la barra de estado superior (Iconos MÚSICA / SFX)

  // Desplazamiento y dibujo del fondo con estrellas
  for (int i = 0; i < MENU_STARS; i++) {
    mStarX[i] -= mStarSpeed[i];
    if (mStarX[i] < 0) {
      mStarX[i] = SCREEN_WIDTH;
      mStarY[i] = random(14, SCREEN_HEIGHT);
    }
    oledMonitor.drawPixel((int)mStarX[i], mStarY[i], WHITE);
  }

  // Título e indicador de récord
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(4, 2);
  oledMonitor.print("PINPONG");
  
  oledMonitor.setCursor(70, 2);
  oledMonitor.print("HI:");
  oledMonitor.print(highScore);
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  // Animación del puntero de selección
  int animOffset = abs((int)(millis() / 120) % 4 - 2); 

  const char* options[5] = {"JUGAR", "CONFIGURACION", "PUNTAJES", "CREDITOS", "SALIR"};
  
  // Dibujado de las opciones del menú
  for (int i = 0; i < totalMenuOptions; i++) {
    int yPos = 15 + (i * 10);
    
    if (i == currentMenuOption) {
      oledMonitor.setTextColor(WHITE);
      oledMonitor.setCursor(0 + animOffset, yPos);
      oledMonitor.print(">");

      // Opción resaltada con fondo blanco e icono/texto invertidos a negro
      oledMonitor.fillRoundRect(8, yPos - 1, 114, 9, 2, WHITE);
      
      oledMonitor.drawBitmap(12, yPos, menuIcons[i], 7, 7, BLACK);
      oledMonitor.setTextColor(BLACK); 
      oledMonitor.setCursor(23, yPos);
      oledMonitor.print(options[i]);
    } else {
      oledMonitor.drawBitmap(12, yPos, menuIcons[i], 7, 7, WHITE);
      oledMonitor.setTextColor(WHITE);
      oledMonitor.setCursor(23, yPos);
      oledMonitor.print(options[i]);
    }
  }

  // Verificación de inactividad para activar el modo demostración
  if (millis() - lastActivityTime > INACTIVITY_TIMEOUT) {
    currentState = STATE_ATTRACT;
    oledMonitor.clearDisplay();
    oledMonitor.display();
    return;
  }

  oledMonitor.display();
}

// ==========================================
//   SUBMENÚ: ESTADÍSTICAS
// ==========================================
/**
 * @brief Despliega el contador acumulado de victorias de los jugadores e IA.
 */
void handleScores() {
  oledMonitor.clearDisplay();
  headerHud();

  // Regreso al menú principal con pulsación de botón
  if (digitalRead(J1_BTN_PIN) == LOW && millis() - lastBtnPress > 300) {
    lastBtnPress = millis();
    playSFX(800, 40);
    animateScreenWipe();
    currentState = STATE_MENU;
    return;
  }

  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(24, 2);
  oledMonitor.print("ESTADISTICAS");
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  // Uso de la fuente Picopixel para mayor densidad de texto
  oledMonitor.setFont(&Picopixel);

  oledMonitor.setCursor(2, 22);
  oledMonitor.print("MODO [ 1P vs CPU ]");
  
  oledMonitor.setCursor(10, 30);
  oledMonitor.print("JUGADOR: "); oledMonitor.print(wins_1p_ia); oledMonitor.print(" WINS");
  
  oledMonitor.setCursor(10, 37);
  oledMonitor.print("CPU (IA): "); oledMonitor.print(wins_ia); oledMonitor.print(" WINS");

  oledMonitor.setCursor(2, 47);
  oledMonitor.print("MODO [ 1P vs 2P ]");
  
  oledMonitor.setCursor(10, 55);
  oledMonitor.print("JUGADOR 1: "); oledMonitor.print(wins_p1_vs); oledMonitor.print(" WINS");
  
  oledMonitor.setCursor(10, 62);
  oledMonitor.print("JUGADOR 2: "); oledMonitor.print(wins_p2_vs); oledMonitor.print(" WINS");

  oledMonitor.setFont(); // Restaura la fuente por defecto
  oledMonitor.display();
}

// ==========================================
//   SUBMENÚ: CRÉDITOS
// ==========================================
/**
 * @brief Muestra la pantalla de información de los creadores.
 */
void handleCredits() {
  oledMonitor.clearDisplay();
  headerHud();

  if (digitalRead(J1_BTN_PIN) == LOW && millis() - lastBtnPress > 300) {
    lastBtnPress = millis();
    playSFX(800, 40);
    animateScreenWipe();
    currentState = STATE_MENU;
    return;
  }

  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(36, 2);
  oledMonitor.print("CREDITOS");
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  oledMonitor.setCursor(20, 20);
  oledMonitor.print("ARCADE STUDIO");
  oledMonitor.setCursor(20, 32);
  oledMonitor.print("PINPONG ESP32");
  oledMonitor.setCursor(20, 44);
  oledMonitor.print("Diego Y Edison");

  oledMonitor.setCursor(10, 56);
  oledMonitor.print("PULSA BTN SALIR");

  oledMonitor.display();
}

// ==========================================
//   SUBMENÚ: PREPARACIÓN DE PARTIDA
// ==========================================
/**
 * @brief Menú para configurar Modo de juego, Dificultad e Historial de Puntos.
 */
void handleMatchSetup() {
  const int totalSetupOptions = 5;

  int joyY = readJoystickY();
  int joyX = analogRead(J1_X_PIN);

  // Navegación Vertical
  if (joyY < 1000 && !joystickLocked) {
    currentSetupOption--;
    if (currentSetupOption < 0) currentSetupOption = totalSetupOptions - 1;
    playSFX(1200, 15);
    joystickLocked = true;
    lastActivityTime = millis();
  }
  else if (joyY > 3000 && !joystickLocked) {
    currentSetupOption++;
    if (currentSetupOption >= totalSetupOptions) currentSetupOption = 0;
    playSFX(1000, 15);
    joystickLocked = true;
    lastActivityTime = millis();
  }

  // Navegación Horizontal (Ajuste rápido de parámetros con Eje X)
  if ((joyX < 1000 || joyX > 3000) && !joystickLocked) {
    bool moveRight = (joyX > 3000);
    playSFX(1400, 20);
    lastActivityTime = millis();

    switch (currentSetupOption) {
      case 0: // Alterna Modo: 1P vs CPU o 2P VS
        gameMode = (gameMode == 0) ? 1 : 0;
        break;
      case 1: // Cambia dificultad de la IA (Fácil, Normal, Difícil)
        if (moveRight) gameDifficulty = (gameDifficulty + 1) % 3;
        else gameDifficulty = (gameDifficulty == 0) ? 2 : gameDifficulty - 1;
        saveSettings();
        break;
      case 2: // Modifica límite de puntos (3, 5, 10)
        if (scoreLimit == 3) scoreLimit = 5;
        else if (scoreLimit == 5) scoreLimit = 10;
        else scoreLimit = 3;
        break;
    }
    joystickLocked = true;
  }

  // Liberación del antirebote
  if (joyY > 1500 && joyY < 2500 && joyX > 1500 && joyX < 2500) {
    joystickLocked = false;
  }

  // Confirmación con Botón
  if (digitalRead(J1_BTN_PIN) == LOW && millis() - lastBtnPress > 300) {
    lastBtnPress = millis();
    lastActivityTime = millis();

    if (currentSetupOption == 0) {
      gameMode = (gameMode == 0) ? 1 : 0;
      playSFX(1400, 20);
    }
    else if (currentSetupOption == 1) {
      gameDifficulty = (gameDifficulty + 1) % 3;
      playSFX(1400, 20);
      saveSettings();
    }
    else if (currentSetupOption == 2) {
      if (scoreLimit == 3) scoreLimit = 5;
      else if (scoreLimit == 5) scoreLimit = 10;
      else scoreLimit = 3;
      playSFX(1400, 20);
    }
    else if (currentSetupOption == 3) { // INICIAR PARTIDA
      playSFX(1800, 80);
      delay(80);
      playSFX(2200, 150);
      animateScreenWipe();
      stopAudio(); 

      // INICIALIZACIÓN COMPLETA DE VARIABLES DEL JUEGO
      scoreP1 = 0; scoreP2 = 0;
      paddle1Y = 24; paddle2Y = 24;
      lastPaddle1Y = paddle1Y; lastPaddle2Y = paddle2Y;
      paddle1Velocity = 0.0; paddle2Velocity = 0.0;
      consecutiveHits = 0; currentSpeedLevel = 1;
      resetMainBall(1.0);
      scheduleArcadeTimers();
      lastPhysicsUpdate = millis();

      currentState = STATE_PLAY;
      return;
    }
    else if (currentSetupOption == 4) { // CANCELAR
      playSFX(800, 40);
      animateScreenWipe();
      currentState = STATE_MENU;
      return;
    }
  }

  oledMonitor.clearDisplay();
  headerHud();
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(18, 2);
  oledMonitor.print("NUEVA PARTIDA");
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  const char* diffLabels[3] = {"FACIL", "NORMAL", "DIFICIL"};
  int animOffset = abs((int)(millis() / 120) % 4 - 2);

  // Renderizado de las filas de opciones
  for (int i = 0; i < totalSetupOptions; i++) {
    int yPos = 14 + (i * 10);

    if (i == currentSetupOption) {
      oledMonitor.setCursor(0 + animOffset, yPos);
      oledMonitor.print(">");
    }

    if (i == 0) {
      oledMonitor.setCursor(10, yPos);
      oledMonitor.print("MODO:");
      oledMonitor.setCursor(80, yPos);
      oledMonitor.print(gameMode == 0 ? "[1P IA]" : "[2P VS]");
    }
    else if (i == 1) {
      oledMonitor.setCursor(10, yPos);
      oledMonitor.print("DIFICULTAD:");
      oledMonitor.setCursor(80, yPos);
      if (gameMode == 1) oledMonitor.print("[---]"); // Deshabilitado en Modo 2P
      else oledMonitor.print(diffLabels[gameDifficulty]);
    }
    else if (i == 2) {
      oledMonitor.setCursor(10, yPos);
      oledMonitor.print("PUNTOS:");
      oledMonitor.setCursor(80, yPos);
      oledMonitor.print("[");
      oledMonitor.print(scoreLimit);
      oledMonitor.print(" PTS]");
    }
    else if (i == 3) {
      if (i == currentSetupOption) {
        oledMonitor.fillRect(10, yPos - 1, 108, 9, WHITE);
        oledMonitor.setTextColor(BLACK);
        oledMonitor.setCursor(18, yPos);
        oledMonitor.print("! EMPEZAR JUEGO !");
        oledMonitor.setTextColor(WHITE);
      } else {
        oledMonitor.setCursor(18, yPos);
        oledMonitor.print("[ EMPEZAR JUEGO ]");
      }
    }
    else if (i == 4) {
      oledMonitor.setCursor(10, yPos);
      oledMonitor.print("< CANCELAR");
    }
  }

  oledMonitor.display();
}

// ==========================================
//   SUBMENÚ: CONFIGURACIÓN DEL SISTEMA
// ==========================================
/**
 * @brief Permite ajustar parámetros de hardware y sonido (guardados en NVS).
 */
void handleSettings() {
  const int totalSettingsOptions = 5; 

  int joyY = readJoystickY();
  int joyX = analogRead(J1_X_PIN);

  // Selección de opción vertical
  if (joyY < 1000 && !joystickLocked) {
    currentSettingOption--;
    if (currentSettingOption < 0) currentSettingOption = totalSettingsOptions - 1;
    playSFX(1200, 15);
    joystickLocked = true;
    lastActivityTime = millis();
  }
  else if (joyY > 3000 && !joystickLocked) {
    currentSettingOption++;
    if (currentSettingOption >= totalSettingsOptions) currentSettingOption = 0;
    playSFX(1000, 15);
    joystickLocked = true;
    lastActivityTime = millis();
  }

  // Ajuste fino del nivel de brillo con el eje horizontal
  if ((joyX < 1000 || joyX > 3000) && !joystickLocked && currentSettingOption == 3) {
    if (joyX > 3000 && screenBrightness < 255) screenBrightness += 15;
    else if (joyX < 1000 && screenBrightness > 0) screenBrightness -= 15;
    
    screenBrightness = constrain(screenBrightness, 0, 255);
    setOledBrightness(screenBrightness);
    saveSettings(); 
    playSFX(1600, 10);
    joystickLocked = true;
    lastActivityTime = millis();
  }

  if (joyY > 1500 && joyY < 2500 && joyX > 1500 && joyX < 2500) {
    joystickLocked = false;
  }

  // Alternar estados mediante pulsación de botón
  if (digitalRead(J1_BTN_PIN) == LOW && millis() - lastBtnPress > 300) {
    lastBtnPress = millis();
    lastActivityTime = millis();

    if (currentSettingOption == 0) { // MÚSICA
      musicEnabled = !musicEnabled;
      if (!musicEnabled) stopAudio(); 
      else playSFX(1400, 30);
      saveSettings();
    }
    else if (currentSettingOption == 1) { // EFECTOS SFX
      sfxEnabled = !sfxEnabled;
      playSFX(1400, 30);
      saveSettings();
    }
    else if (currentSettingOption == 2) { // INVERTIR CONTROLES
      invertControls = !invertControls;
      playSFX(1400, 30);
      saveSettings();
    }
    else if (currentSettingOption == 4) { // VOLVER
      playSFX(800, 40);
      animateScreenWipe();
      currentState = settingsReturnState; // Regresa al menú origen (Pausa o Menú Principal)
      settingsReturnState = STATE_MENU;
      return;
    }
  }

  oledMonitor.clearDisplay();
  headerHud();

  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(24, 2);
  oledMonitor.print("CONFIGURACION");
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  int animOffset = abs((int)(millis() / 120) % 4 - 2); 

  const char* settingNames[5] = {"MUSICA", "EFECTOS SFX", "INVERTIR Y", "BRILLO", "< VOLVER"};

  for (int i = 0; i < totalSettingsOptions; i++) {
    int yPos = 14 + (i * 10);

    if (i == currentSettingOption) {
      oledMonitor.setCursor(0 + animOffset, yPos);
      oledMonitor.print(">");
    }

    oledMonitor.setCursor(10, yPos);
    oledMonitor.print(settingNames[i]);

    oledMonitor.setCursor(88, yPos);
    if (i == 0) {
      oledMonitor.print(musicEnabled ? "[ON]" : "[OFF]");
    }
    else if (i == 1) {
      oledMonitor.print(sfxEnabled ? "[ON]" : "[OFF]");
    }
    else if (i == 2) {
      oledMonitor.print(invertControls ? "[SI]" : "[NO]");
    }
    else if (i == 3) { // Barra gráfica de brillo
      oledMonitor.drawRect(86, yPos, 30, 7, WHITE);
      oledMonitor.fillRect(86, yPos, map(screenBrightness, 0, 255, 0, 30), 7, WHITE);
    }
  }

  oledMonitor.display();
}

// ==========================================
//   SECUENCIA DE APAGADO (DEEP SLEEP)
// ==========================================
/**
 * @brief Ejecuta el apagado cinemático e ingresa al estado de suspensión profunda (Deep Sleep).
 */
void handleSleep() {
  oledMonitor.clearDisplay();
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(28, 28);
  oledMonitor.print("SYSTEM OFF...");
  oledMonitor.display();
  
  // Tono decreciente de apagado
  playSFX(1000, 100); delay(120);
  playSFX(800, 100);  delay(120);
  playSFX(500, 300);  delay(350);
  stopAudio();

  // Cierre de pantalla vertical tipo TV de tubo CRT
  for (int h = 32; h > 0; h -= 2) {
    oledMonitor.clearDisplay();
    oledMonitor.drawRect(0, 32 - h, 128, h * 2, WHITE);
    oledMonitor.display();
    delay(10);
  }

  // Punto blanco central de colapso
  oledMonitor.clearDisplay();
  oledMonitor.fillRect(63, 31, 2, 2, WHITE);
  oledMonitor.display();
  delay(300);

  oledMonitor.clearDisplay();
  oledMonitor.display();

  // Configura el pin del botón J1 para despertar al ESP32 del modo Deep Sleep cuando pase a nivel LOW (0)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)J1_BTN_PIN, 0); 
  esp_deep_sleep_start();
}

// ==========================================
//   MODO DEMOSTRACIÓN (ATTRACT MODE)
// ==========================================
/**
 * @brief Modo libre en el que la CPU controla ambas paletas automáticamente tras inactividad.
 */
void handleAttractMode() {
  static float demoX = 64, demoY = 32;
  static float demoDX = 2.5, demoDY = 1.8;
  static float demoP1Y = 24, demoP2Y = 24;
  const int pH = 14, pW = 2;
  static unsigned long blinkTimer = 0;
  static bool showText = true;

  int joyY = readJoystickY();
  bool btnPressed = (digitalRead(J1_BTN_PIN) == LOW);

  // Al presionar cualquier control, se sale del modo Attract Mode y regresa al Menú
  if (joyY < 1000 || joyY > 3000 || btnPressed) {
    lastActivityTime = millis(); 
    currentState = STATE_MENU;
    musicIndex = 0;
    nextNoteTime = millis();
    playSFX(800, 30);
    delay(150); 
    return;
  }

  // Desplazamiento de la pelota demo
  demoX += demoDX;
  demoY += demoDY;

  // Seguimiento automatizado de las paletas virtuales
  if (demoX < 70) {
    if (demoP1Y + (pH / 2) < demoY) demoP1Y += 1.5;
    if (demoP1Y + (pH / 2) > demoY) demoP1Y -= 1.5;
  }

  if (demoX > 58) {
    if (demoP2Y + (pH / 2) < demoY) demoP2Y += 1.5;
    if (demoP2Y + (pH / 2) > demoY) demoP2Y -= 1.5;
  }

  demoP1Y = constrain(demoP1Y, 0, SCREEN_HEIGHT - pH);
  demoP2Y = constrain(demoP2Y, 0, SCREEN_HEIGHT - pH);

  // Rebotes de bordes verticales
  if (demoY <= 0 || demoY >= SCREEN_HEIGHT - 2) {
    demoDY *= -1;
    playSFX(600, 10);
  }

  // Rebote paleta izquierda
  if (demoX <= (4 + pW) && demoY >= demoP1Y && demoY <= demoP1Y + pH) {
    demoDX *= -1;
    demoX = 4 + pW + 1;
    playSFX(900, 15);
  }

  // Rebote paleta derecha
  if (demoX >= (124 - pW) && demoY >= demoP2Y && demoY <= demoP2Y + pH) {
    demoDX *= -1;
    demoX = 124 - pW - 1;
    playSFX(900, 15);
  }

  // Reinicio si la pelota se sale
  if (demoX < 0 || demoX > SCREEN_WIDTH) {
    demoX = 64; demoY = 32;
    demoDX = (random(0, 2) == 0 ? 2.5 : -2.5);
  }

  oledMonitor.clearDisplay();

  // Línea discontinua central
  for (int y = 0; y < SCREEN_HEIGHT; y += 6) {
    oledMonitor.drawFastVLine(64, y, 3, WHITE);
  }

  oledMonitor.fillRect(4, (int)demoP1Y, pW, pH, WHITE);
  oledMonitor.fillRect(124 - pW, (int)demoP2Y, pW, pH, WHITE);
  oledMonitor.fillRect((int)demoX, (int)demoY, 2, 2, WHITE);

  // Texto parpadeante "PRESS ANY BUTTON"
  if (millis() - blinkTimer > 500) {
    showText = !showText;
    blinkTimer = millis();
  }

  if (showText) {
    oledMonitor.fillRect(14, 26, 100, 12, BLACK); 
    oledMonitor.drawRect(14, 26, 100, 12, WHITE);
    oledMonitor.setTextSize(1);
    oledMonitor.setTextColor(WHITE);
    oledMonitor.setCursor(18, 28);
    oledMonitor.print("PRESS ANY BUTTON");
  }

  oledMonitor.display();
  delay(15); 
}

// ==========================================
//   SPLASH SCREEN
// ==========================================
/**
 * @brief Muestra la pantalla inicial con el título "PINPONG RETRO CONSOLE".
 */
void handleSplashScreen() {
  for (int y = -16; y <= 6; y += 2) {
    oledMonitor.clearDisplay();
    oledMonitor.setTextSize(2);
    oledMonitor.setTextColor(WHITE);
    oledMonitor.setCursor(22, y);
    oledMonitor.print("PINPONG");
    oledMonitor.display();
    delay(10);
  }

  playSFX(987, 80);   delay(90);
  playSFX(1318, 220); delay(220);

  oledMonitor.drawFastHLine(14, 25, 100, WHITE);
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(26, 31);
  oledMonitor.print("RETRO CONSOLE");

  oledMonitor.setCursor(31, 44);
  oledMonitor.print("CARGANDO...");

  oledMonitor.drawRect(24, 55, 80, 6, WHITE); 
  
  // Carga de barra horizontal
  for (int w = 0; w <= 76; w += 4) {
    oledMonitor.fillRect(26, 57, w, 2, WHITE); 
    oledMonitor.display();
    delay(25); 
  }

  delay(400); 

  currentState = STATE_MENU;
  musicIndex = 0;
  nextNoteTime = millis();
  lastActivityTime = millis(); 
}

/**
 * @brief Animación de barrido blanco lateral para transiciones limpias entre pantallas.
 */
void animateScreenWipe() {
  for (int x = 0; x <= 128; x += 10) {
    oledMonitor.fillRect(0, 0, x, 64, WHITE);
    oledMonitor.display();
    delay(5); 
  }
  
  delay(40); 
  oledMonitor.clearDisplay();
  oledMonitor.display();
}

// ==========================================
//   NVS FLASH & HARDWARE
// ==========================================
/**
 * @brief Carga todos los ajustes del usuario guardados en el espacio NVS "arcade".
 */
void loadSettings() {
  prefs.begin("arcade", true); // Abre en modo solo lectura (true)

  musicEnabled     = prefs.getBool("music", true);
  sfxEnabled       = prefs.getBool("sfx", true);
  invertControls   = prefs.getBool("invert", false);
  gameDifficulty   = prefs.getInt("diff", 1);
  highScore        = prefs.getInt("hscore", 0);
  screenBrightness = prefs.getInt("bright", 255);

  wins_1p_ia = prefs.getInt("w_1pia", 0);
  wins_ia    = prefs.getInt("w_ia", 0);
  wins_p1_vs = prefs.getInt("w_p1vs", 0);
  wins_p2_vs = prefs.getInt("w_p2vs", 0);
  
  prefs.end(); // Cierra el espacio de nombres
  setOledBrightness(screenBrightness);
}

/**
 * @brief Guarda los ajustes y variables de contadores en la memoria Flash NVS.
 */
void saveSettings() {
  prefs.begin("arcade", false); // Abre en modo lectura/escritura (false)

  prefs.putBool("music", musicEnabled);
  prefs.putBool("sfx", sfxEnabled);
  prefs.putBool("invert", invertControls);
  prefs.putInt("diff", gameDifficulty);
  prefs.putInt("bright", screenBrightness);

  prefs.putInt("w_1pia", wins_1p_ia);
  prefs.putInt("w_ia", wins_ia);
  prefs.putInt("w_p1vs", wins_p1_vs);
  prefs.putInt("w_p2vs", wins_p2_vs);

  prefs.end();
}

/**
 * @brief Evalúa si el puntaje actual supera al récord histórico registrado y lo almacena.
 * @param currentScore Puntuación a evaluar.
 */
void checkAndSaveHighScore(int currentScore) {
  if (currentScore > highScore) {
    highScore = currentScore;
    prefs.begin("arcade", false);
    prefs.putInt("hscore", highScore);
    prefs.end();
  }
}

/**
 * @brief Envía el comando de control de contraste I2C directo al chip SH1106 para modificar el brillo.
 * @param brightness Valor de brillo de 0 a 255.
 */
void setOledBrightness(uint8_t brightness) {
  Wire.beginTransmission(OLED_ADDRESS);
  Wire.write(0x00);         // Byte de control de comandos
  Wire.write(0x81);         // Comando SH1106: Ajustar Contraste/Brillo
  Wire.write(brightness);   // Valor del contraste
  Wire.endTransmission();
}

/**
 * @brief Renderiza los mini iconos de audio en la esquina superior derecha del HUD.
 */
void headerHud() {
  oledMonitor.drawBitmap(102, 2, hud_music_on, 7, 7, WHITE);
  if (!musicEnabled) { 
    oledMonitor.drawBitmap(102, 2, hud_off_mark, 7, 7, BLACK); 
    oledMonitor.drawLine(102, 2, 108, 8, WHITE); 
  }

  oledMonitor.drawBitmap(114, 2, hud_sfx_on, 7, 7, WHITE);
  if (!sfxEnabled) {
    oledMonitor.drawBitmap(114, 2, hud_off_mark, 7, 7, BLACK);
    oledMonitor.drawLine(114, 2, 120, 8, WHITE); 
  }
}

// ==========================================
//   MOTOR PRINCIPAL DEL JUEGO (STATE_PLAY)
// ==========================================
/**
 * @brief Núcleo ejecutor del gameplay: Física, colisiones, eventos, renderizado y reglas de puntuación.
 */
void handlePlay() {
  oledMonitor.clearDisplay();

  // --- BOTÓN DE PAUSA (Detección por tiempo mantenido) ---
  if (digitalRead(J1_BTN_PIN) == LOW) {
    if (!isPauseButtonPressed) {
      isPauseButtonPressed = true;
      pauseButtonTimer = millis(); 
    } 
    else if (millis() - pauseButtonTimer > 1600) { // Si se mantiene presionado por más de 1.6s
      isPauseButtonPressed = true; 
      pauseOption = 0;             
      currentState = STATE_PAUSE;  
      stopAudio();                 
      return;                      
    }
  } else {
    isPauseButtonPressed = false; 
  }

  // --- TEMBLOR DE PANTALLA (Screen Shake) ---
  int shakeOffset = 0;
  if (shakeFrames > 0) {
    shakeOffset = random(-2, 3);
    shakeFrames--; 
  }

  // 1. DIBUJAR LA CANCHA DE JUEGO
  // Delimitación del campo debajo del marco del HUD (Y=12)
  for (int y = 13; y < SCREEN_HEIGHT; y += 6) {
    oledMonitor.drawFastVLine(64, y + shakeOffset, 3, WHITE); // Línea central
  }
  oledMonitor.drawFastHLine(0, 12 + shakeOffset, SCREEN_WIDTH, WHITE); // Borde superior
  oledMonitor.drawFastHLine(0, SCREEN_HEIGHT - 1 + shakeOffset, SCREEN_WIDTH, WHITE); // Borde inferior

  // Destello en paredes tras un impacto fuerte
  if (wallFlashFrames > 0) {
    int age = 5 - wallFlashFrames;
    int flashY = wallFlashY + shakeOffset;
    int radius = 2 + age * 2;
    int startX = max(0, wallFlashX - radius);
    int endX = min(SCREEN_WIDTH - 1, wallFlashX + radius);
    oledMonitor.drawFastHLine(startX, flashY, endX - startX + 1, WHITE);
    if (age >= 1) {
      int y2 = flashY + (wallImpactDirection < 0 ? age : -age);
      if (y2 >= 12 && y2 < SCREEN_HEIGHT) oledMonitor.drawFastHLine(max(0, wallFlashX - radius + 2), y2, max(1, radius * 2 - 2), WHITE);
    }
    wallFlashFrames--;
  }

  // Indicadores visuales en pantalla por jugadas especiales
  if (perfectHitTimer > millis()) {
    oledMonitor.setCursor(43, 14);
    oledMonitor.print("PERFECT!");
  }
  if (nearMissTimer > millis()) {
    oledMonitor.setCursor(47, 54);
    oledMonitor.print("CERCA!");
  }

  // 2. MARCADORES E INDICADOR DE PODERES/NIVEL
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(20, 2 + shakeOffset); oledMonitor.print(scoreP1);
  oledMonitor.setCursor(102, 2 + shakeOffset); oledMonitor.print(scoreP2);
  
  if (activePowerUpType != -1) {
    oledMonitor.setCursor(40, 2 + shakeOffset);
    if (activePowerUpType == 0) oledMonitor.print("GIGANTE");
    else if (activePowerUpType == 1) oledMonitor.print("ENCOGER");
    else if (activePowerUpType == 2) oledMonitor.print("FUEGO");
    else if (activePowerUpType == 3) oledMonitor.print("FANTASMA");
    else if (activePowerUpType == 4) oledMonitor.print("INVERTIR");
  } else {
    oledMonitor.setCursor(50, 2 + shakeOffset);
    oledMonitor.print("LVL "); oledMonitor.print(currentSpeedLevel);
  }

  if (consecutiveHits >= 3) {
    oledMonitor.setCursor(50, 54);
    if (consecutiveHits >= 15) oledMonitor.print("FEVER ");
    else oledMonitor.print("RALLY ");
    oledMonitor.print(consecutiveHits);
  }

  // 3. MOVIMIENTO DE JUGADORES (Proporcional al tiempo con deltaTime / physicsScale)
  unsigned long now = millis();
  float physicsScale = 1.0;
  if (lastPhysicsUpdate != 0) {
    float elapsed = (float)(now - lastPhysicsUpdate);
    physicsScale = constrain(elapsed / FRAME_REFERENCE_MS, 0.45f, 1.35f);
  }
  lastPhysicsUpdate = now;

  // Actualización de la paleta del Jugador 1
  int joy1Y = readJoystickY();
  lastPaddle1Y = paddle1Y;
  updatePlayerPaddle(joy1Y, paddle1Y, currentPaddle1_H, invertedControlsP1, physicsScale);
  paddle1Velocity = (paddle1Y - lastPaddle1Y) / max(physicsScale, 0.01f);

  // Actualización de la paleta del Jugador 2 o IA
  int joy2Y = analogRead(J2_Y_PIN);
  if (gameMode == 1) { // Modo 2P VS
    if (invertControls) joy2Y = JOY_MAX - joy2Y;
    lastPaddle2Y = paddle2Y;
    updatePlayerPaddle(joy2Y, paddle2Y, currentPaddle2_H, invertedControlsP2, physicsScale);
    paddle2Velocity = (paddle2Y - lastPaddle2Y) / max(physicsScale, 0.01f);
  } else { // Modo 1P vs CPU
    lastPaddle2Y = paddle2Y;
    updateCpuPaddle(physicsScale);
    paddle2Velocity = (paddle2Y - lastPaddle2Y) / max(physicsScale, 0.01f);
  }

  // Delimitación de movimiento dentro de las paredes verticales
  paddle1Y = constrain(paddle1Y, (float)COURT_TOP_Y, (float)(COURT_BOTTOM_Y - currentPaddle1_H));
  paddle2Y = constrain(paddle2Y, (float)COURT_TOP_Y, (float)(COURT_BOTTOM_Y - currentPaddle2_H));

  // 4. GENERACIÓN Y LÓGICA DE CAJAS MISTERIOSAS (POWER-UPS)
  if (!powerUpActive && activePowerUpType == -1 && millis() - powerUpSpawnTimer > powerUpSpawnDelay) {
    powerUpActive = true;
    powerUpX = random(30, 90); 
    powerUpY = random(COURT_TOP_Y + 2, COURT_BOTTOM_Y - 8);
    powerUpType = random(0, 5);
  }

  if (powerUpActive) {
    // Dibujo parpadeante de la caja mística
    if ((millis() / 150) % 2 == 0) {
      oledMonitor.drawRect((int)powerUpX, (int)powerUpY + shakeOffset, 6, 6, WHITE);
      oledMonitor.drawPixel((int)powerUpX + 2, (int)powerUpY + 2 + shakeOffset, WHITE);
    }
    
    // Verificación de colisión de las pelotas con el Power-Up
    for (int b = 0; b < 2; b++) {
      if (!balls[b].active) continue;
      if (balls[b].x + BALL_SIZE >= powerUpX && balls[b].x <= powerUpX + 6 && 
          balls[b].y + BALL_SIZE >= powerUpY && balls[b].y <= powerUpY + 6) {
          
          if (balls[b].lastHitBy > 0) { // Solo si la pelota fue golpeada por un jugador previamente
            powerUpActive = false;
            playSFX(1800, 150);
            activePowerUpPlayer = balls[b].lastHitBy;
            activePowerUpType = powerUpType;
            powerUpDurationTimer = millis();
            powerUpSpawnTimer = millis();
            powerUpSpawnDelay = random(10000, 15001);

            // Aplicación de efectos según el tipo
            if (powerUpType == 0) { // GIGANTE
              if (activePowerUpPlayer == 1) currentPaddle1_H = PADDLE_H * 2; else currentPaddle2_H = PADDLE_H * 2;
            } else if (powerUpType == 1) { // ENCOGER AL RIVAL
              if (activePowerUpPlayer == 1) currentPaddle2_H = PADDLE_H / 2; else currentPaddle1_H = PADDLE_H / 2;
            } else if (powerUpType == 2) { // BOLA DE FUEGO
              shakeFrames = 10;
              balls[b].dx = (activePowerUpPlayer == 1) ? BALL_MAX_SPEED : -BALL_MAX_SPEED;
            } else if (powerUpType == 3) { // BOLA FANTASMA
              ghostBallActive = true;
            } else if (powerUpType == 4) { // INVERTIR CONTROLES DEL RIVAL
              if (activePowerUpPlayer == 1) invertedControlsP2 = true; else invertedControlsP1 = true;
            }
          }
          break;
      }
    }
  }

  // Expiración del efecto de Power-Up tras 5 segundos
  if (activePowerUpType != -1 && millis() - powerUpDurationTimer > 5000) {
      currentPaddle1_H = PADDLE_H; currentPaddle2_H = PADDLE_H;
      ghostBallActive = false;
      invertedControlsP1 = false; invertedControlsP2 = false;
      activePowerUpType = -1;
      powerUpSpawnTimer = millis();
      powerUpSpawnDelay = random(10000, 15001);
      playSFX(400, 100); 
  }

  // 5. ZONA DE PORTALES DE TELETRANSPORTACIÓN
  if (!activePortal.active && millis() - nextPortalTimer > portalSpawnDelay) {
    activePortal.active = true;
    activePortal.x = random(52, 72);
    activePortal.w = 6;
    activePortal.h = 16;
    activePortal.y = random(COURT_TOP_Y + 1, COURT_BOTTOM_Y - activePortal.h - 1);
    activePortal.spawnTime = millis();
  }

  if (activePortal.active) {
    if (millis() - activePortal.spawnTime > 6000) {
      activePortal.active = false;
      nextPortalTimer = millis();
      portalSpawnDelay = random(8000, 15001);
    } else {
      if ((millis() / 100) % 2 == 0) {
        oledMonitor.drawRect(activePortal.x, activePortal.y + shakeOffset, activePortal.w, activePortal.h, WHITE);
      }
    }
  }

  // 6. GENERAR MULTIPELOTA SI HAY RALLY LARGO
  if (!balls[1].active && consecutiveHits > 6 && random(0, 100) < 5) {
    spawnSecondaryBall();
  }

  // 7. DIBUJAR ESTELA DE LA PELOTA PRINCIPAL
  if (balls[0].active) {
    trailX[2] = trailX[1]; trailY[2] = trailY[1];
    trailX[1] = trailX[0]; trailY[1] = trailY[0];
    trailX[0] = balls[0].x; trailY[0] = balls[0].y;

    if ((currentSpeedLevel > 1 || abs(balls[0].spin) > 0.05) && (!ghostBallActive || (millis() / 200) % 2 == 0)) {
      oledMonitor.drawPixel((int)trailX[0], (int)trailY[0] + shakeOffset, WHITE); 
      oledMonitor.drawPixel((int)trailX[1], (int)trailY[1] + shakeOffset, WHITE); 
      oledMonitor.drawPixel((int)trailX[2], (int)trailY[2] + shakeOffset, WHITE);
      if (currentSpeedLevel == 3 || fabs(balls[0].dx) > 4.5) {
        oledMonitor.drawPixel((int)trailX[1] - 1, (int)trailY[1] + shakeOffset, WHITE);
        oledMonitor.drawPixel((int)trailX[1] + 1, (int)trailY[1] + shakeOffset, WHITE);
      }
    }
  }

  // 8. DIBUJAR PALETAS
  oledMonitor.fillRect(4, (int)paddle1Y + shakeOffset, PADDLE_W, currentPaddle1_H, WHITE);
  oledMonitor.fillRect(124 - PADDLE_W, (int)paddle2Y + shakeOffset, PADDLE_W, currentPaddle2_H, WHITE);

  // 9. BUCLE DE FÍSICA Y COLISIONES PARA LAS PELOTAS ACTIVAS
  int activeBallsCount = 0;
  for (int i = 0; i < 2; i++) {
    if (balls[i].active) activeBallsCount++;
  }

  for (int i = 0; i < 2; i++) {
    if (!balls[i].active) continue;

    // Actualización de posición aplicando efecto / spin
    float previousX = balls[i].x;
    balls[i].dy += balls[i].spin * physicsScale;
    balls[i].dy = constrain(balls[i].dy, -MAX_VERTICAL_SPEED, MAX_VERTICAL_SPEED);
    balls[i].x += balls[i].dx * physicsScale;
    balls[i].y += balls[i].dy * physicsScale;

    // Colisión de la pelota con el Portal de teletransportación
    if (activePortal.active && 
        balls[i].x + BALL_SIZE >= activePortal.x && balls[i].x <= activePortal.x + activePortal.w &&
        balls[i].y + BALL_SIZE >= activePortal.y && balls[i].y <= activePortal.y + activePortal.h) {
      
      balls[i].dx *= 1.20; // Aceleración extra
      balls[i].dx = constrain(balls[i].dx, -BALL_MAX_SPEED, BALL_MAX_SPEED);
      balls[i].y = random(COURT_TOP_Y + 2, COURT_BOTTOM_Y - BALL_SIZE - 1); 
      activePortal.active = false;
      nextPortalTimer = millis();
      portalSpawnDelay = random(8000, 15001);
      shakeFrames = 6;
      playSFX(2100, 100);
    }

    // Colisión con Pared Superior e Inferior
    const float BALL_TOP = COURT_TOP_Y;
    const float BALL_BOTTOM = COURT_BOTTOM_Y - BALL_SIZE;
    if (balls[i].y <= BALL_TOP) {
      balls[i].y = BALL_TOP;
      balls[i].dy = fabs(balls[i].dy);
      balls[i].spin *= -0.5; // Reduce la curva tras el rebote
      wallFlashX = (int)balls[i].x + BALL_SIZE / 2;
      wallFlashY = 12;
      wallFlashFrames = 5;
      wallImpactDirection = -1;
      wallImpactPower = constrain((int)(fabs(balls[i].dy) * 2.0), 1, 3);
      shakeFrames = max(shakeFrames, wallImpactPower >= 3 ? 3 : 1);
      playSFX(520 + wallImpactPower * 90, 22 + wallImpactPower * 5);
    } else if (balls[i].y >= BALL_BOTTOM) {
      balls[i].y = BALL_BOTTOM;
      balls[i].dy = -fabs(balls[i].dy);
      balls[i].spin *= -0.5;
      wallFlashX = (int)balls[i].x + BALL_SIZE / 2;
      wallFlashY = SCREEN_HEIGHT - 1;
      wallFlashFrames = 5;
      wallImpactDirection = 1;
      wallImpactPower = constrain((int)(fabs(balls[i].dy) * 2.0), 1, 3);
      shakeFrames = max(shakeFrames, wallImpactPower >= 3 ? 3 : 1);
      playSFX(520 + wallImpactPower * 90, 22 + wallImpactPower * 5);
    }

    // Colisión con Paleta del Jugador 1 (Izquierda)
    const float P1_RIGHT = 4 + PADDLE_W;
    if (balls[i].dx < 0 &&
        balls[i].x <= P1_RIGHT &&
        previousX >= P1_RIGHT &&
        balls[i].y + BALL_SIZE >= paddle1Y &&
        balls[i].y <= paddle1Y + currentPaddle1_H) {
      float hitPoint = (balls[i].y + (BALL_SIZE / 2.0)) - (paddle1Y + (currentPaddle1_H / 2.0));
      lastPlayerToHit = 1;
      balls[i].lastHitBy = 1;
      
      // Aplicar spin/efecto según el movimiento del joystick
      if (joy1Y < 1000) balls[i].spin = -0.025;
      else if (joy1Y > 3000) balls[i].spin = 0.025;
      else balls[i].spin = 0.0;

      consecutiveHits++;
      if (consecutiveHits % HITS_FOR_NEXT_LEVEL == 0 && currentSpeedLevel < 3) {
        currentSpeedLevel++; playSFX(1500, 100);
      }
      if (currentSpeedLevel == 3) shakeFrames = 5;

      float speed = getCurrentBallSpeed();

      // Incremento de velocidad acumulativa por rally
      int rallyHits = min(consecutiveHits, RALLY_BONUS_CAP);
      speed *= (1.0 + rallyHits * RALLY_SPEED_BONUS);

      // Evaluación de Golpe Perfecto (Centro de la paleta)
      float normalizedHit1 = fabs(hitPoint) / max(1.0f, currentPaddle1_H / 2.0f);
      if (normalizedHit1 < 0.18f) {
        perfectHitTimer = millis() + 650;
        perfectHitPlayer = 1;
        speed *= 1.08f;
        shakeFrames = max(shakeFrames, 3);
        playSFX(2000 + min(consecutiveHits, 10) * 35, 55);
      } else if (abs(hitPoint) > (currentPaddle1_H / 3.0)) {
        speed *= 1.10;
        shakeFrames = 4;
        playSFX(1800, 30);
      }
      speed = constrain(speed, 1.5, BALL_MAX_SPEED);

      balls[i].dx = speed;
      balls[i].dy = calculateBounceY(balls[i].y, paddle1Y, currentPaddle1_H, paddle1Velocity);
      if (fabs(balls[i].dy) < MIN_VERTICAL_SPEED) {
        balls[i].dy = (hitPoint >= 0) ? MIN_VERTICAL_SPEED : -MIN_VERTICAL_SPEED;
        if (fabs(hitPoint) < 1.0f) balls[i].dy = (random(0, 2) == 0) ? MIN_VERTICAL_SPEED : -MIN_VERTICAL_SPEED;
      }
      balls[i].x = 4 + PADDLE_W;
      playSFX(900 + (consecutiveHits * 20), 20);
    }

    // Colisión con Paleta del Jugador 2 / CPU (Derecha)
    const float P2_LEFT = 124 - PADDLE_W;
    if (balls[i].dx > 0 &&
        balls[i].x + BALL_SIZE >= P2_LEFT &&
        previousX + BALL_SIZE <= P2_LEFT &&
        balls[i].y + BALL_SIZE >= paddle2Y &&
        balls[i].y <= paddle2Y + currentPaddle2_H) {
      float hitPoint = (balls[i].y + (BALL_SIZE / 2.0)) - (paddle2Y + (currentPaddle2_H / 2.0));
      lastPlayerToHit = 2;
      balls[i].lastHitBy = 2;
      
      if (gameMode == 1) {
        if (joy2Y < 1000) balls[i].spin = -0.025;
        else if (joy2Y > 3000) balls[i].spin = 0.025;
        else balls[i].spin = 0.0;
      } else {
        balls[i].spin = (gameDifficulty == 2) ? (random(-25, 26) / 1000.0) : 0.0;
      }

      consecutiveHits++;
      if (consecutiveHits % HITS_FOR_NEXT_LEVEL == 0 && currentSpeedLevel < 3) {
        currentSpeedLevel++; playSFX(1500, 100);
      }
      if (currentSpeedLevel == 3) shakeFrames = 5;

      float speed = getCurrentBallSpeed();

      int rallyHits = min(consecutiveHits, RALLY_BONUS_CAP);
      speed *= (1.0 + rallyHits * RALLY_SPEED_BONUS);

      float normalizedHit2 = fabs(hitPoint) / max(1.0f, currentPaddle2_H / 2.0f);
      if (normalizedHit2 < 0.18f) {
        perfectHitTimer = millis() + 650;
        perfectHitPlayer = 2;
        speed *= 1.08f;
        shakeFrames = max(shakeFrames, 3);
        playSFX(2000 + min(consecutiveHits, 10) * 35, 55);
      } else if (abs(hitPoint) > (currentPaddle2_H / 3.0)) {
        speed *= 1.10;
        shakeFrames = 4;
        playSFX(1800, 30);
      }
      speed = constrain(speed, 1.5, BALL_MAX_SPEED);

      balls[i].dx = -speed;
      balls[i].dy = calculateBounceY(balls[i].y, paddle2Y, currentPaddle2_H, paddle2Velocity);
      if (fabs(balls[i].dy) < MIN_VERTICAL_SPEED) {
        balls[i].dy = (hitPoint >= 0) ? MIN_VERTICAL_SPEED : -MIN_VERTICAL_SPEED;
        if (fabs(hitPoint) < 1.0f) balls[i].dy = (random(0, 2) == 0) ? MIN_VERTICAL_SPEED : -MIN_VERTICAL_SPEED;
      }
      balls[i].x = 124 - PADDLE_W - BALL_SIZE;
      playSFX(900 + (consecutiveHits * 20), 20);
    }

    // Detección de Punto Marcado (La pelota supera el límite izquierdo o derecho)
    if (balls[i].x < 0 || balls[i].x > SCREEN_WIDTH) {
      bool isP1Point = (balls[i].x > SCREEN_WIDTH);
      float missCenter = balls[i].y + BALL_SIZE / 2.0f;
      float paddleCenter = isP1Point ? (paddle2Y + currentPaddle2_H / 2.0f) : (paddle1Y + currentPaddle1_H / 2.0f);
      
      // Evaluador de "Casi Fallo"
      if (fabs(missCenter - paddleCenter) <= 7.0f) {
        nearMissTimer = millis() + 500;
        nearMissPlayer = isP1Point ? 2 : 1;
      }
      balls[i].active = false; 

      // Si había multibola, elimina una y mantiene la partida activa
      if (activeBallsCount > 1) {
        if (isP1Point) scoreP1++; else scoreP2++;
        playSFX(1000, 100);
        activeBallsCount--;
        continue; 
      }

      // Conteo oficial de punto
      if (isP1Point) { scoreP1++; playSFX(1200, 200); }
      else { scoreP2++; playSFX(300, 200); }

      // Reinicio de parámetros para el siguiente saque
      resetMainBall(isP1Point ? BALL_SPEED_LVL1 : -BALL_SPEED_LVL1);
      consecutiveHits = 0; currentSpeedLevel = 1;
      activePortal.active = false;
      nextPortalTimer = millis();
      portalSpawnDelay = random(8000, 15001);

      powerUpActive = false; activePowerUpType = -1; lastPlayerToHit = 0;
      currentPaddle1_H = PADDLE_H; currentPaddle2_H = PADDLE_H;
      ghostBallActive = false; invertedControlsP1 = false; invertedControlsP2 = false;
      powerUpSpawnTimer = millis();
      powerUpSpawnDelay = random(10000, 15001);
      lastPhysicsUpdate = millis();

      paddle1Velocity = 0.0;
      paddle2Velocity = 0.0;
      lastPaddle1Y = paddle1Y;
      lastPaddle2Y = paddle2Y;

      stateTimer = millis();
      currentState = STATE_POINT_SCORED;
      return;
    }

    // Dibujar la pelota en pantalla
    if (!ghostBallActive || (ghostBallActive && (millis() / 200) % 2 == 0)) {
      oledMonitor.fillRect((int)balls[i].x, (int)balls[i].y + shakeOffset, BALL_SIZE, BALL_SIZE, WHITE);
    }
  }

  // 10. COMPROBAR VICTORIA Y FIN DE LA PARTIDA
  if (scoreP1 >= scoreLimit || scoreP2 >= scoreLimit) {
    checkAndSaveHighScore(max(scoreP1, scoreP2));
    int winner = (scoreP1 >= scoreLimit) ? 1 : 2;
    if (gameMode == 0) { if (winner == 1) wins_1p_ia++; else wins_ia++; } 
    else { if (winner == 1) wins_p1_vs++; else wins_p2_vs++; }
    saveSettings(); 
    stopAudio();    
    
    // Melodía de victoria o derrota
    if (gameMode == 0 && winner == 2) playSFX(300, 800); 
    else { playSFX(523, 150); playSFX(784, 150); playSFX(1046, 400); } 

    gameOverOption = 0;             
    currentState = STATE_GAME_OVER; 
    return;
  }

  oledMonitor.display();
}

// ==========================================
// ESTADO: PUNTO ANOTADO
// ==========================================
/**
 * @brief Pantalla estática temporal tras marcar un punto antes del re-saque.
 */
void handlePointScored() {
  oledMonitor.clearDisplay();
  
  for (int y = 0; y < SCREEN_HEIGHT; y += 6) oledMonitor.drawFastVLine(64, y, 3, WHITE);
  oledMonitor.setTextSize(1); oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(20, 2); oledMonitor.print(scoreP1);
  oledMonitor.setCursor(102, 2); oledMonitor.print(scoreP2);
  oledMonitor.fillRect(4, (int)paddle1Y, PADDLE_W, currentPaddle1_H, WHITE);
  oledMonitor.fillRect(124 - PADDLE_W, (int)paddle2Y, PADDLE_W, currentPaddle2_H, WHITE);

  if ((millis() / 250) % 2 == 0 && balls[0].active) {
    oledMonitor.fillRect((int)balls[0].x, (int)balls[0].y, BALL_SIZE, BALL_SIZE, WHITE);
  }
  
  oledMonitor.display();

  // Pausa de 1.5 segundos antes de reanudar
  if (millis() - stateTimer > 1500) {
    currentState = STATE_PLAY;
  }
}

// ==========================================
// ESTADO: MENÚ DE FIN DE PARTIDA
// ==========================================
/**
 * @brief Muestra el ganador del juego y las opciones para reiniciar o volver.
 */
void handleGameOver() {
  oledMonitor.clearDisplay();
  int winner = (scoreP1 >= scoreLimit) ? 1 : 2;
  
  oledMonitor.setTextSize(2);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(10, 8);
  if (gameMode == 0 && winner == 2) oledMonitor.print("CPU GANA");
  else { oledMonitor.print("J"); oledMonitor.print(winner); oledMonitor.print(" GANA"); }

  int joy1Y = readJoystickY();
  if (joy1Y < 1000) gameOverOption = 0; 
  if (joy1Y > 3000) gameOverOption = 1; 

  oledMonitor.setTextSize(1);
  if (gameOverOption == 0) {
    oledMonitor.fillRect(15, 33, 98, 13, WHITE);
    oledMonitor.setTextColor(BLACK);
  } else { oledMonitor.setTextColor(WHITE); }
  oledMonitor.setCursor(22, 36);
  oledMonitor.print("NUEVA PARTIDA");

  if (gameOverOption == 1) {
    oledMonitor.fillRect(15, 48, 98, 13, WHITE);
    oledMonitor.setTextColor(BLACK);
  } else { oledMonitor.setTextColor(WHITE); }
  oledMonitor.setCursor(22, 51);
  oledMonitor.print("MENU PRINCIPAL");

  oledMonitor.display();

  if (digitalRead(J1_BTN_PIN) == LOW) {
    delay(200); 
    
    scoreP1 = 0; scoreP2 = 0;
    paddle1Y = 24; paddle2Y = 24;
    lastPaddle1Y = paddle1Y; lastPaddle2Y = paddle2Y;
    paddle1Velocity = 0.0; paddle2Velocity = 0.0;
    consecutiveHits = 0; currentSpeedLevel = 1;
    resetMainBall(1.0);
    scheduleArcadeTimers();
    lastPhysicsUpdate = millis();

    if (gameOverOption == 0) {
      currentState = STATE_PLAY; 
    } else {
      animateScreenWipe();
      currentState = STATE_MENU; 
    }
  }
}

// ==========================================
// ESTADO: MENÚ DE PAUSA
// ==========================================
/**
 * @brief Gestiona la interfaz de pausa mid-game.
 */
void handlePause() {
  oledMonitor.clearDisplay();
  
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(50, 5);
  oledMonitor.print("PAUSA");

  int joy1Y = readJoystickY();
  if (joy1Y < 1000) {
    pauseOption--;
    if (pauseOption < 0) pauseOption = 2; 
    playSFX(800, 15); 
    delay(150); 
  }
  if (joy1Y > 3000) {
    pauseOption++;
    if (pauseOption > 2) pauseOption = 0;  
    playSFX(800, 15);  
    delay(150);
  }

  // Opción Continuar
  if (pauseOption == 0) {
    oledMonitor.fillRect(15, 18, 98, 11, WHITE);
    oledMonitor.setTextColor(BLACK);
  } else { oledMonitor.setTextColor(WHITE); }
  oledMonitor.setCursor(35, 20);
  oledMonitor.print("CONTINUAR");

  // Opción Configuración
  if (pauseOption == 1) {
    oledMonitor.fillRect(15, 33, 98, 11, WHITE);
    oledMonitor.setTextColor(BLACK);
  } else { oledMonitor.setTextColor(WHITE); }
  oledMonitor.setCursor(25, 35);
  oledMonitor.print("CONFIGURACION");

  // Opción Salir
  if (pauseOption == 2) {
    oledMonitor.fillRect(15, 48, 98, 11, WHITE);
    oledMonitor.setTextColor(BLACK);
  } else { oledMonitor.setTextColor(WHITE); }
  oledMonitor.setCursor(48, 50);
  oledMonitor.print("SALIR");

  oledMonitor.display();

  if (digitalRead(J1_BTN_PIN) == HIGH) {
    isPauseButtonPressed = false; 
  }

  if (digitalRead(J1_BTN_PIN) == LOW && !isPauseButtonPressed) {
    delay(200); 
    playSFX(800, 15);
    
    if (pauseOption == 0) {
      currentState = STATE_PLAY; 
    } else if (pauseOption == 1) {
      settingsReturnState = STATE_PAUSE; 
      currentState = STATE_SETTINGS; 
    } else {
      scoreP1 = 0; scoreP2 = 0;
      lastPaddle1Y = paddle1Y; lastPaddle2Y = paddle2Y;
      paddle1Velocity = 0.0; paddle2Velocity = 0.0;
      consecutiveHits = 0; currentSpeedLevel = 1;
      resetMainBall(1.0);
      scheduleArcadeTimers();
      lastPhysicsUpdate = millis();

      animateScreenWipe();
      settingsReturnState = STATE_MENU; 
      currentState = STATE_MENU; 
    }
  }
}

// ==========================================
// FUNCIONES AUXILIARES MULTIPELOTA Y FÍSICA
// ==========================================
/**
 * @brief Obtiene el valor de velocidad correspondiente al nivel actual del juego.
 */
float getCurrentBallSpeed() {
  if (currentSpeedLevel == 1) return BALL_SPEED_LVL1;
  if (currentSpeedLevel == 2) return BALL_SPEED_LVL2;
  return BALL_SPEED_LVL3;
}

/**
 * @brief Calcula el ángulo y velocidad de rebote vertical basándose en el punto de contacto e inercia.
 */
float calculateBounceY(float ballY, float paddleY, int paddleHeight, float paddleVelocity) {
  float paddleCenter = paddleY + paddleHeight / 2.0;
  float ballCenter = ballY + BALL_SIZE / 2.0;

  // Cálculo del impacto relativo entre -1.0 y 1.0
  float relativeHit = (ballCenter - paddleCenter) / (paddleHeight / 2.0);
  relativeHit = constrain(relativeHit, -1.0, 1.0);

  float vertical = relativeHit * MAX_VERTICAL_SPEED;

  // Inercia: Transfiere parte de la velocidad de la paleta a la pelota
  vertical += paddleVelocity * PADDLE_INFLUENCE;

  return constrain(vertical, -MAX_VERTICAL_SPEED, MAX_VERTICAL_SPEED);
}

/**
 * @brief Procesa el movimiento suave de la paleta del jugador con filtrado de zona muerta.
 */
void updatePlayerPaddle(int rawY, float &paddleY, int paddleHeight, bool inverted, float physicsScale) {
  float error = (float)rawY - JOY_CENTER;
  if (fabs(error) <= JOY_DEADZONE) return; // Si está dentro de la zona muerta, descarta el movimiento

  float normalized = (error > 0)
      ? (error - JOY_DEADZONE) / (JOY_MAX - JOY_CENTER - JOY_DEADZONE)
      : (error + JOY_DEADZONE) / (JOY_CENTER - JOY_MIN - JOY_DEADZONE);

  normalized = constrain(normalized, -1.0f, 1.0f);

  // Curva cuadrática de aceleración para respuestas más precisas
  float curved = normalized * fabs(normalized);
  float movement = curved * paddleSpeed * 1.55f * physicsScale;

  if (inverted) movement = -movement;

  paddleY += movement;
  paddleY = constrain(paddleY, (float)COURT_TOP_Y, (float)(COURT_BOTTOM_Y - paddleHeight));
}

// ============================================================
// IA CPU V5 - LÓGICA COMPETITIVA Y PREDICCIÓN DE TRAYECTORIA
// ============================================================
/**
 * @brief Controla la paleta de la CPU calculando rebotes, anticipación de trayectorias y errores humanos simular.
 * @param physicsScale Factor de escala para independizar el framerate.
 */
void updateCpuPaddle(float physicsScale) {

  // 1. Parámetros según la dificultad elegida
  float multiplier;
  float predictionFactor;
  float errorRange;
  float deadzone;

  if (gameDifficulty == 0) {
    // FÁCIL: Respuesta lenta, baja precisión
    multiplier       = 0.48f;
    predictionFactor = 0.55f;
    errorRange      = 7.0f;
    deadzone        = 3.5f;

  } else if (gameDifficulty == 1) {
    // NORMAL: Respuesta equilibrada
    multiplier       = 0.72f;
    predictionFactor = 0.78f;
    errorRange      = 4.5f;
    deadzone        = 1.8f;

  } else {
    // DIFÍCIL: Alta precisión y anticipación rápida
    multiplier       = 1.00f;
    predictionFactor = 0.94f;
    errorRange      = 1.8f;
    deadzone        = 0.75f;
  }

  // 2. Identifica cuál es la pelota con mayor amenaza (la más cercana desplazándose hacia la derecha)
  int targetIndex = -1;
  float closestDistance = 9999.0f;

  for (int i = 0; i < 2; i++) {
    if (!balls[i].active) continue;
    if (balls[i].dx <= 0.01f) continue; // Descarta las que van alejándose de la CPU

    float distance = (124.0f - PADDLE_W) - balls[i].x;

    if (distance < closestDistance) {
      closestDistance = distance;
      targetIndex = i;
    }
  }

  // 3. Posición predeterminada al centro
  float targetY = SCREEN_HEIGHT / 2.0f;

  if (targetIndex >= 0) {
    Ball &target = balls[targetIndex];
    targetY = target.y + BALL_SIZE / 2.0f;

    // 4. Predicción de trayectoria matemática con rebote en paredes
    if (target.dx > 0.01f) {
      float predictionDistance = closestDistance * predictionFactor;
      float projected = targetY + target.dy * (predictionDistance / target.dx);

      float minY = COURT_TOP_Y + currentPaddle2_H / 2.0f;
      float maxY = COURT_BOTTOM_Y - currentPaddle2_H / 2.0f;
      float span = maxY - minY;

      if (span > 0) {
        float period = span * 2.0f;
        float v = fmod(projected - minY, period);

        if (v < 0) v += period;
        if (v > span) v = period - v;

        targetY = minY + v;
      }

      // 5. Anticipación extra en pelotas de alta velocidad en modo difícil
      if (gameDifficulty == 2 && target.dx > 3.5f) {
        float speedFactor = constrain((fabs(target.dx) - 3.5f) / 2.0f, 0.0f, 1.0f);
        targetY += target.dy * speedFactor * 2.0f;
      }

      // 6. Generación de un margen de error aleatorio
      if (gameDifficulty >= 1) {
        float randomError = random((int)(-errorRange * 10.0f), (int)(errorRange * 10.0f) + 1) / 10.0f;
        targetY += randomError;
      }
    }
  }

  // 7. Reacción de pánico/aceleración cuando la pelota está muy cerca
  if (targetIndex >= 0 && closestDistance < 35.0f) {
    multiplier += 0.08f;
  }

  // 8. Desplazamiento real de la paleta
  float center = paddle2Y + currentPaddle2_H / 2.0f;
  float error = targetY - center;

  if (fabs(error) > deadzone) {
    float maxMove = paddleSpeed * multiplier * physicsScale;
    float move = constrain(error, -maxMove, maxMove);

    // Si la CPU sufre el castigo de controles invertidos
    if (invertedControlsP2) move = -move;

    paddle2Y += move;
  }

  // 9. Mantener la paleta dentro del área permitida
  paddle2Y = constrain(paddle2Y, (float)COURT_TOP_Y, (float)(COURT_BOTTOM_Y - currentPaddle2_H));
}

/**
 * @brief Programa los temporizadores de aparición para elementos especiales.
 */
void scheduleArcadeTimers() {
  powerUpSpawnTimer = millis();
  powerUpSpawnDelay = random(10000, 15001);
  nextPortalTimer = millis();
  portalSpawnDelay = random(8000, 15001);
}

/**
 * @brief Restablece la pelota principal al centro del campo con una dirección inicial.
 * @param dirX Dirección del saque (positivo para la derecha, negativo para la izquierda).
 */
void resetMainBall(float dirX) {
  balls[0].x = 64;
  balls[0].y = 32;
  balls[0].spin = 0.0;
  balls[0].lastHitBy = 0;
  balls[0].dx = (dirX >= 0 ? 1.0 : -1.0) * BALL_SPEED_LVL1;
  balls[0].dy = (random(0, 2) == 0 ? 1.5 : -1.5);
  balls[0].active = true;

  balls[1].active = false; // Desactiva la pelota secundaria
}

/**
 * @brief Instancia la segunda pelota para activar el modo Multibola.
 */
void spawnSecondaryBall() {
  if (!balls[1].active && balls[0].active) {
    balls[1].x = balls[0].x;
    balls[1].y = balls[0].y;
    balls[1].dx = -balls[0].dx; 
    balls[1].dy = -balls[0].dy * 0.9;
    balls[1].spin = 0.0;
    balls[1].lastHitBy = 0;
    balls[1].active = true;
    playSFX(1500, 150);
  }
}