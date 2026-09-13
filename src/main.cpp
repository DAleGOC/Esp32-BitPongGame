/**
 * @file      PinPong_Arcade.ino
 * @author    [Tu Nombre / Arcade Studio]
 * @brief     Secuencia de inicio profesional para consola retro ESP32.
 *            Incluye físicas, partículas, screen shake y sonido de 8-bits.
 * @hardware  ESP32, Pantalla OLED SH1106 (I2C), Zumbador Pasivo + Transistor 2N2222.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>

// ==========================================
//   CONFIGURACIÓN DE HARDWARE Y PINES
// ==========================================
// Pantalla OLED
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RESET -1 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

// Audio
#define BUZZER_PIN 5 

// ==========================================
//   CONFIGURACIÓN DE CONTROLES (JOYSTICKS)
// ==========================================
// Joystick 1 (Jugador 1 / Navegación de menú)
#define J1_Y_PIN 34    // Pin analógico para Arriba/Abajo
#define J1_BTN_PIN 25  // Pin digital para el botón (Push button)

// Joystick 2 (Jugador 2) - Los usaremos más adelante
#define J2_Y_PIN 35    
#define J2_BTN_PIN 26  

// ==========================================
//   MÁQUINA DE ESTADOS DEL JUEGO
// ==========================================
enum GameState {
  STATE_SPLASH, // Nuevo: Modo Demostración
  STATE_MENU,
  STATE_PLAY,
  STATE_SETTINGS,
  STATE_CREDITS,
  STATE_SLEEP,
  STATE_ATTRACT,
 
};

GameState currentState = STATE_MENU; // El juego inicia en la pantalla de bienvenida








// ==========================================
//   INSTANCIAS GLOBALES
// ==========================================
Adafruit_SH1106 oledMonitor(OLED_RESET);


// Variables globales para el control del menú
int currentMenuOption = 0;       // 0 = Jugar, 1 = Configuración, 2 = Créditos
const int totalMenuOptions = 3;
bool joystickLocked = false;     // Evita que el menú haga scroll a la velocidad de la luz
unsigned long lastBtnPress = 0;  // Antirrebote (Debounce) del botón


// Estrellas de fondo para el menú (Parallax)
const int MENU_STARS = 8;
float mStarX[MENU_STARS];
int mStarY[MENU_STARS];
float mStarSpeed[MENU_STARS];
bool starsInitialized = false;



//animacion barra deslizante
int animOffset = abs((int)(millis() / 120) % 4 - 2);

// Temporizador de Inactividad para Attract Mode
unsigned long lastActivityTime = 0; 
const unsigned long INACTIVITY_TIMEOUT = 15000; // 15000 ms = 15 segundos




// ==========================================
//   BITMAPS PIXEL-ART 7x7 (ICONOS MENÚ)
// ==========================================
// 🏓 Icono JUGAR (Raqueta y Pelota)
const uint8_t PROGMEM icon_play[] = {
  0b01110000,
  0b10001010,
  0b10001000,
  0b01110000,
  0b00100000,
  0b00100000,
  0b00100000
};

// ⚙️ Icono CONFIGURACIÓN (Engranaje)
const uint8_t PROGMEM icon_settings[] = {
  0b00111000,
  0b01010100,
  0b10000010,
  0b11010110,
  0b10000010,
  0b01010100,
  0b00111000
};

// 🏆 Icono CRÉDITOS (Copa/Trofeo)
const uint8_t PROGMEM icon_credits[] = {
  0b11111110,
  0b10111010,
  0b01111100,
  0b00111000,
  0b00010000,
  0b00010000,
  0b00111000
};

// ⏻ Icono SALIR (Power/Encendido)
const uint8_t PROGMEM icon_sleep[] = {
  0b00010000,
  0b01111100,
  0b11010010,
  0b10010010,
  0b10000010,
  0b01000010,
  0b00111000
};

// Array de punteros para acceder fácilmente por índice
const uint8_t* const menuIcons[] = {
  icon_play, 
  icon_settings, 
  icon_credits, 
  icon_sleep
};









// ==========================================
//   PROTOTIPOS DE FUNCIONES (Firma de métodos)
// ==========================================

// funciones de animacion de inicio 
void showStudioScreen();
void showBouncingBallAnimation();
void showCenterExplosion();
void showLoadingBarAnimation();
void starParallaxEffect();
//funciones demenu
void handleSplashScreen();
void handleMenu();
void handleSleep();
void handleAttractMode();


/**
 * @brief Función principal de configuración.
 * Inicializa hardware y ejecuta la secuencia cinemática de inicio.
 */
