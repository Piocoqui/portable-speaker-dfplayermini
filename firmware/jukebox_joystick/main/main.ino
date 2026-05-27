#include <Arduino.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

SoftwareSerial mp3Serial(2, 3); 
DFRobotDFPlayerMini myDFPlayer;

// --- JOYSTICK PINS ---
const int joyX = A1;  // Left/Right (Track Control)
const int joyY = A0;  // Up/Down (Volume Control)
const int joySW = 4;  // Center Click (Play/Pause)

int currentTrack = 1;
int totalTracks = 0; 
bool isPlaying = true;

// Volume Tracking Variable
int currentVolume = 22; // Default for rear-firing setup

// Animation Variables
unsigned long lastFrameTime = 0;
const int frameDelay = 60; 
float angle = 0.0;         

// Layout Center coordinates for the vinyl record disk
const int centerX = 94;
const int centerY = 36;
const int radius = 22;

void drawVinyl(float currentAngle, bool spinning) {
  // 1. Draw the static record player base frame
  display.drawRect(64, 10, 60, 52, SSD1306_WHITE); 
  display.drawCircle(69, 15, 1, SSD1306_WHITE);   
  
  // 2. Draw the main vinyl body
  display.drawCircle(centerX, centerY, radius, SSD1306_WHITE);     
  display.drawCircle(centerX, centerY, radius - 6, SSD1306_WHITE); 
  display.drawCircle(centerX, centerY, 4, SSD1306_WHITE);          
  display.drawPixel(centerX, centerY, SSD1306_WHITE);              

  // 3. Draw shifting vinyl reflection shine vectors if music is playing
  if (spinning) {
    int xOffset1 = radius * cos(currentAngle);
    int yOffset1 = radius * sin(currentAngle);
    int xOffset2 = radius * cos(currentAngle + 3.1415); 
    int yOffset2 = radius * sin(currentAngle + 3.1415);

    display.drawLine(centerX, centerY, centerX + xOffset1, centerY + yOffset1, SSD1306_WHITE);
    display.drawLine(centerX, centerY, centerX + xOffset2, centerY + yOffset2, SSD1306_WHITE);
  } else {
    display.drawLine(centerX, centerY, centerX + 15, centerY + 15, SSD1306_WHITE);
    display.drawLine(centerX, centerY, centerX - 15, centerY - 15, SSD1306_WHITE);
  }

  // 4. Draw the static tonearm needle tracking across the grooves
  display.drawLine(118, 14, 114, 28, SSD1306_WHITE); 
  display.drawLine(114, 28, 100, 32, SSD1306_WHITE); 
  display.fillRect(98, 31, 3, 3, SSD1306_WHITE);     
}

void updateScreen(String statusText, int trackNum, float currentAngle, bool spinning) {
  display.clearDisplay();
  
  // Text UI Panel Layout
  display.setTextColor(SSD1306_WHITE);
  
  // 1. App Title Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("JUKEBOX"));
  display.drawLine(0, 9, 58, 9, SSD1306_WHITE); 
  
  // 2. Playback State Status
  display.setTextSize(1); 
  display.setCursor(0, 20);
  display.println(statusText);
  
  // 3. Main Track ID Label
  display.setTextSize(2); 
  display.setCursor(0, 36); // Moved up slightly to make room for volume stat
  display.print(F("#0"));
  display.println(trackNum);

  // 4. New: Real-time Volume UI Text Block
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(F("VOL: "));
  display.print(currentVolume);

  // Turntable Canvas Render Panel
  drawVinyl(currentAngle, spinning);
  
  display.display();
}

void setup() {
  mp3Serial.begin(9600);
  
  // Joystick Switch Pin setup
  pinMode(joySW, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { while(true); }
  if (!myDFPlayer.begin(mp3Serial)) { while(true); }
  
  delay(500); // Give the SD card an extra moment to mount
  totalTracks = myDFPlayer.readFileCounts(); 
  
  // Safety fallback: if reading fails, default to a safe number
  if (totalTracks <= 0) {
    totalTracks = 3; 
  }

  myDFPlayer.volume(currentVolume);
  delay(500);
  
  myDFPlayer.play(currentTrack);
  updateScreen("PLAYING", currentTrack, angle, isPlaying);
}

void loop() {
  // --- CORE SYSTEM CLOCK ANIMATION TICK ---
  if (isPlaying && (millis() - lastFrameTime >= frameDelay)) {
    lastFrameTime = millis();
    angle += 0.25; 
    if (angle >= 6.283) angle = 0.0; 
    
    updateScreen("PLAYING", currentTrack, angle, true);
  }

  // Read the Joystick's current physical position
  int xVal = analogRead(joyX);
  int yVal = analogRead(joyY);

  // --- JOYSTICK RIGHT: SKIP FORWARD ---
  if (xVal > 800) {
    currentTrack++;
    if (currentTrack > totalTracks) currentTrack = 1;
    
    isPlaying = true; 
    myDFPlayer.play(currentTrack);
    updateScreen("PLAYING", currentTrack, angle, isPlaying);
    
    delay(400); // Cooldown to prevent rapid-fire skipping
  }

  // --- JOYSTICK LEFT: SKIP BACKWARD ---
  if (xVal < 200) {
    currentTrack--;
    if (currentTrack < 1) currentTrack = totalTracks; 
    
    isPlaying = true;
    myDFPlayer.play(currentTrack);
    updateScreen("PLAYING", currentTrack, angle, isPlaying);
    
    delay(400); // Cooldown to prevent rapid-fire skipping
  }

  // --- JOYSTICK UP: VOLUME UP ---
  if (yVal > 800) {
    if (currentVolume < 30) {
      currentVolume++;
      myDFPlayer.volume(currentVolume);
      // Immediately refresh screen to show updated volume numbers
      updateScreen(isPlaying ? "PLAYING" : "PAUSED", currentTrack, angle, isPlaying);
    }
    delay(150); // Fast scaling step
  }

  // --- JOYSTICK DOWN: VOLUME DOWN ---
  if (yVal < 200) {
    if (currentVolume > 0) {
      currentVolume--;
      myDFPlayer.volume(currentVolume);
      // Immediately refresh screen to show updated volume numbers
      updateScreen(isPlaying ? "PLAYING" : "PAUSED", currentTrack, angle, isPlaying);
    }
    delay(150); // Fast scaling step
  }

  // --- JOYSTICK CLICK: PLAY / PAUSE ---
  if (digitalRead(joySW) == LOW) {
    delay(50); // Mechanical debounce
    if (isPlaying) {
      myDFPlayer.pause();
      isPlaying = false;
      updateScreen("PAUSED", currentTrack, angle, isPlaying);
    } else {
      myDFPlayer.start(); 
      isPlaying = true;
      updateScreen("PLAYING", currentTrack, angle, isPlaying);
    }
    
    while(digitalRead(joySW) == LOW); 
    delay(50);
  }

  // --- AUTOMATIC PLAYLIST CYCLE (The Safe Way) ---
  if (myDFPlayer.available()) {
    if (myDFPlayer.readType() == DFPlayerPlayFinished) {
      delay(200); 
      currentTrack++;
      if (currentTrack > totalTracks) currentTrack = 1;
      
      myDFPlayer.play(currentTrack);
      updateScreen("PLAYING", currentTrack, angle, isPlaying);

      delay(800);
    }
  }
}