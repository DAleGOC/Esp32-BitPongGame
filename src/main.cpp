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
#define J1_X_PIN 32    // Pin analógico para Izquierda/Derecha
// Joystick 2 (Jugador 2)
#define J2_Y_PIN 35    
#define J2_BTN_PIN 26  

// ==========================================
//   MÁQUINA DE ESTADOS DEL JUEGO
// ==========================================
enum GameState {
  STATE_SPLASH,
  STATE_MENU,
  STATE_PLAY,
  STATE_SETTINGS,
  STATE_CREDITS,
  STATE_SLEEP,
  STATE_ATTRACT
};

GameState currentState = STATE_MENU;

// ==========================================
//   INSTANCIAS GLOBALES
// ==========================================
Adafruit_SH1106 oledMonitor(OLED_RESET);

// Variables globales para el control del menú
int currentMenuOption = 0;       // 0 = Jugar, 1 = Configuración, 2 = Créditos, 3 = Salir
const int totalMenuOptions = 4;
bool joystickLocked = false;     // Evita que el menú haga scroll descontrolado
unsigned long lastBtnPress = 0;  // Antirrebote (Debounce) del botón

// Estrellas de fondo para el menú (Parallax)
const int MENU_STARS = 12;
float mStarX[MENU_STARS];
int mStarY[MENU_STARS];
float mStarSpeed[MENU_STARS];
bool starsInitialized = false;

// Animación barra deslizante
int animOffset = abs((int)(millis() / 120) % 4 - 2);

// Temporizador de Inactividad para Attract Mode
unsigned long lastActivityTime = 0; 
const unsigned long INACTIVITY_TIMEOUT = 15000; // 15 segundos

// ==========================================
//   CONFIGURACIÓN GLOBAL DEL JUEGO
// ==========================================
int gameDifficulty = 1;    // 0: FÁCIL, 1: NORMAL, 2: DIFÍCIL
bool soundEnabled  = true;  // true: SONIDO ON, false: SONIDO OFF
int gameMode       = 0;     // 0: 1 JUGADOR (vs IA), 1: 2 JUGADORES (Local)
int scoreLimit     = 5;     // 3, 5, o 10 puntos para ganar

// Navegación dentro del menú de ajustes
int currentSettingOption = 0; // 0: MODO, 1: DIFICULTAD, 2: SONIDO, 3: PUNTOS, 4: VOLVER

// ==========================================
//   BITMAPS PIXEL-ART 7x7 (ICONOS MENÚ)
// ==========================================
const uint8_t PROGMEM icon_play[] = {
  0b01110000,
  0b10001010,
  0b10001000,
  0b01110000,
  0b00100000,
  0b00100000,
  0b00100000
};

const uint8_t PROGMEM icon_settings[] = {
  0b00111000,
  0b01010100,
  0b10000010,
  0b11010110,
  0b10000010,
  0b01010100,
  0b00111000
};

const uint8_t PROGMEM icon_credits[] = {
  0b11111110,
  0b10111010,
  0b01111100,
  0b00111000,
  0b00010000,
  0b00010000,
  0b00111000
};

const uint8_t PROGMEM icon_sleep[] = {
  0b00010000,
  0b01111100,
  0b11010010,
  0b10010010,
  0b10000010,
  0b01000010,
  0b00111000
};

const uint8_t* const menuIcons[] = {
  icon_play, 
  icon_settings, 
  icon_credits, 
  icon_sleep
};

// ==========================================
//   PROTOTIPOS DE FUNCIONES
// ==========================================
void playSound(int frequency, int duration);
void stopSound();
void showStudioScreen();
void showBouncingBallAnimation();
void showCenterExplosion();
void showLoadingBarAnimation();
void starParallaxEffect();
void handleSplashScreen();
void handleMenu();
void handleSleep();
void handleAttractMode();
void animateScreenWipe();
void handleSettings();

// ==========================================
//   CONTROL DE AUDIO CENTRALIZADO
// ==========================================
void playSound(int frequency, int duration) {
  if (soundEnabled) {
    tone(BUZZER_PIN, frequency, duration);
  }
}

void stopSound() {
  noTone(BUZZER_PIN);
}

/**
 * @brief Función principal de configuración.
 */
