#include <SdFat.h>
#include <TMRpcm.h>
#include <OneButton.h>
#include <EEPROMex.h>

//#include <IniFile.h>

#define DEBUG          false

#define SD_SELECT_PIN    4
#define BUTTON_MAIN_PIN  5
#define BUTTON_AUX_PIN   6
#define SPEAKER_PIN      9
#define LED_PIN          13

#define BUTTON_CLICK_TICKS  300
#define BUTTON_PRESS_TICKS  600

#define MAX_SOUND_FONT_COUNT 4

#define SOUND_FONTS_BASE "/fonts"

OneButton buttonMain(BUTTON_MAIN_PIN, true);
OneButton buttonAux(BUTTON_AUX_PIN, true);

TMRpcm audio;
SdFat sd;

/* -------------------------------------------------------------------------------------------------- */

int addressInt;
int addressCharArray;

bool saberOn = false;
bool playingLockupSound = false;
long availableSoundFontCount = 0;
long currentSoundFontIndex = 0;
char currentSoundFontName[24];
char availableSoundFonts[MAX_SOUND_FONT_COUNT][24];

/* -------------------------------------------------------------------------------------------------- */

void StreamPrint_progmem(Print &out, PGM_P format, ...) {
  if (Serial.available()) { return; }
  // program memory version of printf - copy of format string and result share a buffer
  // so as to avoid too much memory use
  char formatString[128], *ptr;
  strncpy_P(formatString, format, sizeof(formatString)); // copy in from program mem
  // null terminate - leave last char since we might need it in worst case for result's \0
  formatString[sizeof(formatString) - 2] = '\0'; 
  ptr = &formatString[strlen(formatString) + 1]; // our result buffer...
  va_list args;
  va_start (args, format);
  vsnprintf(ptr, sizeof(formatString) - 1 - strlen(formatString), formatString, args);
  va_end (args);
  formatString[sizeof(formatString) - 1] = '\0'; 
  out.print(ptr);
}

#define Serialprint(format, ...) StreamPrint_progmem(Serial, PSTR(format), ##__VA_ARGS__)
#define Streamprint(stream,format, ...) StreamPrint_progmem(stream, PSTR(format), ##__VA_ARGS__)

/* -------------------------------------------------------------------------------------------------- */

void setLED(int value) {
  digitalWrite(LED_PIN, value);  
}

void fireLED(int d) {
  digitalWrite(LED_PIN, HIGH);
  delay(d);
  digitalWrite(LED_PIN, LOW);
}

/* -------------------------------------------------------------------------------------------------- */

void setupSerial() {
  Serial.begin(115200);
}

void setupLED() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

void setupMainSwitch() {
  buttonMain.setClickTicks(BUTTON_CLICK_TICKS);
  buttonMain.setPressTicks(BUTTON_PRESS_TICKS);
  buttonMain.attachClick(mainButtonPress);
  buttonMain.attachLongPressStart(mainButtonLongPress);
}

void setupAuxSwitch() {
  buttonAux.setClickTicks(BUTTON_CLICK_TICKS);
  buttonAux.setPressTicks(BUTTON_PRESS_TICKS);
  buttonAux.attachClick(auxButtonPress);
  buttonAux.attachLongPressStart(auxButtonLongPress);
}

void setupSwitches() {
  setupMainSwitch();
  setupAuxSwitch();
}

void setupSDCard() { 
  pinMode(SD_SELECT_PIN, OUTPUT);
  digitalWrite(SD_SELECT_PIN, HIGH);

  if (!sd.begin(SD_SELECT_PIN, SPI_FULL_SPEED)) {
    Serialprint("sd.begin() failed\n");
    return;    
  }
}

//void setupConfigFile() {
//  char buffer[bufferLen];
//
//  IniFile configFile("/config.ini");
//
//  if (! configFile.open(sd)) {
//    Serialprint("INI file does not exist\n");
//    return;
//  }
//
//  if (!configFile.validate(buffer, bufferLen)) {
//    Serialprint("INI file is invalid\n");
//    return;    
//  }
//}

void setupAudio(unsigned int volume) {
  audio.speakerPin = SPEAKER_PIN;
  audio.setVolume(volume);
}

/* -------------------------------------------------------------------------------------------------- */

void processButtonClicks() {
  buttonMain.tick();
  buttonAux.tick();
}

void mainButtonPress() {
  Serialprint("MAIN button press!\n");
  if (!saberOn) {    
    turnSaberOn();    
  }
}

void mainButtonLongPress() {
  Serialprint("MAIN button long press!\n");
  turnSaberOff();
}

void auxButtonPress() {
  Serialprint("AUX button press!\n");
  if (saberOn) {
    playLockupSound();
  } else {
    changeSoundFont();
  }
}

void auxButtonLongPress() {
  Serialprint("AUX button long press!\n");
  if (!saberOn) {
    enterConfigMode();
  }
}

void enterConfigMode() {
  Serialprint("Entering config mode!\n");
}

/* -------------------------------------------------------------------------------------------------- */

void turnSaberOn() {
  if (!saberOn) {
    Serialprint("Powering ON..\n");
    playSoundFontPowerOnSound();
    setLED(HIGH);
    saberOn = true;
  }
}

