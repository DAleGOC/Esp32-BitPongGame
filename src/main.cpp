/**
 * @file      PinPong_Arcade.ino
 * @author    [Tu Nombre / Arcade Studio]
 * @brief     Secuencia de inicio y menús para consola retro ESP32.
 *            Incluye física de audio independiente (Música/SFX),
 *            inversión de controles y submenú de preparación de partida.
 * @hardware  ESP32, Pantalla OLED SH1106 (I2C), Zumbador Pasivo + Transistor 2N2222.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#include <Preferences.h>


Preferences preferences;



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
#define J1_Y_PIN 34    // Pin analógico para Arriba/Abajo
#define J1_BTN_PIN 25  // Pin digital para el botón (Push button)
#define J1_X_PIN 32    // Pin analógico para Izquierda/Derecha
#define J2_Y_PIN 35    
#define J2_BTN_PIN 26  

// ==========================================
//   MÁQUINA DE ESTADOS DEL JUEGO
// ==========================================
enum GameState {
  STATE_SPLASH,
  STATE_MENU,
  STATE_MATCH_SETUP,
  STATE_PLAY,
  STATE_SETTINGS,
  STATE_CREDITS,
  STATE_SLEEP,
  STATE_ATTRACT,
};

GameState currentState = STATE_MENU;

// ==========================================
//   ESTRUCTURA Y MOTOR DE AUDIO NO BLOQUEANTE
// ==========================================
struct Note {
  int frequency; // Frecuencia en Hz (0 = Pausa/Silencio)
  int duration;  // Duración en ms
};

// Melodía Chiptune Retro en Bucle
const Note menuMusic[] = {
  {262, 120}, {330, 120}, {392, 120}, {523, 200}, {0, 60},
  {349, 120}, {440, 120}, {523, 120}, {659, 200}, {0, 60},
  {294, 120}, {392, 120}, {494, 120}, {587, 200}, {0, 60},
  {392, 120}, {494, 120}, {587, 120}, {784, 240}, {0, 100}
};

const int menuMusicLength = sizeof(menuMusic) / sizeof(menuMusic[0]);

int musicIndex = 0;             // Índice de la nota actual
unsigned long nextNoteTime = 0; // Temporizador para la siguiente nota
unsigned long sfxEndTime = 0;   // Bloqueo de prioridad para efectos de sonido

// ==========================================
//   INSTANCIAS Y VARIABLES GLOBALES
// ==========================================
Adafruit_SH1106 oledMonitor(OLED_RESET);

int currentMenuOption = 0;       
const int totalMenuOptions = 4;
bool joystickLocked = false;     
unsigned long lastBtnPress = 0;  

// Estrellas de fondo para el menú (Parallax)
const int MENU_STARS = 12;
float mStarX[MENU_STARS];
int mStarY[MENU_STARS];
float mStarSpeed[MENU_STARS];
bool starsInitialized = false;

// Temporizador de Inactividad para Attract Mode
unsigned long lastActivityTime = 0; 
const unsigned long INACTIVITY_TIMEOUT = 15000; 

// CONFIGURACIÓN GLOBAL Y DE SISTEMA
bool musicEnabled   = true;  // Música de fondo (ON/OFF)
bool sfxEnabled     = true;  // Efectos de sonido (ON/OFF)
bool invertControls = false; // Inversión del eje Y del Joystick (SI/NO)

// CONFIGURACIÓN DE LA PARTIDA
int gameDifficulty = 1;   // 0: FACIL, 1: NORMAL, 2: DIFICIL
int gameMode       = 0;    // 0: 1P vs IA, 1: 2P VS
int scoreLimit     = 5;    // 3, 5, 10 Puntos

// Navegación de Submenús
int currentSetupOption   = 0; 
int currentSettingOption = 0; 

// ==========================================
//   BITMAPS PIXEL-ART 7x7 (ICONOS MENÚ)
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

const uint8_t* const menuIcons[] = {
  icon_play, icon_settings, icon_credits, icon_sleep
};

// ==========================================
//   PROTOTIPOS DE FUNCIONES
// ==========================================
int readJoystickY();
void showStudioScreen();
void showBouncingBallAnimation();
void showCenterExplosion();
void showLoadingBarAnimation();

void handleSplashScreen();
void handleMenu();
void handleMatchSetup();
void handleSettings();
void handleSleep();
void handleAttractMode();

void animateScreenWipe();
void playSFX(int frequency, int duration);
void updateAudio();
void stopAudio();

// ==========================================
//   HELPER DE LECTURA CON INVERSIÓN
// ==========================================
int readJoystickY() {
  int rawY = analogRead(J1_Y_PIN);
  return invertControls ? (4095 - rawY) : rawY;
}

// ==========================================
//   SETUP Y LOOP
// ==========================================
void setup() {
  Wire.begin(OLED_SDA, OLED_SCL);
  oledMonitor.begin(SH1106_SWITCHCAPVCC, OLED_ADDRESS);
  oledMonitor.clearDisplay();
  oledMonitor.display();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(J1_BTN_PIN, INPUT_PULLUP);

  // Secuencia de Arranque
  showStudioScreen();          
  showBouncingBallAnimation(); 
  showCenterExplosion();       
  showLoadingBarAnimation();   
  
  lastActivityTime = millis();
}

void loop() {
  // Motor de audio continuo en segundo plano
  updateAudio();

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
      // Próxima sección: Lógica del juego
      break;

    case STATE_SETTINGS:
      handleSettings();
      break;

    case STATE_CREDITS:
      break;

    case STATE_SLEEP:
      handleSleep();
      break;
  }
}

// ==========================================
//   SISTEMA DE AUDIO CENTRALIZADO
// ==========================================
void playSFX(int frequency, int duration) {
  if (!sfxEnabled) return;
  sfxEndTime = millis() + duration; 
  tone(BUZZER_PIN, frequency, duration);
}

void stopAudio() {
  noTone(BUZZER_PIN);
}

void updateAudio() {
  // 1. Si hay un efecto de sonido activo, darle prioridad
  if (millis() < sfxEndTime) {
    return; 
  }

  // 2. Si la música está apagada o no estamos en Menú/Setup, detener tono
  if (!musicEnabled || (currentState != STATE_MENU && currentState != STATE_MATCH_SETUP)) {
    return;
  }

  // 3. Temporizador no bloqueante para las notas de la música
  if (millis() >= nextNoteTime) {
    int freq = menuMusic[musicIndex].frequency;
    int dur  = menuMusic[musicIndex].duration;

    if (freq > 0) {
      tone(BUZZER_PIN, freq, dur);
    } else {
      noTone(BUZZER_PIN);
    }

    nextNoteTime = millis() + dur + 20; // 20ms de espacio entre notas
    musicIndex = (musicIndex + 1) % menuMusicLength;
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
  for(int i = 0; i < strlen(linea1); i++) {
    oledMonitor.print(linea1[i]);
    oledMonitor.display();
    playSFX(1500 + random(0, 500), 20);
    delay(100); 
  }

  delay(400); 

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

  for(int i = 0; i <= 32; i += 2) {
    oledMonitor.fillRect(0, 32 - i, 128, i * 2, BLACK);
    oledMonitor.display();
    delay(15);
  }
  
  stopAudio();
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
      playSFX(800, 15); 
    }
    if (ballY <= 0 || ballY >= (SCREEN_HEIGHT - ballSize)) {
      ballDY *= -1;
      speedMultiplier *= 1.01; 
      playSFX(1000, 15); 
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
  playSFX(150, 60); 
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
    playSFX(random(40, 200), 15); 
    delay(15); 
  }

  stopAudio(); 

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

  playSFX(200, 100); delay(100);
  playSFX(150, 200); delay(100);

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
//   LÓGICA E INTERFAZ DEL MENÚ PRINCIPAL
// ==========================================
void handleMenu() {
  if (!starsInitialized) {
    for (int i = 0; i < MENU_STARS; i++) {
      mStarX[i] = random(0, SCREEN_WIDTH);
      mStarY[i] = random(12, SCREEN_HEIGHT);
      mStarSpeed[i] = random(3, 15) / 10.0;
    }
    starsInitialized = true;
  }

  // 1. LECTURA DEL JOYSTICK
  int joyY = readJoystickY(); 
  
  if (joyY < 1000 && !joystickLocked) {
    currentMenuOption--;
    if (currentMenuOption < 0) currentMenuOption = totalMenuOptions - 1; 
    playSFX(1200, 25); 
    joystickLocked = true;
    lastActivityTime = millis(); 
  }
  else if (joyY > 3000 && !joystickLocked) {
    currentMenuOption++;
    if (currentMenuOption >= totalMenuOptions) currentMenuOption = 0; 
    playSFX(1000, 25); 
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

    playSFX(1500, 40);
    delay(40);
    playSFX(2000, 100);

    animateScreenWipe();

    if (currentMenuOption == 0) {
      currentState = STATE_MATCH_SETUP;
      currentSetupOption = 0;
    }
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
//   SUBMENÚ: PREPARACIÓN DE PARTIDA
// ==========================================
void handleMatchSetup() {
  const int totalSetupOptions = 5;

  int joyY = readJoystickY();
  int joyX = analogRead(J1_X_PIN);

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

  if ((joyX < 1000 || joyX > 3000) && !joystickLocked) {
    bool moveRight = (joyX > 3000);
    playSFX(1400, 20);
    lastActivityTime = millis();

    switch (currentSetupOption) {
      case 0:
        gameMode = (gameMode == 0) ? 1 : 0;
        break;
      case 1:
        if (moveRight) gameDifficulty = (gameDifficulty + 1) % 3;
        else gameDifficulty = (gameDifficulty == 0) ? 2 : gameDifficulty - 1;
        break;
      case 2:
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
    lastActivityTime = millis();

    if (currentSetupOption == 0) {
      gameMode = (gameMode == 0) ? 1 : 0;
      playSFX(1400, 20);
    }
    else if (currentSetupOption == 1) {
      gameDifficulty = (gameDifficulty + 1) % 3;
      playSFX(1400, 20);
    }
    else if (currentSetupOption == 2) {
      if (scoreLimit == 3) scoreLimit = 5;
      else if (scoreLimit == 5) scoreLimit = 10;
      else scoreLimit = 3;
      playSFX(1400, 20);
    }
    else if (currentSetupOption == 3) { // ¡EMPEZAR PARTIDA!
      playSFX(1800, 80);
      delay(80);
      playSFX(2200, 150);
      animateScreenWipe();
      stopAudio(); 
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

  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(18, 2);
  oledMonitor.print("NUEVA PARTIDA");
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  const char* diffLabels[3] = {"FACIL", "NORMAL", "DIFICIL"};
  int animOffset = abs((int)(millis() / 120) % 4 - 2);

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
      if (gameMode == 1) oledMonitor.print("[---]");
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
//   SUBMENÚ DE CONFIGURACIÓN DEL SISTEMA (ACTUALIZADO)
// ==========================================
void handleSettings() {
  const int totalSettingsOptions = 4; // 0: MUSICA, 1: SFX, 2: INVERTIR, 3: VOLVER

  int joyY = readJoystickY();

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

  if (joyY > 1500 && joyY < 2500) {
    joystickLocked = false;
  }

  if (digitalRead(J1_BTN_PIN) == LOW && millis() - lastBtnPress > 300) {
    lastBtnPress = millis();
    lastActivityTime = millis();

    if (currentSettingOption == 0) { // MÚSICA DE FONDO
      musicEnabled = !musicEnabled;
      if (!musicEnabled) stopAudio(); 
      else playSFX(1400, 30);
    }
    else if (currentSettingOption == 1) { // EFECTOS DE SONIDO
      sfxEnabled = !sfxEnabled;
      playSFX(1400, 30);
    }
    else if (currentSettingOption == 2) { // INVERTIR CONTROLES
      invertControls = !invertControls;
      playSFX(1400, 30);
    }
    else if (currentSettingOption == 3) { // VOLVER
      playSFX(800, 40);
      animateScreenWipe();
      currentState = STATE_MENU;
      return;
    }
  }

  oledMonitor.clearDisplay();

  oledMonitor.setTextSize(1);
  oledMonitor.setTextColor(WHITE);
  oledMonitor.setCursor(24, 2);
  oledMonitor.print("CONFIGURACION");
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  int animOffset = abs((int)(millis() / 120) % 4 - 2); 

  const char* settingNames[4] = {"MUSICA", "EFECTOS SFX", "INVERTIR Y", "< VOLVER"};

  for (int i = 0; i < totalSettingsOptions; i++) {
    int yPos = 15 + (i * 11);

    if (i == currentSettingOption) {
      oledMonitor.setCursor(0 + animOffset, yPos);
      oledMonitor.print(">");
    }

    oledMonitor.setCursor(10, yPos);
    oledMonitor.print(settingNames[i]);

    oledMonitor.setCursor(88, yPos);
    switch (i) {
      case 0:
        oledMonitor.print(musicEnabled ? "[ON]" : "[OFF]");
        break;
      case 1:
        oledMonitor.print(sfxEnabled ? "[ON]" : "[OFF]");
        break;
      case 2:
        oledMonitor.print(invertControls ? "[SI]" : "[NO]");
        break;
      case 3:
        break;
    }
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
  
  playSFX(1000, 100); delay(120);
  playSFX(800, 100);  delay(120);
  playSFX(500, 300);  delay(350);
  stopAudio();

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

  int joyY = readJoystickY();
  bool btnPressed = (digitalRead(J1_BTN_PIN) == LOW);

  if (joyY < 1000 || joyY > 3000 || btnPressed) {
    lastActivityTime = millis(); 
    currentState = STATE_MENU;
    musicIndex = 0;
    nextNoteTime = millis();
    playSFX(800, 30);
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
    playSFX(600, 10);
  }

  if (ballX <= (4 + paddleW) && ballY >= paddle1Y && ballY <= paddle1Y + paddleH) {
    ballDX *= -1;
    ballX = 4 + paddleW + 1;
    playSFX(900, 15);
  }

  if (ballX >= (124 - paddleW) && ballY >= paddle2Y && ballY <= paddle2Y + paddleH) {
    ballDX *= -1;
    ballX = 124 - paddleW - 1;
    playSFX(900, 15);
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