void setup() {
  Wire.begin(OLED_SDA, OLED_SCL);
  oledMonitor.begin(SH1106_SWITCHCAPVCC, OLED_ADDRESS);
  oledMonitor.clearDisplay();
  oledMonitor.display();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(J1_BTN_PIN, INPUT_PULLUP);

  // --- SECUENCIA DE ARRANQUE (BOOT SEQUENCE) ---
  showStudioScreen();          
  showBouncingBallAnimation(); 
  showCenterExplosion();       
  showLoadingBarAnimation();   
}

/**
 * @brief Bucle principal del juego.
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
      handleSettings();
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
void showStudioScreen() {
  oledMonitor.clearDisplay();
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);

  const char* linea1 = "ARCADE STUDIO";
  const char* linea2 = "PRESENTS";

  oledMonitor.setCursor(26, 20); 
  for(size_t i = 0; i < strlen(linea1); i++) {
    oledMonitor.print(linea1[i]);
    oledMonitor.display();
    playSound(1500 + random(0, 500), 20);
    delay(100); 
  }

  delay(400);

  oledMonitor.setCursor(40, 36);
  for(size_t i = 0; i < strlen(linea2); i++) {
    oledMonitor.print(linea2[i]);
    oledMonitor.display();
    playSound(800 + random(0, 200), 20);
    delay(80);
  }

  delay(200);
  playSound(880, 100);  
  delay(100);
  playSound(1108, 100); 
  delay(100);
  playSound(1318, 400); 
  
  delay(1200); 

  for(int i = 0; i <= 32; i += 2) {
    oledMonitor.fillRect(0, 32 - i, 128, i * 2, BLACK);
    oledMonitor.display();
    delay(15);
  }
  
  stopSound();
  delay(600); 
}

void showBouncingBallAnimation() {
  oledMonitor.clearDisplay();
  oledMonitor.display();

  int ballSize = 2; 
  float ballX = 10; float ballY = 10; 
  float ballDX = 3; float ballDY = 3; 
  float speedMultiplier = 1.0;

  int trailX[10]; int trailY[10];
  for(int i = 0; i < 10; i++) { trailX[i] = -1; trailY[i] = -1; }
  int trailIndex = 0;

  unsigned long startTime = millis();
  unsigned long phaseDuration = 4000;

  while (millis() - startTime < phaseDuration) {
    trailX[trailIndex] = (int)ballX;
    trailY[trailIndex] = (int)ballY;
    trailIndex = (trailIndex + 1) % 10;

    ballX += ballDX * speedMultiplier;
    ballY += ballDY * speedMultiplier;

    if (ballX <= 0 || ballX >= (SCREEN_WIDTH - ballSize)) {
      ballDX *= -1;
      speedMultiplier *= 1.01; 
      playSound(800, 15); 
    }
    if (ballY <= 0 || ballY >= (SCREEN_HEIGHT - ballSize)) {
      ballDY *= -1;
      speedMultiplier *= 1.01; 
      playSound(1000, 15); 
    }

    if (millis() - startTime > 2000) { speedMultiplier += 0.08; }

    oledMonitor.clearDisplay(); 

    for (int i = 0; i < 10; i++) {
      if (trailX[i] != -1) {
        oledMonitor.drawPixel(trailX[i], trailY[i], WHITE);
        oledMonitor.drawPixel(trailX[i]+1, trailY[i], WHITE);
        oledMonitor.drawPixel(trailX[i], trailY[i]+1, WHITE);
        oledMonitor.drawPixel(trailX[i]+1, trailY[i]+1, WHITE);
      }
    }

    oledMonitor.fillRect((int)ballX, (int)ballY, ballSize, ballSize, WHITE);
    oledMonitor.display();
    delay(10);
  }
}

void showCenterExplosion() {
  int centerX = SCREEN_WIDTH / 2;
  int centerY = SCREEN_HEIGHT / 2;

  oledMonitor.clearDisplay(); 
  oledMonitor.display();
  delay(100);

  oledMonitor.fillRect(centerX-1, centerY-1, 2, 2, WHITE);
  oledMonitor.display();
  delay(300); 

  oledMonitor.invertDisplay(true);
  playSound(150, 60); 
  delay(60);
  oledMonitor.invertDisplay(false);

  int maxRadius = 45; 
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
    playSound(random(40, 200), 15); 
    delay(15); 
  }

  stopSound(); 

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

void showLoadingBarAnimation() {
  oledMonitor.setTextSize(2);
  oledMonitor.setTextColor(WHITE);
  
  for (int posY = -20; posY <= 4; posY += 4) {
    oledMonitor.clearDisplay();
    oledMonitor.setCursor(22, posY); 
    oledMonitor.print("PINPONG");
    oledMonitor.display();
    
    playSound(400 + (posY * 10), 10);
    delay(15);
  }
  
  playSound(120, 80);

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

    oledMonitor.fillRect(xPos, yPos, blockWidth, barHeight - (innerPadding * 2), WHITE);
    oledMonitor.display(); 
    
    playSound(1800, 15);
    delay(100 + random(30, 150)); 
  }

  delay(300);

  playSound(988, 100);  
  delay(120);
  playSound(1319, 200); 

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
    delay(20); 
  }

  playSound(200, 100);
  delay(100);
  playSound(150, 200);
  delay(100);

  for (int paso = 0; paso < 8; paso++) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
      for (int x = 0; x < SCREEN_WIDTH; x++) {
        if ((x + (y * 3)) % 8 == paso) {
          oledMonitor.drawPixel(x, y, BLACK);
        }
      }
    }
    oledMonitor.display();
    playSound(random(30, 80), 30); 
    delay(40); 
  }
  
  stopSound(); 

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

  if (!starsInitialized) {
    for (int i = 0; i < MENU_STARS; i++) {
      mStarX[i] = random(0, SCREEN_WIDTH);
      mStarY[i] = random(12, SCREEN_HEIGHT);
      mStarSpeed[i] = random(3, 15) / 10.0;
    }
    starsInitialized = true;
  }

  // 1. LECTURA DEL JOYSTICK
  int joyY = analogRead(J1_Y_PIN); 
  
  if (joyY < 1000 && !joystickLocked) {
    currentMenuOption--;
    if (currentMenuOption < 0) currentMenuOption = totalMenuOptions - 1; 
    playSound(1200, 15); 
    joystickLocked = true;
    lastActivityTime = millis(); 
  }
  else if (joyY > 3000 && !joystickLocked) {
    currentMenuOption++;
    if (currentMenuOption >= totalMenuOptions) currentMenuOption = 0; 
    playSound(1000, 15); 
    joystickLocked = true;
    lastActivityTime = millis(); 
  }
  else if (joyY > 1500 && joyY < 2500) {
    joystickLocked = false; 
  }

  // 2. SELECCIÓN CON BOTÓN
  if (digitalRead(J1_BTN_PIN) == LOW && millis() - lastBtnPress > 300) {
    lastBtnPress = millis();
    lastActivityTime = millis(); 

    playSound(1500, 40);
    delay(40);
    playSound(2000, 100);

    animateScreenWipe();

    if (currentMenuOption == 0) currentState = STATE_PLAY;
    if (currentMenuOption == 1) currentState = STATE_SETTINGS;
    if (currentMenuOption == 2) currentState = STATE_CREDITS;
    if (currentMenuOption == 3) currentState = STATE_SLEEP;
    
    oledMonitor.clearDisplay();
    oledMonitor.display();
    delay(150);
    return; 
  }

  // 3. RENDERIZADO VISUAL
  oledMonitor.clearDisplay();

  for (int i = 0; i < MENU_STARS; i++) {
    mStarX[i] -= mStarSpeed[i];
    if (mStarX[i] < 0) {
      mStarX[i] = SCREEN_WIDTH;
      mStarY[i] = random(14, SCREEN_HEIGHT);
    }
    oledMonitor.drawPixel((int)mStarX[i], mStarY[i], WHITE);
  }

  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(4, 2);
  oledMonitor.print("PINPONG");
  
  oledMonitor.setCursor(100, 2);
  oledMonitor.print("v1.0");
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  int animOffset = abs((int)(millis() / 120) % 4 - 2); 

  const char* options[4] = {"JUGAR", "CONFIGURACION", "CREDITOS", "SALIR"};
  
  for (int i = 0; i < totalMenuOptions; i++) {
    int yPos = 15 + (i * 12);
    
    if (i == currentMenuOption) {
      oledMonitor.setTextColor(WHITE);
      oledMonitor.setCursor(0 + animOffset, yPos);
      oledMonitor.print(">");

      oledMonitor.fillRoundRect(8, yPos - 1, 114, 10, 2, WHITE);
      
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
  
  playSound(1000, 100); delay(120);
  playSound(800, 100);  delay(120);
  playSound(500, 300);  delay(350);
  stopSound();

  for (int h = 32; h > 0; h -= 2) {
    oledMonitor.clearDisplay();
    oledMonitor.drawRect(0, 32 - h, 128, h * 2, WHITE);
    oledMonitor.display();
    delay(10);
  }

  oledMonitor.clearDisplay();
  oledMonitor.fillRect(63, 31, 2, 2, WHITE);
  oledMonitor.display();
  delay(300);

  oledMonitor.clearDisplay();
  oledMonitor.display();

  esp_sleep_enable_ext0_wakeup((gpio_num_t)J1_BTN_PIN, 0); 
  esp_deep_sleep_start();
}

// ==========================================
//   MODO DEMOSTRACIÓN (ATTRACT MODE - IA vs IA)
// ==========================================
void handleAttractMode() {
  static float ballX = 64, ballY = 32;
  static float ballDX = 2.5, ballDY = 1.8;
  static float paddle1Y = 24, paddle2Y = 24;
  const int paddleH = 14, paddleW = 2;
  static unsigned long blinkTimer = 0;
  static bool showText = true;

  int joyY = analogRead(J1_Y_PIN);
  bool btnPressed = (digitalRead(J1_BTN_PIN) == LOW);

  if (joyY < 1000 || joyY > 3000 || btnPressed) {
    lastActivityTime = millis(); 
    currentState = STATE_MENU;
    playSound(800, 30);
    delay(150); 
    return;
  }

  ballX += ballDX;
  ballY += ballDY;

  if (ballX < 70) {
    if (paddle1Y + (paddleH / 2) < ballY) paddle1Y += 1.5;
    if (paddle1Y + (paddleH / 2) > ballY) paddle1Y -= 1.5;
  }

  if (ballX > 58) {
    if (paddle2Y + (paddleH / 2) < ballY) paddle2Y += 1.5;
    if (paddle2Y + (paddleH / 2) > ballY) paddle2Y -= 1.5;
  }

  paddle1Y = constrain(paddle1Y, 0, SCREEN_HEIGHT - paddleH);
  paddle2Y = constrain(paddle2Y, 0, SCREEN_HEIGHT - paddleH);

  if (ballY <= 0 || ballY >= SCREEN_HEIGHT - 2) {
    ballDY *= -1;
    playSound(600, 10);
  }

  if (ballX <= (4 + paddleW) && ballY >= paddle1Y && ballY <= paddle1Y + paddleH) {
    ballDX *= -1;
    ballX = 4 + paddleW + 1;
    playSound(900, 15);
  }

  if (ballX >= (124 - paddleW) && ballY >= paddle2Y && ballY <= paddle2Y + paddleH) {
    ballDX *= -1;
    ballX = 124 - paddleW - 1;
    playSound(900, 15);
  }

  if (ballX < 0 || ballX > SCREEN_WIDTH) {
    ballX = 64;
    ballY = 32;
    ballDX = (random(0, 2) == 0 ? 2.5 : -2.5);
  }

  oledMonitor.clearDisplay();

  for (int y = 0; y < SCREEN_HEIGHT; y += 6) {
    oledMonitor.drawFastVLine(64, y, 3, WHITE);
  }

  oledMonitor.fillRect(4, (int)paddle1Y, paddleW, paddleH, WHITE);
  oledMonitor.fillRect(124 - paddleW, (int)paddle2Y, paddleW, paddleH, WHITE);
  oledMonitor.fillRect((int)ballX, (int)ballY, 2, 2, WHITE);

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
//   PANTALLA DE CARGA / SPLASH SCREEN
// ==========================================
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

  playSound(987, 80);   
  delay(90);
  playSound(1318, 220); 
  delay(220);
  stopSound();

  oledMonitor.drawFastHLine(14, 25, 100, WHITE);
  
  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(26, 31);
  oledMonitor.print("RETRO CONSOLE");

  oledMonitor.setCursor(31, 44);
  oledMonitor.print("CARGANDO...");

  oledMonitor.drawRect(24, 55, 80, 6, WHITE); 
  
  for (int w = 0; w <= 76; w += 4) {
    oledMonitor.fillRect(26, 57, w, 2, WHITE); 
    oledMonitor.display();
    delay(25); 
  }

  delay(400); 

  currentState = STATE_MENU;
  lastActivityTime = millis(); 
}

// ==========================================
//   TRANSICIÓN SUAVE ENTRE PANTALLAS (WIPE)
// ==========================================
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
//   SUBMENÚ DE CONFIGURACIÓN
// ==========================================
void handleSettings() {
  const int totalSettingsOptions = 5;

  int joyY = analogRead(J1_Y_PIN);
  int joyX = analogRead(J1_X_PIN); 

  if (joyY < 1000 && !joystickLocked) {
    currentSettingOption--;
    if (currentSettingOption < 0) currentSettingOption = totalSettingsOptions - 1;
    playSound(1200, 15);
    joystickLocked = true;
  }
  else if (joyY > 3000 && !joystickLocked) {
    currentSettingOption++;
    if (currentSettingOption >= totalSettingsOptions) currentSettingOption = 0;
    playSound(1000, 15);
    joystickLocked = true;
  }

  if ((joyX < 1000 || joyX > 3000) && !joystickLocked) {
    bool moveRight = (joyX > 3000);
    playSound(1400, 20);
    
    switch (currentSettingOption) {
      case 0:
        gameMode = (gameMode == 0) ? 1 : 0;
        break;
      case 1:
        if (moveRight) gameDifficulty = (gameDifficulty + 1) % 3;
        else gameDifficulty = (gameDifficulty == 0) ? 2 : gameDifficulty - 1;
        break;
      case 2:
        soundEnabled = !soundEnabled;
        break;
      case 3:
        if (scoreLimit == 3) scoreLimit = 5;
        else if (scoreLimit == 5) scoreLimit = 10;
        else scoreLimit = 3;
        break;
    }
    joystickLocked = true;
  }

  if (joyY > 1500 && joyY < 2500 && joyX > 1500 && joyX < 2500) {
    joystickLocked = false;
  }

  if (digitalRead(J1_BTN_PIN) == LOW && millis() - lastBtnPress > 300) {
    lastBtnPress = millis();
    playSound(1800, 50);

    if (currentSettingOption == 0) gameMode = (gameMode == 0) ? 1 : 0;
    else if (currentSettingOption == 1) gameDifficulty = (gameDifficulty + 1) % 3;
    else if (currentSettingOption == 2) soundEnabled = !soundEnabled;
    else if (currentSettingOption == 3) {
      if (scoreLimit == 3) scoreLimit = 5;
      else if (scoreLimit == 5) scoreLimit = 10;
      else scoreLimit = 3;
    }
    else if (currentSettingOption == 4) { 
      animateScreenWipe();
      currentState = STATE_MENU;
      lastActivityTime = millis();
      return;
    }
  }

  oledMonitor.clearDisplay();

  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(24, 2);
  oledMonitor.print("AJUSTES DE JUEGO");
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  const char* settingNames[5] = {"MODO", "DIFICULTAD", "AUDIO", "PUNTOS", " <VOLVER"};
  const char* diffLabels[3]   = {"FACIL", "NORMAL", "DIFICIL"};

  int animOffset = abs((int)(millis() / 120) % 4 - 2); 

  for (int i = 0; i < totalSettingsOptions; i++) {
    int yPos = 14 + (i * 10);

    if (i == currentSettingOption) {
      oledMonitor.setCursor(0 + animOffset, yPos);
      oledMonitor.print(">");
      oledMonitor.setTextColor(WHITE);
    } else {
      oledMonitor.setTextColor(WHITE);
    }

    oledMonitor.setCursor(10, yPos);
    oledMonitor.print(settingNames[i]);

    oledMonitor.setCursor(82, yPos);
    switch (i) {
      case 0:
        oledMonitor.print(gameMode == 0 ? "[1P IA]" : "[2P VS]");
        break;
      case 1:
        oledMonitor.print(diffLabels[gameDifficulty]);
        break;
      case 2:
        oledMonitor.print(soundEnabled ? "[ON]" : "[OFF]");
        break;
      case 3:
        oledMonitor.print("[");
        oledMonitor.print(scoreLimit);
        oledMonitor.print(" PTS]");
        break;
      case 4:
        break;
    }
  }

  oledMonitor.display();
}