void setup() {
  // Inicialización de bus I2C y Pantalla
  Wire.begin(OLED_SDA, OLED_SCL);
  oledMonitor.begin(SH1106_SWITCHCAPVCC, OLED_ADDRESS);
  oledMonitor.clearDisplay();
  oledMonitor.display();

  // Inicialización de periféricos
  pinMode(BUZZER_PIN, OUTPUT);
  // Configurar botón del joystick con resistencia pull-up interna
  pinMode(J1_BTN_PIN, INPUT_PULLUP);

  // --- SECUENCIA DE ARRANQUE (BOOT SEQUENCE) ---
  showStudioScreen();          // Fase 0: Logo del estudio
  showBouncingBallAnimation(); // Fase 1: Calentamiento de físicas
  showCenterExplosion();       // Fase 2: Impacto visual y sonoro
  showLoadingBarAnimation();   // Fase 3: Menú principal y transición
}

/**
 * @brief Bucle principal del juego.
 * Aquí se implementará la lógica no bloqueante (millis) del Ping Pong.
 */
void loop() {
switch (currentState) {
   case STATE_SPLASH:
      handleSplashScreen();
      break;

    case STATE_MENU:
      handleMenu();
      break;

    case STATE_ATTRACT:
      handleAttractMode();
      break;

    case STATE_PLAY:
      // Próxima sección: Lógica del juego
      break;

    case STATE_SETTINGS:
      // Ajustes
      break;

    case STATE_CREDITS:
      // Créditos
      break;

    case STATE_SLEEP:
      handleSleep();
      break;
  }
}


// ==========================================
//   IMPLEMENTACIÓN DE ANIMACIONES
// ==========================================


/**
 * @brief Muestra el logo del creador con efecto de máquina de escribir.
 * Utiliza const char* en lugar de String para optimizar la memoria del ESP32.
 */
void showStudioScreen() {
  oledMonitor.clearDisplay();
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);

  // Uso de punteros constantes de caracteres (Práctica C/C++ Profesional)
  const char* linea1 = "ARCADE STUDIO";
  const char* linea2 = "PRESENTS";

  // 1. Línea 1: Máquina de escribir
  oledMonitor.setCursor(26, 20); 
  for(int i = 0; i < strlen(linea1); i++) {
    oledMonitor.print(linea1[i]);
    oledMonitor.display();
    
    tone(BUZZER_PIN, 1500 + random(0, 500), 20); // Beep de terminal
    delay(100); 
  }

  delay(400); // Pausa de lectura

  // 2. Línea 2: Máquina de escribir
  oledMonitor.setCursor(40, 36);
  for(int i = 0; i < strlen(linea2); i++) {
    oledMonitor.print(linea2[i]);
    oledMonitor.display();
    
    tone(BUZZER_PIN, 800 + random(0, 200), 20); // Beep grave
    delay(80);
  }

  // 3. Acorde de presentación
  delay(200);
  tone(BUZZER_PIN, 880, 100);  
  delay(100);
  tone(BUZZER_PIN, 1108, 100); 
  delay(100);
  tone(BUZZER_PIN, 1318, 400); 
  
  delay(1200); 

  // 4. Wipe Out (Barrido cinematográfico)
  for(int i = 0; i <= 32; i += 2) {
    oledMonitor.fillRect(0, 32 - i, 128, i * 2, BLACK);
    oledMonitor.display();
    delay(15);
  }
  
  noTone(BUZZER_PIN);
  delay(600); 
}

/**
 * @brief Simula una pelota acelerando con físicas de rebote básico.
 * Deja un rastro de partículas (Motion Trail) manipulando un buffer circular.
 */
