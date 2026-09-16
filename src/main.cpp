/**
 * @file      PinPong_Arcade.ino
 * @author    Arcade Studio
 * @brief     Secuencia de inicio y menús para consola retro ESP32.
 *            Incluye persistencia NVS (Flash), física de audio,
 *            inversión de controles, submenú de preparación de partida,
 *            control de brillo, mejores puntajes, multibola y portales.
 * @hardware  ESP32, Pantalla OLED SH1106 (I2C), Zumbador Pasivo + Transistor 2N2222.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#include <Preferences.h>
#include <Fonts/Picopixel.h>

Preferences prefs;

// ==========================================
//   CONFIGURACIÓN DE HARDWARE Y PINES
// ==========================================
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
#define J1_Y_PIN 34    
#define J1_BTN_PIN 25  
#define J1_X_PIN 32    
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
  STATE_POINT_SCORED,
  STATE_PAUSE,
  STATE_GAME_OVER,
  STATE_SETTINGS,
  STATE_CREDITS,
  STATE_SCORES,
  STATE_SLEEP,
  STATE_ATTRACT
};

GameState currentState = STATE_MENU;

// ==========================================
//   ESTRUCTURA Y MOTOR DE AUDIO NO BLOQUEANTE
// ==========================================
struct Note {
  int frequency;
  int duration; 
};

const Note menuMusic[] = {
  {262, 120}, {330, 120}, {392, 120}, {523, 200}, {0, 60},
  {349, 120}, {440, 120}, {523, 120}, {659, 200}, {0, 60},
  {294, 120}, {392, 120}, {494, 120}, {587, 200}, {0, 60},
  {392, 120}, {494, 120}, {587, 120}, {784, 240}, {0, 100}
};
const int menuMusicLength = sizeof(menuMusic) / sizeof(menuMusic[0]);

const Note gameMusic[] = {
  {523, 80}, {587, 80}, {659, 80}, {698, 80}, {784, 120}, {0, 40},
  {659, 80}, {523, 80}, {587, 80}, {392, 160}, {0, 40},
  {440, 80}, {494, 80}, {523, 80}, {587, 80}, {659, 120}, {0, 40},
  {587, 80}, {494, 80}, {523, 80}, {392, 160}, {0, 40}
};
const int gameMusicLength = sizeof(gameMusic) / sizeof(gameMusic[0]);

int musicIndex = 0;             
unsigned long nextNoteTime = 0; 
unsigned long sfxEndTime = 0;   

// ==========================================
//   INSTANCIAS Y VARIABLES GLOBALES
// ==========================================
Adafruit_SH1106 oledMonitor(OLED_RESET);

int currentMenuOption = 0;       
const int totalMenuOptions = 5;
bool joystickLocked = false;     
unsigned long lastBtnPress = 0;  

// Estrellas de fondo
const int MENU_STARS = 12;
float mStarX[MENU_STARS];
int mStarY[MENU_STARS];
float mStarSpeed[MENU_STARS];
bool starsInitialized = false;

// Attract Mode
unsigned long lastActivityTime = 0; 
const unsigned long INACTIVITY_TIMEOUT = 15000; 

// CONFIGURACIÓN GLOBAL
bool musicEnabled   = true;  
bool sfxEnabled     = true;  
bool invertControls = false; 
int screenBrightness = 255;  

// CONFIGURACIÓN DE LA PARTIDA
int gameDifficulty = 1;   
int gameMode       = 0;   
int scoreLimit     = 5;   

int currentSetupOption   = 0; 
int currentSettingOption = 0; 

// Puntuaciones
int highScore = 0;
int wins_1p_ia = 0; 
int wins_ia = 0;    
int wins_p1_vs = 0; 
int wins_p2_vs = 0; 

// ==========================================
//   MOTOR FÍSICO
// ==========================================
float paddle1Y = 24; 
float paddle2Y = 24; 
const int PADDLE_H = 14; 
const int PADDLE_W = 2;  
float paddleSpeed = 2.5; 

const int BALL_SIZE = 2;
int scoreP1 = 0;
int scoreP2 = 0;

const int HITS_FOR_NEXT_LEVEL = 10; 
const float BALL_SPEED_LVL1 = 2.0;
const float BALL_SPEED_LVL2 = 3.5;
const float BALL_SPEED_LVL3 = 5.0;

int consecutiveHits = 0;   
int currentSpeedLevel = 1; 

unsigned long stateTimer = 0;   
int gameOverOption = 0;         
unsigned long pauseButtonTimer = 0; 
bool isPauseButtonPressed = false; 
int pauseOption = 0; 
GameState settingsReturnState = STATE_MENU; 

// Estela y Shake
float trailX[3], trailY[3]; 
int shakeFrames = 0;        

// POWER-UPS
bool powerUpActive = false;
float powerUpX = 0, powerUpY = 0;
int powerUpType = 0;             
unsigned long powerUpSpawnTimer = 0;
unsigned long powerUpDurationTimer = 0;

int lastPlayerToHit = 0;         
int activePowerUpPlayer = 0;     
int activePowerUpType = -1;      

int currentPaddle1_H = PADDLE_H; 
int currentPaddle2_H = PADDLE_H;
bool ghostBallActive = false;
bool invertedControlsP1 = false;
bool invertedControlsP2 = false; 

// ESTRUCTURA MULTIPELOTA
struct Ball {
  float x, y;
  float dx, dy;
  float spin;
  bool active;
};

Ball balls[2]; 

// PORTALES
struct Portal {
  int x, y, w, h;
  bool active;
  unsigned long spawnTime;
};

Portal activePortal = {0, 0, 8, 16, false, 0};
unsigned long nextPortalTimer = 0;

// ==========================================
//   BITMAPS PIXEL-ART
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

  loadSettings();

  showStudioScreen();         
  showBouncingBallAnimation(); 
  showCenterExplosion();       
  showLoadingBarAnimation();   
  
  lastActivityTime = millis();
}

void loop() {
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
void playSFX(int frequency, int duration) {
  if (!sfxEnabled) return;
  sfxEndTime = millis() + duration; 
  tone(BUZZER_PIN, frequency, duration);
}

void stopAudio() {
  noTone(BUZZER_PIN);
}

void updateAudio() {
  if (millis() < sfxEndTime) return; 
  if (!musicEnabled) return;

  const Note* currentTrack = NULL;
  int trackLength = 0;

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

  if (millis() >= nextNoteTime) {
    int freq = currentTrack[musicIndex % trackLength].frequency;
    int dur  = currentTrack[musicIndex % trackLength].duration;

    if (freq > 0) tone(BUZZER_PIN, freq, dur);
    else noTone(BUZZER_PIN);

    nextNoteTime = millis() + dur + 15;
    musicIndex = (musicIndex + 1) % trackLength;
  }
}

// ==========================================
//   ANIMACIONES DE ENTRADA
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

  int bSize = 2; 
  float bX = 10, bY = 10; 
  float bDX = 3, bDY = 3; 
  float speedMult = 1.0;

  int tX[10], tY[10];
  for(int i = 0; i < 10; i++) { tX[i] = -1; tY[i] = -1; }
  int tIndex = 0;

  unsigned long startTime = millis();

  while (millis() - startTime < 4000) {
    tX[tIndex] = (int)bX;
    tY[tIndex] = (int)bY;
    tIndex = (tIndex + 1) % 10;

    bX += bDX * speedMult;
    bY += bDY * speedMult;

    if (bX <= 0 || bX >= (SCREEN_WIDTH - bSize)) {
      bDX *= -1;
      speedMult *= 1.01; 
      playSFX(800, 15); 
    }
    if (bY <= 0 || bY >= (SCREEN_HEIGHT - bSize)) {
      bDY *= -1;
      speedMult *= 1.01; 
      playSFX(1000, 15); 
    }

    if (millis() - startTime > 2000) { speedMult += 0.08; } 

    oledMonitor.clearDisplay(); 

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
      oledMonitor.print("Welcome!");
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
//   MENÚ PRINCIPAL
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
    if (currentMenuOption == 2) currentState = STATE_SCORES;
    if (currentMenuOption == 3) currentState = STATE_CREDITS;
    if (currentMenuOption == 4) currentState = STATE_SLEEP;
    
    oledMonitor.clearDisplay();
    oledMonitor.display();
    delay(150);
    return; 
  }

  oledMonitor.clearDisplay();
  headerHud();

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
  
  oledMonitor.setCursor(70, 2);
  oledMonitor.print("HI:");
  oledMonitor.print(highScore);
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

  int animOffset = abs((int)(millis() / 120) % 4 - 2); 

  const char* options[5] = {"JUGAR", "CONFIGURACION", "PUNTAJES", "CREDITOS", "SALIR"};
  
  for (int i = 0; i < totalMenuOptions; i++) {
    int yPos = 15 + (i * 10);
    
    if (i == currentMenuOption) {
      oledMonitor.setTextColor(WHITE);
      oledMonitor.setCursor(0 + animOffset, yPos);
      oledMonitor.print(">");

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
void handleScores() {
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
  oledMonitor.setCursor(24, 2);
  oledMonitor.print("ESTADISTICAS");
  oledMonitor.drawFastHLine(0, 11, 128, WHITE);

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

  oledMonitor.setFont();
  oledMonitor.display();
}

// ==========================================
//   SUBMENÚ: CRÉDITOS
// ==========================================
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
        saveSettings();
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
      saveSettings();
    }
    else if (currentSetupOption == 2) {
      if (scoreLimit == 3) scoreLimit = 5;
      else if (scoreLimit == 5) scoreLimit = 10;
      else scoreLimit = 3;
      playSFX(1400, 20);
    }
    else if (currentSetupOption == 3) { 
      playSFX(1800, 80);
      delay(80);
      playSFX(2200, 150);
      animateScreenWipe();
      stopAudio(); 

      // INICIALIZAR ESTADO DE LA PARTIDA
      scoreP1 = 0; scoreP2 = 0;
      paddle1Y = 24; paddle2Y = 24;
      consecutiveHits = 0; currentSpeedLevel = 1;
      resetMainBall(1.0);

      currentState = STATE_PLAY;
      return;
    }
    else if (currentSetupOption == 4) { 
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
//   SUBMENÚ: CONFIGURACIÓN DEL SISTEMA
// ==========================================
void handleSettings() {
  const int totalSettingsOptions = 5; 

  int joyY = readJoystickY();
  int joyX = analogRead(J1_X_PIN);

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

  if (digitalRead(J1_BTN_PIN) == LOW && millis() - lastBtnPress > 300) {
    lastBtnPress = millis();
    lastActivityTime = millis();

    if (currentSettingOption == 0) { 
      musicEnabled = !musicEnabled;
      if (!musicEnabled) stopAudio(); 
      else playSFX(1400, 30);
      saveSettings();
    }
    else if (currentSettingOption == 1) { 
      sfxEnabled = !sfxEnabled;
      playSFX(1400, 30);
      saveSettings();
    }
    else if (currentSettingOption == 2) { 
      invertControls = !invertControls;
      playSFX(1400, 30);
      saveSettings();
    }
    else if (currentSettingOption == 4) { 
      playSFX(800, 40);
      animateScreenWipe();
      currentState = settingsReturnState;
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
    else if (i == 3) {
      oledMonitor.drawRect(86, yPos, 30, 7, WHITE);
      oledMonitor.fillRect(86, yPos, map(screenBrightness, 0, 255, 0, 30), 7, WHITE);
    }
  }

  oledMonitor.display();
}

// ==========================================
//   SECUENCIA DE APAGADO
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
//   MODO DEMOSTRACIÓN (ATTRACT MODE)
// ==========================================
void handleAttractMode() {
  static float demoX = 64, demoY = 32;
  static float demoDX = 2.5, demoDY = 1.8;
  static float demoP1Y = 24, demoP2Y = 24;
  const int pH = 14, pW = 2;
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

  demoX += demoDX;
  demoY += demoDY;

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

  if (demoY <= 0 || demoY >= SCREEN_HEIGHT - 2) {
    demoDY *= -1;
    playSFX(600, 10);
  }

  if (demoX <= (4 + pW) && demoY >= demoP1Y && demoY <= demoP1Y + pH) {
    demoDX *= -1;
    demoX = 4 + pW + 1;
    playSFX(900, 15);
  }

  if (demoX >= (124 - pW) && demoY >= demoP2Y && demoY <= demoP2Y + pH) {
    demoDX *= -1;
    demoX = 124 - pW - 1;
    playSFX(900, 15);
  }

  if (demoX < 0 || demoX > SCREEN_WIDTH) {
    demoX = 64; demoY = 32;
    demoDX = (random(0, 2) == 0 ? 2.5 : -2.5);
  }

  oledMonitor.clearDisplay();

  for (int y = 0; y < SCREEN_HEIGHT; y += 6) {
    oledMonitor.drawFastVLine(64, y, 3, WHITE);
  }

  oledMonitor.fillRect(4, (int)demoP1Y, pW, pH, WHITE);
  oledMonitor.fillRect(124 - pW, (int)demoP2Y, pW, pH, WHITE);
  oledMonitor.fillRect((int)demoX, (int)demoY, 2, 2, WHITE);

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
void loadSettings() {
  prefs.begin("arcade", true);

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
  
  prefs.end();
  setOledBrightness(screenBrightness);
}

void saveSettings() {
  prefs.begin("arcade", false);

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

void checkAndSaveHighScore(int currentScore) {
  if (currentScore > highScore) {
    highScore = currentScore;
    prefs.begin("arcade", false);
    prefs.putInt("hscore", highScore);
    prefs.end();
  }
}

void setOledBrightness(uint8_t brightness) {
  Wire.beginTransmission(OLED_ADDRESS);
  Wire.write(0x00);         
  Wire.write(0x81);         
  Wire.write(brightness);   
  Wire.endTransmission();
}

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
void handlePlay() {
  oledMonitor.clearDisplay();

  // --- BOTÓN DE PAUSA ---
  if (digitalRead(J1_BTN_PIN) == LOW) {
    if (!isPauseButtonPressed) {
      isPauseButtonPressed = true;
      pauseButtonTimer = millis(); 
    } 
    else if (millis() - pauseButtonTimer > 1600) {
      isPauseButtonPressed = true; 
      pauseOption = 0;             
      currentState = STATE_PAUSE;  
      stopAudio();                 
      return;                      
    }
  } else {
    isPauseButtonPressed = false; 
  }

  // --- TEMBLOR DE PANTALLA ---
  int shakeOffset = 0;
  if (shakeFrames > 0) {
    shakeOffset = random(-2, 3);
    shakeFrames--; 
  }

  // 1. DIBUJAR LA CANCHA
  for (int y = 0; y < SCREEN_HEIGHT; y += 6) {
    oledMonitor.drawFastVLine(64, y + shakeOffset, 3, WHITE);
  }

  // 2. MARCADORES E INDICADOR DE PODERES
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

  // 3. MOVIMIENTO DE JUGADORES (J1 Y J2 / CPU)
  int joy1Y = readJoystickY(); 
  if (!invertedControlsP1) {
    if (joy1Y < 1000) paddle1Y -= paddleSpeed;
    if (joy1Y > 3000) paddle1Y += paddleSpeed;
  } else { 
    if (joy1Y < 1000) paddle1Y += paddleSpeed; 
    if (joy1Y > 3000) paddle1Y -= paddleSpeed;
  }

  int joy2Y = analogRead(J2_Y_PIN);
  if (gameMode == 1) {
    if (invertControls) joy2Y = 4095 - joy2Y; 
    if (!invertedControlsP2) {
      if (joy2Y < 1000) paddle2Y -= paddleSpeed;
      if (joy2Y > 3000) paddle2Y += paddleSpeed;
    } else { 
      if (joy2Y < 1000) paddle2Y += paddleSpeed;
      if (joy2Y > 3000) paddle2Y -= paddleSpeed;
    }
  } else {
    // IA CPU: Rastrea la pelota relevante
    float targetY = 32;
    if (balls[0].active && balls[1].active) {
      targetY = (balls[0].x > balls[1].x) ? balls[0].y : balls[1].y;
    } else if (balls[0].active) {
      targetY = balls[0].y;
    } else if (balls[1].active) {
      targetY = balls[1].y;
    }

    float aiSpeedMultiplier = (gameDifficulty == 0) ? 0.4 : ((gameDifficulty == 1) ? 0.7 : 1.0);
    if (!invertedControlsP2) {
      if (paddle2Y + (currentPaddle2_H / 2) < targetY) paddle2Y += (paddleSpeed * aiSpeedMultiplier);
      if (paddle2Y + (currentPaddle2_H / 2) > targetY) paddle2Y -= (paddleSpeed * aiSpeedMultiplier);
    } else {
      if (paddle2Y + (currentPaddle2_H / 2) < targetY) paddle2Y -= (paddleSpeed * aiSpeedMultiplier);
      if (paddle2Y + (currentPaddle2_H / 2) > targetY) paddle2Y += (paddleSpeed * aiSpeedMultiplier);
    }
  }

  paddle1Y = constrain(paddle1Y, 0, SCREEN_HEIGHT - currentPaddle1_H);
  paddle2Y = constrain(paddle2Y, 0, SCREEN_HEIGHT - currentPaddle2_H);

  // 4. CAJAS MISTERIOSAS
  if (!powerUpActive && activePowerUpType == -1 && millis() - powerUpSpawnTimer > random(10000, 15000)) {
    powerUpActive = true;
    powerUpX = random(30, 90); 
    powerUpY = random(10, SCREEN_HEIGHT - 10);
    powerUpType = random(0, 5);
  }

  if (powerUpActive) {
    if ((millis() / 150) % 2 == 0) {
      oledMonitor.drawRect((int)powerUpX, (int)powerUpY + shakeOffset, 6, 6, WHITE);
      oledMonitor.drawPixel((int)powerUpX + 2, (int)powerUpY + 2 + shakeOffset, WHITE);
    }
    
    for (int b = 0; b < 2; b++) {
      if (!balls[b].active) continue;
      if (balls[b].x + BALL_SIZE >= powerUpX && balls[b].x <= powerUpX + 6 && 
          balls[b].y + BALL_SIZE >= powerUpY && balls[b].y <= powerUpY + 6) {
          
          powerUpActive = false;
          playSFX(1800, 150); 
          
          if (lastPlayerToHit > 0) {
            activePowerUpPlayer = lastPlayerToHit;
            activePowerUpType = powerUpType;
            powerUpDurationTimer = millis();

            if (powerUpType == 0) { 
              if (activePowerUpPlayer == 1) currentPaddle1_H = PADDLE_H * 2; else currentPaddle2_H = PADDLE_H * 2;
            } else if (powerUpType == 1) { 
              if (activePowerUpPlayer == 1) currentPaddle2_H = PADDLE_H / 2; else currentPaddle1_H = PADDLE_H / 2;
            } else if (powerUpType == 2) { 
              shakeFrames = 10;
              balls[b].dx = (activePowerUpPlayer == 1) ? 6.0 : -6.0;
            } else if (powerUpType == 3) { 
              ghostBallActive = true;
            } else if (powerUpType == 4) { 
              if (activePowerUpPlayer == 1) invertedControlsP2 = true; else invertedControlsP1 = true;
            }
          }
          break;
      }
    }
  }

  if (activePowerUpType != -1 && millis() - powerUpDurationTimer > 5000) {
      currentPaddle1_H = PADDLE_H; currentPaddle2_H = PADDLE_H;
      ghostBallActive = false;
      invertedControlsP1 = false; invertedControlsP2 = false;
      activePowerUpType = -1;
      powerUpSpawnTimer = millis(); 
      playSFX(400, 100); 
  }

  // 5. ZONA PORTAL
  if (!activePortal.active && millis() - nextPortalTimer > random(8000, 15000)) {
    activePortal.active = true;
    activePortal.x = random(52, 72); 
    activePortal.y = random(8, SCREEN_HEIGHT - 20);
    activePortal.w = 6;
    activePortal.h = 16;
    activePortal.spawnTime = millis();
  }

  if (activePortal.active) {
    if (millis() - activePortal.spawnTime > 6000) {
      activePortal.active = false;
      nextPortalTimer = millis();
    } else {
      if ((millis() / 100) % 2 == 0) {
        oledMonitor.drawRect(activePortal.x, activePortal.y + shakeOffset, activePortal.w, activePortal.h, WHITE);
      }
    }
  }

  // 6. GENERAR MULTIPELOTA
  if (!balls[1].active && consecutiveHits > 6 && random(0, 100) < 5) {
    spawnSecondaryBall();
  }

  // 7. DIBUJAR ESTELA (DE LA PELOTA PRINCIPAL)
  if (balls[0].active) {
    trailX[2] = trailX[1]; trailY[2] = trailY[1];
    trailX[1] = trailX[0]; trailY[1] = trailY[0];
    trailX[0] = balls[0].x; trailY[0] = balls[0].y;

    if ((currentSpeedLevel > 1 || abs(balls[0].spin) > 0.05) && (!ghostBallActive || (millis() / 200) % 2 == 0)) {
      oledMonitor.drawPixel((int)trailX[0], (int)trailY[0] + shakeOffset, WHITE); 
      oledMonitor.drawPixel((int)trailX[1], (int)trailY[1] + shakeOffset, WHITE); 
      oledMonitor.drawPixel((int)trailX[2], (int)trailY[2] + shakeOffset, WHITE); 
    }
  }

  // 8. DIBUJAR PALETAS
  oledMonitor.fillRect(4, (int)paddle1Y + shakeOffset, PADDLE_W, currentPaddle1_H, WHITE);
  oledMonitor.fillRect(124 - PADDLE_W, (int)paddle2Y + shakeOffset, PADDLE_W, currentPaddle2_H, WHITE);

  // 9. BUCLE DE FÍSICA PARA CADA PELOTA
  int activeBallsCount = 0;
  for (int i = 0; i < 2; i++) {
    if (balls[i].active) activeBallsCount++;
  }

  for (int i = 0; i < 2; i++) {
    if (!balls[i].active) continue;

    // Movimiento y curva
    balls[i].dy += balls[i].spin;
    balls[i].dy = constrain(balls[i].dy, -4.0, 4.0);
    balls[i].x += balls[i].dx;
    balls[i].y += balls[i].dy;

    // Colisión con Portal
    if (activePortal.active && 
        balls[i].x + BALL_SIZE >= activePortal.x && balls[i].x <= activePortal.x + activePortal.w &&
        balls[i].y + BALL_SIZE >= activePortal.y && balls[i].y <= activePortal.y + activePortal.h) {
      
      balls[i].dx *= 1.4; 
      balls[i].y = random(4, SCREEN_HEIGHT - 12); 
      activePortal.active = false;
      nextPortalTimer = millis();
      shakeFrames = 6;
      playSFX(2100, 100);
    }

    // Rebote techo y piso
    if (balls[i].y <= 0) {
      balls[i].y = 1; balls[i].dy *= -1; balls[i].spin *= -0.5; playSFX(600, 15);
    } else if (balls[i].y >= SCREEN_HEIGHT - BALL_SIZE) {
      balls[i].y = SCREEN_HEIGHT - BALL_SIZE - 1; balls[i].dy *= -1; balls[i].spin *= -0.5; playSFX(600, 15);
    }

    // Colisión Paleta P1
    if (balls[i].x <= (4 + PADDLE_W) && balls[i].y + BALL_SIZE >= paddle1Y && balls[i].y <= paddle1Y + currentPaddle1_H) {
      float hitPoint = (balls[i].y + (BALL_SIZE / 2.0)) - (paddle1Y + (currentPaddle1_H / 2.0));
      lastPlayerToHit = 1;
      
      if (joy1Y < 1000) balls[i].spin = -0.12;
      else if (joy1Y > 3000) balls[i].spin = 0.12;
      else balls[i].spin = 0.0;

      consecutiveHits++;
      if (consecutiveHits % HITS_FOR_NEXT_LEVEL == 0 && currentSpeedLevel < 3) {
        currentSpeedLevel++; playSFX(1500, 100);
      }
      if (currentSpeedLevel == 3) shakeFrames = 5;

      float speed = (currentSpeedLevel == 1) ? BALL_SPEED_LVL1 : ((currentSpeedLevel == 2) ? BALL_SPEED_LVL2 : BALL_SPEED_LVL3);
      if (abs(hitPoint) > (currentPaddle1_H / 3.0)) { speed *= 1.25; shakeFrames = 4; playSFX(1800, 30); }

      balls[i].dx = speed;
      balls[i].dy = hitPoint * 0.25;
      if (balls[i].dy > -0.1 && balls[i].dy < 0.1) balls[i].dy = (random(0, 2) == 0) ? 0.5 : -0.5;
      balls[i].x = 4 + PADDLE_W;
      playSFX(900 + (consecutiveHits * 20), 20);
    }

    // Colisión Paleta P2
    if (balls[i].x + BALL_SIZE >= (124 - PADDLE_W) && balls[i].y + BALL_SIZE >= paddle2Y && balls[i].y <= paddle2Y + currentPaddle2_H) {
      float hitPoint = (balls[i].y + (BALL_SIZE / 2.0)) - (paddle2Y + (currentPaddle2_H / 2.0));
      lastPlayerToHit = 2;
      
      if (gameMode == 1) {
        if (joy2Y < 1000) balls[i].spin = -0.12;
        else if (joy2Y > 3000) balls[i].spin = 0.12;
        else balls[i].spin = 0.0;
      } else {
        balls[i].spin = (gameDifficulty == 2) ? (random(-10, 11) / 100.0) : 0.0;
      }

      consecutiveHits++;
      if (consecutiveHits % HITS_FOR_NEXT_LEVEL == 0 && currentSpeedLevel < 3) {
        currentSpeedLevel++; playSFX(1500, 100);
      }
      if (currentSpeedLevel == 3) shakeFrames = 5;

      float speed = (currentSpeedLevel == 1) ? BALL_SPEED_LVL1 : ((currentSpeedLevel == 2) ? BALL_SPEED_LVL2 : BALL_SPEED_LVL3);
      if (abs(hitPoint) > (currentPaddle2_H / 3.0)) { speed *= 1.25; shakeFrames = 4; playSFX(1800, 30); }

      balls[i].dx = -speed;
      balls[i].dy = hitPoint * 0.25;
      if (balls[i].dy > -0.1 && balls[i].dy < 0.1) balls[i].dy = (random(0, 2) == 0) ? 0.5 : -0.5;
      balls[i].x = 124 - PADDLE_W - BALL_SIZE;
      playSFX(900 + (consecutiveHits * 20), 20);
    }

    // Anotación de punto
    if (balls[i].x < 0 || balls[i].x > SCREEN_WIDTH) {
      bool isP1Point = (balls[i].x > SCREEN_WIDTH);
      balls[i].active = false; 

      if (activeBallsCount > 1) {
        if (isP1Point) scoreP1++; else scoreP2++;
        playSFX(1000, 100);
        activeBallsCount--;
        continue; 
      }

      // Es la última pelota en pantalla
      if (isP1Point) { scoreP1++; playSFX(1200, 200); }
      else { scoreP2++; playSFX(300, 200); }

      resetMainBall(isP1Point ? BALL_SPEED_LVL1 : -BALL_SPEED_LVL1);
      consecutiveHits = 0; currentSpeedLevel = 1;
      activePortal.active = false;
      nextPortalTimer = millis();

      powerUpActive = false; activePowerUpType = -1; lastPlayerToHit = 0;
      currentPaddle1_H = PADDLE_H; currentPaddle2_H = PADDLE_H;
      ghostBallActive = false; invertedControlsP1 = false; invertedControlsP2 = false;
      powerUpSpawnTimer = millis();

      stateTimer = millis();
      currentState = STATE_POINT_SCORED;
      return;
    }

    // Dibujar pelota
    if (!ghostBallActive || (ghostBallActive && (millis() / 200) % 2 == 0)) {
      oledMonitor.fillRect((int)balls[i].x, (int)balls[i].y + shakeOffset, BALL_SIZE, BALL_SIZE, WHITE);
    }
  }

  // 10. COMPROBAR VICTORIA
  if (scoreP1 >= scoreLimit || scoreP2 >= scoreLimit) {
    int winner = (scoreP1 >= scoreLimit) ? 1 : 2;
    if (gameMode == 0) { if (winner == 1) wins_1p_ia++; else wins_ia++; } 
    else { if (winner == 1) wins_p1_vs++; else wins_p2_vs++; }
    saveSettings(); 
    stopAudio();    
    
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

  if (millis() - stateTimer > 1500) {
    currentState = STATE_PLAY;
  }
}

// ==========================================
// ESTADO: MENÚ DE FIN DE PARTIDA
// ==========================================
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
    consecutiveHits = 0; currentSpeedLevel = 1;
    resetMainBall(1.0);

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

  if (pauseOption == 0) {
    oledMonitor.fillRect(15, 18, 98, 11, WHITE);
    oledMonitor.setTextColor(BLACK);
  } else { oledMonitor.setTextColor(WHITE); }
  oledMonitor.setCursor(35, 20);
  oledMonitor.print("CONTINUAR");

  if (pauseOption == 1) {
    oledMonitor.fillRect(15, 33, 98, 11, WHITE);
    oledMonitor.setTextColor(BLACK);
  } else { oledMonitor.setTextColor(WHITE); }
  oledMonitor.setCursor(25, 35);
  oledMonitor.print("CONFIGURACION");

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
      consecutiveHits = 0; currentSpeedLevel = 1;
      resetMainBall(1.0);

      animateScreenWipe();
      settingsReturnState = STATE_MENU; 
      currentState = STATE_MENU; 
    }
  }
}

// ==========================================
// FUNCIONES AUXILIARES MULTIPELOTA
// ==========================================
void resetMainBall(float dirX) {
  balls[0].x = 64;
  balls[0].y = 32;
  balls[0].spin = 0.0;
  balls[0].dx = (dirX >= 0 ? 1.0 : -1.0) * BALL_SPEED_LVL1;
  balls[0].dy = (random(0, 2) == 0 ? 1.5 : -1.5);
  balls[0].active = true;

  balls[1].active = false;
}

void spawnSecondaryBall() {
  if (!balls[1].active && balls[0].active) {
    balls[1].x = balls[0].x;
    balls[1].y = balls[0].y;
    balls[1].dx = -balls[0].dx; 
    balls[1].dy = -balls[0].dy * 0.9;
    balls[1].spin = 0.0;
    balls[1].active = true;
    playSFX(1500, 150);
  }
}