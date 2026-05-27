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

const int btnNext = 4;
const int btnPause = 6;
const int btnPrev = 7; 

int currentTrack = 1;
const int totalTracks = 2; 
bool isPlaying = true;

// Animation Variables
unsigned long lastFrameTime = 0;
const int frameDelay = 60; // Controls rotation velocity (lower is faster)
float angle = 0.0;         // Tracks the current rotation vector

// Layout Center coordinates for the vinyl record disk
const int centerX = 94;
const int centerY = 36;
const int radius = 22;

void drawVinyl(float currentAngle, bool spinning) {
  // 1. Draw the static record player base frame
  display.drawRect(64, 10, 60, 52, SSD1306_WHITE); // Turntable deck border
  display.drawCircle(69, 15, 1, SSD1306_WHITE);   // Top-left screw accent
  
  // 2. Draw the main vinyl body
  display.drawCircle(centerX, centerY, radius, SSD1306_WHITE);     // Outer rim
  display.drawCircle(centerX, centerY, radius - 6, SSD1306_WHITE); // Groove line
  display.drawCircle(centerX, centerY, 4, SSD1306_WHITE);          // Center paper label
  display.drawPixel(centerX, centerY, SSD1306_WHITE);              // Center spindle pin

  // 3. Draw shifting vinyl reflection shine vectors if music is playing
  if (spinning) {
    // Calculate reflection vector offsets using trigonometry coordinates
    int xOffset1 = radius * cos(currentAngle);
    int yOffset1 = radius * sin(currentAngle);
    int xOffset2 = radius * cos(currentAngle + 3.1415); // Opposite side shine
    int yOffset2 = radius * sin(currentAngle + 3.1415);

    display.drawLine(centerX, centerY, centerX + xOffset1, centerY + yOffset1, SSD1306_WHITE);
    display.drawLine(centerX, centerY, centerX + xOffset2, centerY + yOffset2, SSD1306_WHITE);
  } else {
    // Static reflection lines when audio stream is paused
    display.drawLine(centerX, centerY, centerX + 15, centerY + 15, SSD1306_WHITE);
    display.drawLine(centerX, centerY, centerX - 15, centerY - 15, SSD1306_WHITE);
  }

  // 4. Draw the static tonearm needle tracking across the grooves
  display.drawLine(118, 14, 114, 28, SSD1306_WHITE); // Arm pivot link
  display.drawLine(114, 28, 100, 32, SSD1306_WHITE); // Needle arm pole
  display.fillRect(98, 31, 3, 3, SSD1306_WHITE);     // Needle cartridge box
}

void updateScreen(String statusText, int trackNum, float currentAngle, bool spinning) {
  display.clearDisplay();
  
  // Text UI Panel Layout (Left Side: 0 to 63 pixels)
  display.setTextColor(SSD1306_WHITE);
  
  // 1. App Title Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("JUKEBOX"));
  display.drawLine(0, 9, 58, 9, SSD1306_WHITE); // Clean divider underline
  
  // 2. Playback State Status (PLAYING or PAUSED)
  display.setTextSize(1); // Reduced size to prevent side truncation
  display.setCursor(0, 20);
  display.println(statusText);
  
  // 3. Main Track ID Label
  display.setTextSize(2); // Large distinct font
  display.setCursor(0, 44); // Shifted down to make space for status text
  display.print(F("#0"));
  display.println(trackNum);

  // Turntable Canvas Render Panel (Right Side: 64 to 127 pixels)
  drawVinyl(currentAngle, spinning);
  
  display.display();
}

void setup() {
  mp3Serial.begin(9600);
  
  pinMode(btnNext, INPUT_PULLUP);
  pinMode(btnPause, INPUT_PULLUP);
  pinMode(btnPrev, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { while(true); }
  if (!myDFPlayer.begin(mp3Serial)) { while(true); }
  
  myDFPlayer.volume(22);
  delay(500);
  
  myDFPlayer.play(currentTrack);
  updateScreen("PLAYING", currentTrack, angle, isPlaying);
}

void loop() {
  // --- CORE SYSTEM CLOCK ANIMATION TICK ---
  // Redraws the vinyl rotation state independently without block delays
  if (isPlaying && (millis() - lastFrameTime >= frameDelay)) {
    lastFrameTime = millis();
    angle += 0.25; // Speed multiplier step increment
    if (angle >= 6.283) angle = 0.0; // Reset angle once completing a full 360 loop ($2\pi$)
    
    updateScreen("PLAYING", currentTrack, angle, true);
  }

  // --- BUTTON 1: SKIP FORWARD ---
  if (digitalRead(btnNext) == LOW) {
    delay(50); 
    currentTrack++;
    if (currentTrack > totalTracks) currentTrack = 1;
    
    isPlaying = true; 
    myDFPlayer.play(currentTrack);
    updateScreen("PLAYING", currentTrack, angle, isPlaying);
    
    while(digitalRead(btnNext) == LOW); 
    delay(50);
  }

  // --- BUTTON 2: PLAY / PAUSE ---
  if (digitalRead(btnPause) == LOW) {
    delay(50); 
    if (isPlaying) {
      myDFPlayer.pause();
      isPlaying = false;
      updateScreen("PAUSED", currentTrack, angle, isPlaying);
    } else {
      myDFPlayer.start(); 
      isPlaying = true;
      updateScreen("PLAYING", currentTrack, angle, isPlaying);
    }
    while(digitalRead(btnPause) == LOW); 
    delay(50);
  }

  // --- BUTTON 3: SKIP BACKWARD ---
  if (digitalRead(btnPrev) == LOW) {
    delay(50); 
    currentTrack--;
    if (currentTrack < 1) currentTrack = totalTracks; 
    
    isPlaying = true;
    myDFPlayer.play(currentTrack);
    updateScreen("PLAYING", currentTrack, angle, isPlaying);
    
    while(digitalRead(btnPrev) == LOW); 
    delay(50);
  }

  // --- AUTOMATIC PLAYLIST CYCLE ---
  if (isPlaying && myDFPlayer.readState() == 0) {
    delay(200);
    currentTrack++;
    if (currentTrack > totalTracks) currentTrack = 1;
    
    myDFPlayer.play(currentTrack);
    updateScreen("PLAYING", currentTrack, angle, isPlaying);
    delay(2000); 
  }
}