void showBouncingBallAnimation() {
  oledMonitor.clearDisplay();
  oledMonitor.display();

  int ballSize = 2; 
  float ballX = 10; float ballY = 10; 
  float ballDX = 3; float ballDY = 3; 
  float speedMultiplier = 1.0;

  // Buffer para el rastro de la pelota
  int trailX[10]; int trailY[10];
  for(int i = 0; i < 10; i++) { trailX[i] = -1; trailY[i] = -1; }
  int trailIndex = 0;

  unsigned long startTime = millis();
  unsigned long phaseDuration = 4000;

  while (millis() - startTime < phaseDuration) {
    // Registrar posición actual en el buffer del rastro
    trailX[trailIndex] = (int)ballX;
    trailY[trailIndex] = (int)ballY;
    trailIndex = (trailIndex + 1) % 10;

    // Aplicar vectores de movimiento
    ballX += ballDX * speedMultiplier;
    ballY += ballDY * speedMultiplier;

    // Detección de colisiones y SFX (Efectos de sonido)
    if (ballX <= 0 || ballX >= (SCREEN_WIDTH - ballSize)) {
      ballDX *= -1;
      speedMultiplier *= 1.01; 
      tone(BUZZER_PIN, 800, 15); 
    }
    if (ballY <= 0 || ballY >= (SCREEN_HEIGHT - ballSize)) {
      ballDY *= -1;
      speedMultiplier *= 1.01; 
      tone(BUZZER_PIN, 1000, 15); 
    }

    if (millis() - startTime > 2000) { speedMultiplier += 0.08; } // Frenesí final

    oledMonitor.clearDisplay(); 

    // Renderizar rastro
    for (int i = 0; i < 10; i++) {
      if (trailX[i] != -1) {
        oledMonitor.drawPixel(trailX[i], trailY[i], WHITE);
        oledMonitor.drawPixel(trailX[i]+1, trailY[i], WHITE);
        oledMonitor.drawPixel(trailX[i], trailY[i]+1, WHITE);
        oledMonitor.drawPixel(trailX[i]+1, trailY[i]+1, WHITE);
      }
    }

    // Renderizar pelota principal
    oledMonitor.fillRect((int)ballX, (int)ballY, ballSize, ballSize, WHITE);
    oledMonitor.display();
    delay(10);
  }
}

/**
 * @brief Genera una explosión radial procedimental.
 * Incluye Screen Shake (temblor de cámara) y simulación de ruido blanco en audio.
 */