void turnSaberOff() {
  if (saberOn) {    
    Serialprint("Powering OFF..\n");
    playSoundFontPowerOffSound();
    setLED(LOW);
    saberOn = false;
  }
}

void changeSoundFont() {
  long newIndex = 0;

  if (!saberOn) {
    if (currentSoundFontIndex < (availableSoundFontCount - 1)) {
      newIndex = currentSoundFontIndex + 1;
    }
    
    selectSoundFont(newIndex);
    playSoundFontNameSound();
  }
}

void selectSoundFont(long index) {
  currentSoundFontIndex = index;
  sprintf(currentSoundFontName, availableSoundFonts[currentSoundFontIndex]);

  Serialprint("Selecting font '%s'\n", currentSoundFontName);
  
  EEPROM.writeInt(addressInt, index);
  EEPROM.writeBlock<char>(addressCharArray, currentSoundFontName, 24); 
}

void getCurrentSoundFont(char *fontName) {
  EEPROM.readBlock<char>(addressCharArray, fontName, 24);
  sprintf(currentSoundFontName, fontName);
}

long getCurrentSoundFontIndex() {
  long index;
  
  index = EEPROM.readInt(addressInt);
  currentSoundFontIndex = index;
  
  return index;
}

void setFirstSoundFontIfNotSet() {
  long soundFontIndex;
  char soundFontName[24];
  char *soundFonts[MAX_SOUND_FONT_COUNT];

  soundFontIndex = getCurrentSoundFontIndex();
  getCurrentSoundFont(soundFontName);

  if (strcmp(availableSoundFonts[soundFontIndex], soundFontName) != 0) {
    selectSoundFont(0);
  }
}

/* -------------------------------------------------------------------------------------------------- */

void waitForSoundToFinish() {
  while (audio.isPlaying()) { };
}

void playSound(char *filename, boolean force=false, boolean loopIt=false) {
  if (force) {
    audio.stopPlayback();
  } else {
    waitForSoundToFinish();
  }

  Serialprint("Playing '%s' (force: %d, loop: %d)\n", filename, force, loopIt);
 
  audio.play(filename);
  audio.loop(loopIt);
}

void playFontSound(char *filename, boolean force=false, boolean loopIt=false) {
  char fullFilename[60];
  
  currentSoundFontDirectory(filename, fullFilename);
  playSound(fullFilename, force, loopIt);
}

void loopSound(char *filename, boolean force=false) {
  playSound(filename, force, true);
}

void loopFontSound(char *filename, boolean force=false) {
  playFontSound(filename, force, true);
}

void currentSoundFontDirectory(char *filename, char *fullFilename) {  
  sprintf(fullFilename, "/fonts/%s/%s", currentSoundFontName, filename);
}

void playOKSound() {
  playSound("/internal/okay.wav");
}

void playBootSound() {
  playSound("/internal/boot.wav");
}

void playSoundFontIdleSound(boolean force=false) {
  loopFontSound("idle.wav", force);
}

void playLockupSound() {  
  if (!playingLockupSound) {
    playingLockupSound = true;
    loopSound("/internal/lockup.wav", true);
  } else {
    playingLockupSound = false;
    playSoundFontIdleSound(true);
  }
}

void playSoundFontPowerOnSound() {
  playFontSoundWithHum("poweron.wav");
}

void playSoundFontPowerOffSound() {
  audio.stopPlayback();
  playFontSound("poweroff.wav", true);
}

void playSoundFontSwingSound() {
  playFontSoundWithHum("swing.wav");
}

void playSoundFontClashSound() {
  playFontSoundWithHum("clash.wav");
}

void playSoundFontNameSound() {
  playFontSound("name.wav");
}

void playFontSoundWithHum(char *filename) {
  playFontSound(filename, true);
  playSoundFontIdleSound();
}

/* -------------------------------------------------------------------------------------------------- */

unsigned int soundVolume() {
  unsigned int currentVolume = 4;
  
  return currentVolume;
}

bool readConfig(char *section, char *key) { 
  char buf[20];

  return false;
  
//  if (configFile.getValue(section, key, buf, 20)) {
//    Serial.println(buf);
//    return true;
//  } else {
//    Serialprint("%s\n", "Could not read value");
//    return false;
//  }
}

void discoverSoundFonts() {
  FatFile root;
  FatFile file;
  unsigned int counter = 0;
  char buffer[24];

  waitForSoundToFinish();

  root.open(SOUND_FONTS_BASE);

  while (file.openNext(&root, O_READ)) {
    file.getName(buffer, 24);
    Serialprint("Found sound font %d: '%s'\n", counter, buffer);
    strncpy(availableSoundFonts[counter++], buffer, strlen(buffer));
    file.close();    
  }

  root.close();

  availableSoundFontCount = counter;
}

/* -------------------------------------------------------------------------------------------------- */

void setup() {
  setupSerial();
  
  addressInt = EEPROM.getAddress(sizeof(int));
  addressCharArray = EEPROM.getAddress(sizeof(char) * 24); 

  setupLED();
  setupSwitches();
  setupSDCard();
//  setupConfigFile();
  setupAudio(soundVolume());

  discoverSoundFonts();
  setFirstSoundFontIfNotSet();
  
//  playBootSound();
  playSoundFontNameSound();
}

void loop() {
  processButtonClicks();
  
  if (saberOn) {
    // process movements
  }
}
