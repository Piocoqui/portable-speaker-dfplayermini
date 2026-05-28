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
const int joyX = A1;
const int joyY = A0;
const int joySW = 4;

// --- SYSTEM STATES ---
enum UI_MODE { VISUALIZER, MENU };
UI_MODE currentMode = VISUALIZER;

// --- TRACK & DIRECTORY DATA ---
int currentTrack = 1;
int totalTracks = 0;
bool isPlaying = true;
int menuCursor = 1;

// Song name table. Index 0 is unused; add entries here when you add files.
const char* songTitles[] = {
  "NULL",             // 0 — never used
  "THE WAY I AM.MP3",
  "HOT AND COLD.MP3",
  "BAD.MP3"
};
const int knownNames = sizeof(songTitles) / sizeof(songTitles[0]) - 1;

// --- VOLUME ---
int currentVolume = 22;
unsigned long volumeDisplayTimer = 0;

// --- ANIMATION ---
unsigned long lastFrameTime = 0;
const int frameDelay = 60;
float angle = 0.0;
const int centerX = 94;
const int centerY = 36;
const int radius = 22;

// --- NON-BLOCKING JOYSTICK DEBOUNCE ---
// Replaces all the delay() calls that were freezing the CPU and causing
// DFPlayer serial events to pile up in the buffer.
unsigned long lastJoyTime = 0;
const int joyDelay = 300; // ms between joystick-triggered actions

// --- AUTO-ADVANCE GUARD ---
// After ANY track change (manual or automatic) we ignore DFPlayerPlayFinished
// events for this many ms, preventing the stale buffer event from double-firing.
unsigned long trackChangeBlockTimer = 0;
const unsigned long TRACK_BLOCK_MS = 2500;


// ==========================================
// HELPER: getSongName
// ==========================================
String getSongName(int index) {
  if (index >= 1 && index <= knownNames) {
    return String(songTitles[index]);
  }
  return "TRACK_" + String(index) + ".MP3";
}


// ==========================================
// HELPER: playTrack
//
// Every track change must go through here.
// This is the fix for the double-skip bug:
//   1. Resets trackChangeBlockTimer so the auto-cycle guard is always
//      active after BOTH manual skips AND song-end auto-advance.
//   2. Sets isPlaying = true so the status display is always correct.
//   3. Flushes the DFPlayer serial buffer so stale DFPlayerPlayFinished
//      messages from the previous song don't trigger a second skip.
// ==========================================
void playTrack(int track) {
  // Clamp to valid range
  if (track < 1) track = totalTracks;
  if (track > totalTracks) track = 1;

  currentTrack = track;
  myDFPlayer.play(currentTrack);
  isPlaying = true;

  // Start the ignore window BEFORE the buffer can fill
  trackChangeBlockTimer = millis() + TRACK_BLOCK_MS;

  // Give DFPlayer ~100 ms to process the command, then throw away
  // any stale serial bytes (e.g. a leftover DFPlayerPlayFinished from
  // the song that just ended).
  delay(100);
  while (mp3Serial.available()) mp3Serial.read();
}


// ==========================================
// DRAWING FUNCTIONS
// ==========================================