void showCenterExplosion() {
  int centerX = SCREEN_WIDTH / 2;
  int centerY = SCREEN_HEIGHT / 2;

  oledMonitor.clearDisplay(); 
  oledMonitor.display();
  delay(100);

  // Precarga visual
  oledMonitor.fillRect(centerX-1, centerY-1, 2, 2, WHITE);
  oledMonitor.display();
  delay(300); 

  // Flash de impacto (Inversión de colores)
  oledMonitor.invertDisplay(true);
  tone(BUZZER_PIN, 150, 60); 
  delay(60);
  oledMonitor.invertDisplay(false);

  // Expansión volumétrica
  int maxRadius = 45; 
  for (int r = 1; r < maxRadius; r += 3) {
    oledMonitor.clearDisplay(); 
    
    // Generación de vectores de temblor (Screen Shake)
    int offsetX = random(-3, 4); 
    int offsetY = random(-3, 4); 

    // Renderizado de la onda expansiva hueca
    int numLines = 16;
    for (int i = 0; i < numLines; i++) {
      float angle = (i * 2 * PI) / numLines;
      
      int startX = centerX + offsetX + (r / 1.5) * cos(angle);
      int startY = centerY + offsetY + (r / 1.5) * sin(angle);
      
      int endX = centerX + offsetX + r * cos(angle);
      int endY = centerY + offsetY + r * sin(angle);
      
      oledMonitor.drawLine(startX, startY, endX, endY, WHITE);
    }
    
    // Renderizado de partículas de escombros
    for (int p = 0; p < 10; p++) {
      int px = centerX + offsetX + random(-r, r);
      int py = centerY + offsetY + random(-r, r);
      oledMonitor.drawPixel(px, py, WHITE);
    }

    oledMonitor.display();
    
    // Generador de ruido blanco sintético para el crujido
    tone(BUZZER_PIN, random(40, 200), 15); 
    delay(15); 
  }

  noTone(BUZZER_PIN); 

  // Disipación de energía (Micro-parpadeos)
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
 * @brief Pantalla principal interactiva.
 * Muestra el título con gravedad, carga de progreso, fondo Parallax (3D)
 * y una desintegración por módulo matemático al finalizar.
 */
void showLoadingBarAnimation() {
  oledMonitor.setTextSize(2);
  oledMonitor.setTextColor(WHITE);
  
  // 1. Título cayendo (Física de Gravedad simple)
  for (int posY = -20; posY <= 4; posY += 4) {
    oledMonitor.clearDisplay();
    oledMonitor.setCursor(22, posY); 
    oledMonitor.print("PINPONG");
    oledMonitor.display();
    
    tone(BUZZER_PIN, 400 + (posY * 10), 10); // Pitch rising effect
    delay(15);
  }
  
  // Impacto
  tone(BUZZER_PIN, 120, 80);

  // 2. Barra de Progreso (Simulación de carga)
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

  for (int i = 0; i < numBlocks; i++) {
    int xPos = barX + innerPadding + i * (blockWidth + blockGap);
    int yPos = barY + innerPadding;
    int blockHeight = barHeight - (innerPadding * 2);

    oledMonitor.fillRect(xPos, yPos, blockWidth, blockHeight, WHITE);
    oledMonitor.display(); 
    
    tone(BUZZER_PIN, 1800, 15);
    delay(100 + random(30, 150)); // Delay errático simulando hardware viejo
  }

  delay(300);

  // Acorde de inserción de moneda (Coin Insert)
  tone(BUZZER_PIN, 988, 100);  
  delay(120);
  tone(BUZZER_PIN, 1319, 200); 


  // 3. Bucle de Espera: Campo de Estrellas Parallax y Texto Intermitente
  const int NUM_STARS = 12; 
  float starX[NUM_STARS];
  int starY[NUM_STARS];
  float starSpeed[NUM_STARS];

  // Instanciación del universo
  for(int i = 0; i < NUM_STARS; i++) {
    starX[i] = random(0, SCREEN_WIDTH);
    starY[i] = random(0, SCREEN_HEIGHT);
    starSpeed[i] = random(5, 26) / 10.0; // Velocidad variable genera efecto 3D
  }

  // Timers no bloqueantes
  unsigned long pressStartTimer = millis(); 
  unsigned long blinkTimer = millis();      
  bool showPressStart = true;               

  while (millis() - pressStartTimer < 6000) {
    oledMonitor.clearDisplay();

    // Actualización y renderizado del fondo
    for(int i = 0; i < NUM_STARS; i++) {
      starX[i] -= starSpeed[i]; 
      
      if (starX[i] < 0) { // Respawn de estrellas fuera de cámara
        starX[i] = SCREEN_WIDTH;
        starY[i] = random(0, SCREEN_HEIGHT);
      }
      
      oledMonitor.drawPixel((int)starX[i], starY[i], WHITE);
    }

    // Renderizado UI Principal
    oledMonitor.setTextSize(2);
    oledMonitor.setCursor(22, 4);
    oledMonitor.print("PINPONG");

    // Toggle intermitente sin bloqueo
    if (millis() - blinkTimer > 400) {
      showPressStart = !showPressStart; 
      blinkTimer = millis();            
    }

    if (showPressStart) {
      oledMonitor.setTextSize(1);
      oledMonitor.setCursor(28, 40);
      oledMonitor.print("PRESS START");
    }

    oledMonitor.display();
    delay(20); // Bloqueo mínimo para mantener ~50 FPS
  }

  // 4. Transición de Salida Procedimental (Desintegración entrelazada)
  tone(BUZZER_PIN, 200, 100);
  delay(100);
  tone(BUZZER_PIN, 150, 200);
  delay(100);

  // Algoritmo de borrado de tablero de ajedrez desfasado
  for (int paso = 0; paso < 8; paso++) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
      for (int x = 0; x < SCREEN_WIDTH; x++) {
        if ((x + (y * 3)) % 8 == paso) {
          oledMonitor.drawPixel(x, y, BLACK);
        }
      }
    }
    oledMonitor.display();
    
    tone(BUZZER_PIN, random(30, 80), 30); // Ruido de estática
    delay(40); 
  }
  
  noTone(BUZZER_PIN); 

  // Limpieza de buffer final para iniciar el juego limpio
  oledMonitor.clearDisplay();
  oledMonitor.display();
  delay(300); 
}

void starParallaxEffect() {
  const int NUM_STARS = 12; 
  float starX[NUM_STARS];
  int starY[NUM_STARS];
  float starSpeed[NUM_STARS];

  for(int i = 0; i < NUM_STARS; i++) {
    starX[i] = random(0, SCREEN_WIDTH);
    starY[i] = random(0, SCREEN_HEIGHT);
    starSpeed[i] = random(5, 26) / 10.0; 
  }

  unsigned long startTime = millis();
  while (millis() - startTime < 3000) { 
    oledMonitor.clearDisplay();

    for(int i = 0; i < NUM_STARS; i++) {
      starX[i] -= starSpeed[i]; 
      
      if (starX[i] < 0) { 
        starX[i] = SCREEN_WIDTH;
        starY[i] = random(0, SCREEN_HEIGHT);
      }
      
      oledMonitor.drawPixel((int)starX[i], starY[i], WHITE);
    }

    oledMonitor.display();
    delay(20); 
  }
}






// ==========================================
//   LÓGICA E INTERFAZ DEL MENÚ PRINCIPAL
// ==========================================


void handleMenu() {
const int totalMenuOptions = 4;

  // Inicializar estrellas la primera vez
  if (!starsInitialized) {
    for (int i = 0; i < MENU_STARS; i++) {
      mStarX[i] = random(0, SCREEN_WIDTH);
      mStarY[i] = random(12, SCREEN_HEIGHT);
      mStarSpeed[i] = random(3, 15) / 10.0;
    }
    starsInitialized = true;
  }

  // --- 1. LECTURA DEL JOYSTICK ---
  int joyY = analogRead(J1_Y_PIN); 
  
  if (joyY < 1000 && !joystickLocked) {
    currentMenuOption--;
    if (currentMenuOption < 0) currentMenuOption = totalMenuOptions - 1; 
    tone(BUZZER_PIN, 1200, 15); 
    joystickLocked = true;
    lastActivityTime = millis(); // REINICIAR TEMPORIZADOR
  }
  else if (joyY > 3000 && !joystickLocked) {
    currentMenuOption++;
    if (currentMenuOption >= totalMenuOptions) currentMenuOption = 0; 
    tone(BUZZER_PIN, 1000, 15); 
    joystickLocked = true;
    lastActivityTime = millis(); // REINICIAR TEMPORIZADOR
  }
  else if (joyY > 1500 && joyY < 2500) {
    joystickLocked = false; 
  }

  // --- 2. SELECCIÓN CON BOTÓN ---
  if (digitalRead(J1_BTN_PIN) == LOW && millis() - lastBtnPress > 300) {
    lastBtnPress = millis();
    lastActivityTime = millis(); // REINICIAR TEMPORIZADOR
    
    tone(BUZZER_PIN, 1500, 40);
    delay(40);
    tone(BUZZER_PIN, 2000, 100);
    
    if (currentMenuOption == 0) currentState = STATE_PLAY;
    if (currentMenuOption == 1) currentState = STATE_SETTINGS;
    if (currentMenuOption == 2) currentState = STATE_CREDITS;
    if (currentMenuOption == 3) currentState = STATE_SLEEP;
    
    oledMonitor.clearDisplay();
    oledMonitor.display();
    delay(150);
    return; 
  }

  // --- 3. RENDERIZADO VISUAL ---
  oledMonitor.clearDisplay();

  // A. Fondo Parallax
  for (int i = 0; i < MENU_STARS; i++) {
    mStarX[i] -= mStarSpeed[i];
    if (mStarX[i] < 0) {
      mStarX[i] = SCREEN_WIDTH;
      mStarY[i] = random(14, SCREEN_HEIGHT);
    }
    oledMonitor.drawPixel((int)mStarX[i], mStarY[i], WHITE);
  }

  // B. Cabecera (Header Banner)
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(4, 2);
  oledMonitor.print("PINPONG");
  
  oledMonitor.setCursor(100, 2);
  oledMonitor.print("v1.0");
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  // C. Cálculo de Animación de Respiración para el Cursor
  int animOffset = abs((int)(millis() / 120) % 4 - 2); // Oscila entre 0, 1, 2

  // D. Renderizado de Opciones con Iconos y Flecha Animada
  const char* options[4] = {"JUGAR", "CONFIGURACION", "CREDITOS", "SALIR"};
  
  for (int i = 0; i < totalMenuOptions; i++) {
    int yPos = 15 + (i * 12);
    
    if (i == currentMenuOption) {
      // 1. FLECHA ANIMADA QUE "RESPIRA" (Se mueve entre X=0 y X=2)
      oledMonitor.setTextColor(WHITE);
      oledMonitor.setCursor(0 + animOffset, yPos);
      oledMonitor.print(">");

      // 2. CAJA SELECCIONADA (Ajustada a X=8 para dar espacio a la flecha)
      oledMonitor.fillRoundRect(8, yPos - 1, 114, 10, 2, WHITE);
      
      // 3. ICONO Y TEXTO EN NEGRO
      oledMonitor.drawBitmap(12, yPos, menuIcons[i], 7, 7, BLACK);
      oledMonitor.setTextColor(BLACK); 
      oledMonitor.setCursor(23, yPos);
      oledMonitor.print(options[i]);
    } else {
      // OPCIÓN NORMAL
      oledMonitor.drawBitmap(12, yPos, menuIcons[i], 7, 7, WHITE);
      oledMonitor.setTextColor(WHITE);
      oledMonitor.setCursor(23, yPos);
      oledMonitor.print(options[i]);
    }
  }


  if (millis() - lastActivityTime > INACTIVITY_TIMEOUT) {
    currentState = STATE_ATTRACT;
    oledMonitor.clearDisplay();
    oledMonitor.display();
    return;
  }


  oledMonitor.display();
}