void drawMenu() {
  display.clearDisplay();

  display.setTextSize(0.75);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("DIR /SD_CARD/AUDIO"));
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  // Scrolling window: show up to 5 tracks, centred on the cursor
  int startDisplay = menuCursor - 2;
  if (startDisplay < 1) startDisplay = 1;

  for (int i = 0; i < 5; i++) {
    int trackIdx = startDisplay + i;
    if (trackIdx > totalTracks) break;

    int yPos = 14 + (i * 10);

    if (trackIdx == menuCursor) {
      display.fillRect(0, yPos - 1, 128, 9, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(2, yPos);
    if (trackIdx < 10) display.print(F("0"));
    display.print(trackIdx);
    display.print(F(" "));
    display.print(getSongName(trackIdx));
  }
  display.display();
}

void drawVinyl(float currentAngle, bool spinning) {
  display.drawRect(64, 10, 60, 52, SSD1306_WHITE);
  display.drawCircle(69, 15, 1, SSD1306_WHITE);
  display.drawCircle(centerX, centerY, radius, SSD1306_WHITE);
  display.drawCircle(centerX, centerY, radius - 6, SSD1306_WHITE);
  display.drawCircle(centerX, centerY, 4, SSD1306_WHITE);
  display.drawPixel(centerX, centerY, SSD1306_WHITE);

  if (spinning) {
    int xOffset1 = radius * cos(currentAngle);
    int yOffset1 = radius * sin(currentAngle);
    int xOffset2 = radius * cos(currentAngle + 3.1415);
    int yOffset2 = radius * sin(currentAngle + 3.1415);
    display.drawLine(centerX, centerY, centerX + xOffset1, centerY + yOffset1, SSD1306_WHITE);
    display.drawLine(centerX, centerY, centerX + xOffset2, centerY + yOffset2, SSD1306_WHITE);
  } else {
    // Fixed cross when paused — more visually distinct than a diagonal
    display.drawLine(centerX, centerY - 15, centerX, centerY + 15, SSD1306_WHITE);
    display.drawLine(centerX - 15, centerY, centerX + 15, centerY, SSD1306_WHITE);
  }

  // Tonearm
  display.drawLine(118, 14, 114, 28, SSD1306_WHITE);
  display.drawLine(114, 28, 100, 32, SSD1306_WHITE);
  display.fillRect(98, 31, 3, 3, SSD1306_WHITE);
}

void updateScreen(String statusText, int trackNum, float currentAngle, bool spinning) {
  if (currentMode != VISUALIZER) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Left panel
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("JUKEBOX"));
  display.drawLine(0, 9, 58, 9, SSD1306_WHITE);

  display.setCursor(0, 20);
  display.println(statusText);

  display.setTextSize(2);
  display.setCursor(0, 36);
  display.print(F("#"));
  if (trackNum < 10) display.print(F("0"));
  display.println(trackNum);

  // Turntable panel — only spin when actually playing
  drawVinyl(currentAngle, spinning && isPlaying);

  // Volume overlay (shown for 2 seconds after a volume change)
  if (millis() < volumeDisplayTimer) {
    display.fillRect(0, 50, 128, 14, SSD1306_BLACK);
    display.drawRect(0, 50, 128, 14, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(4, 53);
    display.print(F("VOL"));
    int barWidth = map(currentVolume, 0, 30, 0, 94);
    display.fillRect(28, 53, barWidth, 8, SSD1306_WHITE);
  }

  display.display();
}


// ==========================================
// SETUP
// ==========================================

void setup() {
  mp3Serial.begin(9600);
  pinMode(joySW, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { while (true); }
  if (!myDFPlayer.begin(mp3Serial))               { while (true); }

  delay(500);
  totalTracks = myDFPlayer.readFileCounts();
  if (totalTracks <= 0) totalTracks = knownNames; // safe fallback

  myDFPlayer.volume(currentVolume);
  delay(500);

  playTrack(currentTrack);
  updateScreen(F("PLAYING"), currentTrack, angle, true);
}


// ==========================================
// LOOP
// ==========================================

void loop() {
  int xVal = analogRead(joyX);
  int yVal = analogRead(joyY);
  unsigned long now = millis();

  // ----------------------------------------
  // BUTTON: short click vs long press
  // ----------------------------------------
  if (digitalRead(joySW) == LOW) {
    unsigned long pressStart = millis();
    while (digitalRead(joySW) == LOW); // wait for release
    unsigned long held = millis() - pressStart;

    if (held > 600) {
      // Long press: toggle between Visualizer and Menu
      if (currentMode == VISUALIZER) {
        currentMode = MENU;
        menuCursor = currentTrack;
        drawMenu();
      } else {
        currentMode = VISUALIZER;
        updateScreen(isPlaying ? F("PLAYING") : F("PAUSED"), currentTrack, angle, isPlaying);
      }
    } else {
      // Short press
      if (currentMode == VISUALIZER) {
        // Play / Pause
        if (isPlaying) {
          myDFPlayer.pause();
          isPlaying = false;
        } else {
          myDFPlayer.start();
          isPlaying = true;
        }
        updateScreen(isPlaying ? F("PLAYING") : F("PAUSED"), currentTrack, angle, isPlaying);
      } else if (currentMode == MENU) {
        // Select highlighted track
        playTrack(menuCursor);
        currentMode = VISUALIZER;
        updateScreen(F("PLAYING"), currentTrack, angle, true);
      }
    }
    delay(50); // button debounce only — safe here, very short
  }

  // ----------------------------------------
  // VISUALIZER MODE
  // ----------------------------------------
  if (currentMode == VISUALIZER) {

    // Animate vinyl — only ticks when playing
    if (isPlaying && (now - lastFrameTime >= frameDelay)) {
      lastFrameTime = now;
      angle += 0.25;
      if (angle >= 6.283) angle = 0.0;
      updateScreen(F("PLAYING"), currentTrack, angle, true);
    }

    // Non-blocking joystick gate: ignore repeated inputs within joyDelay ms
    if (now - lastJoyTime >= joyDelay) {

      if (xVal > 800) { // Skip forward
        int next = (currentTrack >= totalTracks) ? 1 : currentTrack + 1;
        playTrack(next);
        updateScreen(F("PLAYING"), currentTrack, angle, true);
        lastJoyTime = now;
      }

      if (xVal < 200) { // Skip backward
        int prev = (currentTrack <= 1) ? totalTracks : currentTrack - 1;
        playTrack(prev);
        updateScreen(F("PLAYING"), currentTrack, angle, true);
        lastJoyTime = now;
      }

      if (yVal > 800) { // Volume up
        if (currentVolume < 30) {
          currentVolume++;
          myDFPlayer.volume(currentVolume);
        }
        volumeDisplayTimer = now + 2000;
        updateScreen(isPlaying ? F("PLAYING") : F("PAUSED"), currentTrack, angle, isPlaying);
        lastJoyTime = now;
      }

      if (yVal < 200) { // Volume down
        if (currentVolume > 0) {
          currentVolume--;
          myDFPlayer.volume(currentVolume);
        }
        volumeDisplayTimer = now + 2000;
        updateScreen(isPlaying ? F("PLAYING") : F("PAUSED"), currentTrack, angle, isPlaying);
        lastJoyTime = now;
      }
    }
  }

  // ----------------------------------------
  // MENU MODE
  // ----------------------------------------
  if (currentMode == MENU) {
    if (now - lastJoyTime >= joyDelay) {

      if (yVal > 800 && menuCursor < totalTracks) { // Scroll down
        menuCursor++;
        drawMenu();
        lastJoyTime = now;
      }

      if (yVal < 200 && menuCursor > 1) { // Scroll up
        menuCursor--;
        drawMenu();
        lastJoyTime = now;
      }

      if (xVal < 200) { // Flick left: exit menu without changing track
        currentMode = VISUALIZER;
        updateScreen(isPlaying ? F("PLAYING") : F("PAUSED"), currentTrack, angle, isPlaying);
        lastJoyTime = now;
      }

      if (xVal > 800) { // Flick right: select (same as short press)
        playTrack(menuCursor);
        currentMode = VISUALIZER;
        updateScreen(F("PLAYING"), currentTrack, angle, true);
        lastJoyTime = now;
      }
    }
  }

  // ----------------------------------------
  // AUTO-ADVANCE: play next track when song ends
  // ----------------------------------------
  if (myDFPlayer.available()) {
    uint8_t type = myDFPlayer.readType();

    // The block timer ensures this only fires once per song end,
    // and is always reset by playTrack() — so manual skips can't
    // leave a stale event that triggers a phantom second skip.
    if (type == DFPlayerPlayFinished && now > trackChangeBlockTimer) {
      int next = (currentTrack >= totalTracks) ? 1 : currentTrack + 1;
      playTrack(next);
      if (currentMode == VISUALIZER) {
        updateScreen(F("PLAYING"), currentTrack, angle, true);
      }
    }
  }
}