// ==========================================
//   SECUENCIA DE APAGADO (STATE_SLEEP)
// ==========================================
void handleSleep() {
  oledMonitor.clearDisplay();
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(28, 28);
  oledMonitor.print("SYSTEM OFF...");
  oledMonitor.display();
  
  // Melodía triste de apagado
  tone(BUZZER_PIN, 1000, 100); delay(120);
  tone(BUZZER_PIN, 800, 100);  delay(120);
  tone(BUZZER_PIN, 500, 300);  delay(350);
  noTone(BUZZER_PIN);

  // Animación CRT: La pantalla se encoje hacia el centro
  for (int h = 32; h > 0; h -= 2) {
    oledMonitor.clearDisplay();
    oledMonitor.drawRect(0, 32 - h, 128, h * 2, WHITE);
    oledMonitor.display();
    delay(10);
  }

  // Punto final que desaparece
  oledMonitor.clearDisplay();
  oledMonitor.fillRect(63, 31, 2, 2, WHITE);
  oledMonitor.display();
  delay(300);

  oledMonitor.clearDisplay();
  oledMonitor.display();

  // Configurar el botón del joystick para despertar al ESP32 si se presiona
  esp_sleep_enable_ext0_wakeup((gpio_num_t)J1_BTN_PIN, 0); // 0 = LOW (Pulsado)
  
  // Entrar en modo Deep Sleep (Ahorro de energía total)
  esp_deep_sleep_start();
}


// ==========================================
//   MODO DEMOSTRACIÓN (ATTRACT MODE - IA vs IA)
// ==========================================
void handleAttractMode() {
  // Variables estáticas para mantener el estado de la partida demo
  static float ballX = 64, ballY = 32;
  static float ballDX = 2.5, ballDY = 1.8;
  static float paddle1Y = 24, paddle2Y = 24;
  const int paddleH = 14, paddleW = 2;
  static unsigned long blinkTimer = 0;
  static bool showText = true;

  // --- 1. COMPROBAR ENTRADA DE USUARIO (INTERRUPCIÓN) ---
  int joyY = analogRead(J1_Y_PIN);
  bool btnPressed = (digitalRead(J1_BTN_PIN) == LOW);

  // Si el usuario mueve el joystick o presiona el botón, regresa al menú
  if (joyY < 1000 || joyY > 3000 || btnPressed) {
    lastActivityTime = millis(); // Reset del cronómetro de inactividad
    currentState = STATE_MENU;
    tone(BUZZER_PIN, 800, 30);
    delay(150); // Evitar falsas lecturas
    return;
  }

  // --- 2. LÓGICA DE FÍSICA Y DOS INTELIGENCIAS ARTIFICIALES ---
  ballX += ballDX;
  ballY += ballDY;

  // IA Paleta Izquierda (Sigue la pelota con suavidad)
  if (ballX < 70) {
    if (paddle1Y + (paddleH / 2) < ballY) paddle1Y += 1.5;
    if (paddle1Y + (paddleH / 2) > ballY) paddle1Y -= 1.5;
  }

  // IA Paleta Derecha (Sigue la pelota con suavidad)
  if (ballX > 58) {
    if (paddle2Y + (paddleH / 2) < ballY) paddle2Y += 1.5;
    if (paddle2Y + (paddleH / 2) > ballY) paddle2Y -= 1.5;
  }

  // Limitar paletas dentro de la pantalla
  paddle1Y = constrain(paddle1Y, 0, SCREEN_HEIGHT - paddleH);
  paddle2Y = constrain(paddle2Y, 0, SCREEN_HEIGHT - paddleH);

  // Rebotes en paredes superior e inferior
  if (ballY <= 0 || ballY >= SCREEN_HEIGHT - 2) {
    ballDY *= -1;
    tone(BUZZER_PIN, 600, 10);
  }

  // Rebote Paleta Izquierda
  if (ballX <= (4 + paddleW) && ballY >= paddle1Y && ballY <= paddle1Y + paddleH) {
    ballDX *= -1;
    ballX = 4 + paddleW + 1;
    tone(BUZZER_PIN, 900, 15);
  }

  // Rebote Paleta Derecha
  if (ballX >= (124 - paddleW) && ballY >= paddle2Y && ballY <= paddle2Y + paddleH) {
    ballDX *= -1;
    ballX = 124 - paddleW - 1;
    tone(BUZZER_PIN, 900, 15);
  }

  // Reset de pelota si se marca punto en la demo
  if (ballX < 0 || ballX > SCREEN_WIDTH) {
    ballX = 64;
    ballY = 32;
    ballDX = (random(0, 2) == 0 ? 2.5 : -2.5);
  }

  // --- 3. RENDERIZADO VISUAL ---
  oledMonitor.clearDisplay();

  // Campo de juego (Línea central punteada)
  for (int y = 0; y < SCREEN_HEIGHT; y += 6) {
    oledMonitor.drawFastVLine(64, y, 3, WHITE);
  }

  // Dibujar Paletas y Pelota
  oledMonitor.fillRect(4, (int)paddle1Y, paddleW, paddleH, WHITE);
  oledMonitor.fillRect(124 - paddleW, (int)paddle2Y, paddleW, paddleH, WHITE);
  oledMonitor.fillRect((int)ballX, (int)ballY, 2, 2, WHITE);

  // Overlay Parpadeante Arcade ("DEMO MODE / PRESS START")
  if (millis() - blinkTimer > 500) {
    showText = !showText;
    blinkTimer = millis();
  }

  if (showText) {
    oledMonitor.fillRect(14, 26, 100, 12, BLACK); // Caja de fondo para legibilidad
    oledMonitor.drawRect(14, 26, 100, 12, WHITE);
    oledMonitor.setTextSize(1);
    oledMonitor.setTextColor(WHITE);
    oledMonitor.setCursor(18, 28);
    oledMonitor.print("PRESS ANY BUTTON");
  }

  oledMonitor.display();
  delay(15); // ~60 FPS
}


// ==========================================
//   PANTALLA DE CARGA / SPLASH SCREEN
// ==========================================
void handleSplashScreen() {
  // A. Animación: El título cae desde arriba
  for (int y = -16; y <= 6; y += 2) {
    oledMonitor.clearDisplay();
    
    oledMonitor.setTextSize(2);
    oledMonitor.setTextColor(WHITE);
    oledMonitor.setCursor(22, y);
    oledMonitor.print("PINPONG");
    
    oledMonitor.display();
    delay(10);
  }

  // B. Chime de Audio Retro (Estilo GameBoy / Arcade)
  tone(BUZZER_PIN, 987, 80);   // Nota B5
  delay(90);
  tone(BUZZER_PIN, 1318, 220); // Nota E6
  delay(220);
  noTone(BUZZER_PIN);

  // C. Subtítulo y Marco Decorativo
  oledMonitor.drawFastHLine(14, 25, 100, WHITE);
  
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(26, 31);
  oledMonitor.print("RETRO CONSOLE");

  oledMonitor.setCursor(31, 44);
  oledMonitor.print("CARGANDO...");

  // D. Barra de Carga Animada (Pixel-Art)
  oledMonitor.drawRect(24, 55, 80, 6, WHITE); // Borde exterior
  
  for (int w = 0; w <= 76; w += 4) {
    oledMonitor.fillRect(26, 57, w, 2, WHITE); // Relleno progresivo
    oledMonitor.display();
    delay(25); // Controla la velocidad de carga
  }

  delay(400); // Pausa visual con la barra llena

  // E. Transición limpia al Menú Principal
  currentState = STATE_MENU;
  lastActivityTime = millis(); // Reinicia el temporizador de inactividad (Attract Mode)
}

