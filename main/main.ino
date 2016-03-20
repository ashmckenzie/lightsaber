#include <avr/sleep.h>

#include <SdFat.h>
#include <TMRpcm.h>
#include <OneButton.h>
#include <EEPROMex.h>
#include <Timer.h>
#include <LowPower.h>

//#include <MPU6050_6Axis_MotionApps20.h>
//#include <IniFile.h>

//#define DEBUG
#define DEBUG_WIP

#define ENABLE_SLEEP

#define SD_SELECT_PIN       4
#define BUTTON_MAIN_PIN     2
#define BUTTON_MAIN_LED_PIN 7
#define BUTTON_AUX_PIN      6
#define BUTTON_AUX_LED_PIN  8
#define SPEAKER_PIN         9
#define LED_PIN            13

#define BUTTON_CLICK_TICKS  200
#define BUTTON_PRESS_TICKS  700

#define MAX_SECONDS_BEFORE_SLEEP      60   // 1 minute
#define MAX_SECONDS_BEFORE_DEEP_SLEEP 300  // 5 minutes

#define MAX_SOUND_FONT_COUNT          10
#define MAX_SOUND_FONT_FOLDER_LENGTH  24

#define OKAY_SOUND_FILE_NAME          "okay"
#define BOOT_SOUND_FILE_NAME          "boot"
#define LOCKUP_SOUND_FILE_NAME        "lockup"
#define POWERING_DOWN_SOUND_FILE_NAME "powering_down"

#define NAME_SOUND_FILE_NAME      "name"
#define IDLE_SOUND_FILE_NAME      "idle"
#define POWER_ON_SOUND_FILE_NAME  "poweron"
#define POWER_OFF_SOUND_FILE_NAME "poweroff"
#define SWING_SOUND_FILE_FORMAT   "swing%d"
#define CLASH_SOUND_FILE_FORMAT   "clash%d"

OneButton buttonMain(BUTTON_MAIN_PIN, true);
OneButton buttonAux(BUTTON_AUX_PIN, true);

TMRpcm audio;
SdFat sd;
Timer t;

/* -------------------------------------------------------------------------------------------------- */

//MPU6050 mpu;
//
//uint8_t mpuIntStatus;
//uint8_t devStatus;
//uint16_t packetSize;
//uint16_t fifoCount;
//uint8_t fifoBuffer[64];
//
//Quaternion q;
//VectorInt16 aaWorld;
//
//static Quaternion ql;
//static Quaternion qr;
//static VectorInt16 aaWorld_last;
//static VectorInt16 aaWorld_reading;
//
//unsigned long magnitude = 0;
//
//volatile bool mpuInterrupt = false;
//
//void dmpDataReady() {
//  mpuInterrupt = true;
//}

/* -------------------------------------------------------------------------------------------------- */

int addressCharArray = EEPROM.getAddress(sizeof(char) * MAX_SOUND_FONT_FOLDER_LENGTH);

byte buttonMainLEDEvent = 0;
byte buttonAuxLEDEvent = 0;
byte checkIfShouldSleepEvent = 0;
byte secondsIdle = 0;
byte currentSwingSound = 1;
byte currentClashSound = 1;

boolean playSounds = true;

boolean saberOn = false;
boolean soundAvailable = false;
boolean playingLockupSound = false;
boolean sleeping = false;
boolean justBackFromSleep = false;

/* -------------------------------------------------------------------------------------------------- */

//void setLED(int value) {
//  digitalWrite(LED_PIN, value);
//}
//
//void fireLED(int delayLength) {
//  digitalWrite(LED_PIN, HIGH);
//  delay(delayLength);
//  digitalWrite(LED_PIN, LOW);
//}

/* -------------------------------------------------------------------------------------------------- */

void setupSerial() {
  Serial.begin(115200);
}

void setupLED() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

//void setupAccelerometer() {
//  Wire.begin();
//  mpu.initialize();
//
//  if (!mpu.testConnection()) {
//#ifdef DEBUG
//    Serial.println(F("setupAccelerometer(): MPU6050 failed"));
//#endif
//    return;
//  }
//
//  devStatus = mpu.dmpInitialize();
//
//  mpu.setXAccelOffset(-762);
//  mpu.setYAccelOffset(583);
//  mpu.setZAccelOffset(1254);
//
//  mpu.setXGyroOffset(84);
//  mpu.setYGyroOffset(-47);
//  mpu.setZGyroOffset(87);
//
//  mpu.setDMPEnabled(true);
//
////  attachInterrupt(0, dmpDataReady, RISING);
//  mpu.getIntStatus();
//  packetSize = mpu.dmpGetFIFOPacketSize();
//
//#ifdef DEBUG
//  Serial.println(F("setupAccelerometer(): MPU6050 ready!"));
//#endif
//}

void setupSwitches() {
  pinMode(BUTTON_MAIN_LED_PIN, OUTPUT);
  turnOffButtonMainLED();
  
  buttonMain.setClickTicks(BUTTON_CLICK_TICKS);
  buttonMain.setPressTicks(BUTTON_PRESS_TICKS);
  buttonMain.attachClick(mainButtonPress);
  buttonMain.attachLongPressStart(mainButtonLongPress);

  pinMode(BUTTON_AUX_LED_PIN, OUTPUT);
  turnOffButtonAuxLED();

  buttonAux.setClickTicks(BUTTON_CLICK_TICKS);
  buttonAux.setPressTicks(BUTTON_PRESS_TICKS);
  buttonAux.attachClick(auxButtonPress);
  buttonAux.attachLongPressStart(auxButtonLongPress);
}

void setupSDCard() {
  pinMode(SD_SELECT_PIN, OUTPUT);
  digitalWrite(SD_SELECT_PIN, HIGH);

  if (!sd.begin(SD_SELECT_PIN, SPI_FULL_SPEED)) {
#ifdef DEBUG    
    Serial.println(F("setupSDCard(): sd.begin() failed"));
#endif
    return;
  }
}

//void setupConfigFile() {
//  char buffer[bufferLen];
//
//  IniFile configFile("/config.ini");
//
//  if (! configFile.open(sd)) {
//#ifdef DEBUG
//    Serial.println(F("setupConfigFile(): INI file does not exist"));
//#endif
//    return;
//  }
//
//  if (!configFile.validate(buffer, bufferLen)) {
//#ifdef DEBUG
//    Serial.println(F("setupConfigFile(): INI file is invalid"));
//#endif
//    return;
//  }
//}

void setupAudio() {
  audio.speakerPin = SPEAKER_PIN;
  audio.setVolume(soundVolume());
  soundAvailable = true;
}

/* -------------------------------------------------------------------------------------------------- */

void processButtonPresses() {
  buttonMain.tick();
  buttonAux.tick();
}

boolean amBackFromSleep() {
#ifdef DEBUG
  Serial.print(F("amBackFromSleep(): justBackFromSleep: "));
  Serial.println(justBackFromSleep);
#endif
  if (justBackFromSleep) { 
    justBackFromSleep = false;
    return true;
  } else {
    return false;
  }
}

void mainButtonPress() {
#ifdef DEBUG
  Serial.print(F("mainButtonPress(): saberOn: "));
  Serial.println(saberOn);
#endif
  if (amBackFromSleep()) { return; }

  if (saberOn) {
    turnOnButtonAuxLED(); 
    playSoundFontSwingSound();
  } else {
    turnSaberOn();
  }
}

void mainButtonLongPress() {
#ifdef DEBUG
  Serial.print(F("mainButtonLongPress(): saberOn: "));
  Serial.println(saberOn);
#endif
  if (amBackFromSleep()) { return; }
  
  if (saberOn) { 
    turnSaberOff();
  } else {
#ifdef ENABLE_SLEEP
    playPoweringDownSound();
    delay(1000);
    goToSleep(true);
#endif
  }
}

void auxButtonPress() {
#ifdef DEBUG
  Serial.print(F("auxButtonPress(): saberOn: "));
  Serial.println(saberOn);
#endif
  if (amBackFromSleep()) { return; }

  if (saberOn) { 
    turnOnButtonAuxLED();
    playSoundFontClashSound();
  } else {
    selectNextSoundFont();
  }
}

void auxButtonLongPress() {
#ifdef DEBUG
  Serial.print(F("auxButtonLongPress(): saberOn: "));
  Serial.println(saberOn);
#endif
  if (amBackFromSleep()) { return; }
  
  if (saberOn) { playLockupSound(); }
}

/* -------------------------------------------------------------------------------------------------- */

void turnOnButtonMainLED() {
  stopOscillateButtonMainLED();
  delay(100);
  digitalWrite(BUTTON_MAIN_LED_PIN, HIGH);
}

void flashButtonMainLED(byte wait=50) {
  digitalWrite(BUTTON_MAIN_LED_PIN, HIGH);
  delay(wait);
  digitalWrite(BUTTON_MAIN_LED_PIN, LOW);
}

void turnOffButtonMainLED() {
  stopOscillateButtonMainLED();
  delay(100);
  digitalWrite(BUTTON_MAIN_LED_PIN, LOW);
}

void startIdleMonitor() {
  secondsIdle = 0;
#ifdef ENABLE_SLEEP  
  checkIfShouldSleepEvent = t.every(1000, checkIfShouldSleep);
#endif
}

void stopIdleMonitor() {
#ifdef DEBUG
  Serial.print(F("stopIdleMonitor(): checkIfShouldSleepEvent: "));
  Serial.println(checkIfShouldSleepEvent);
#endif
  if (checkIfShouldSleepEvent > 0) {
    t.stop(checkIfShouldSleepEvent);
    checkIfShouldSleepEvent = 0;
  }
}

void startOscillateButtonMainLED(unsigned int speed=750) {
  stopOscillateButtonMainLED();
  delay(10);
  buttonMainLEDEvent = t.oscillate(BUTTON_MAIN_LED_PIN, speed, HIGH);  
}

void stopOscillateButtonMainLED() {
#ifdef DEBUG
  Serial.print(F("stopOscillateButtonMainLED(): buttonMainLEDEvent: "));
  Serial.println(buttonMainLEDEvent);
#endif
  if (buttonMainLEDEvent > 0) {
    t.stop(buttonMainLEDEvent);
    buttonMainLEDEvent = 0;
  }
}

void turnOnButtonAuxLED() {
  stopOscillateButtonAuxLED();
  digitalWrite(BUTTON_AUX_LED_PIN, HIGH);
}

void turnOffButtonAuxLED() {
  stopOscillateButtonAuxLED();
  digitalWrite(BUTTON_AUX_LED_PIN, LOW);
}

void startOscillateButtonAuxLED(unsigned int speed=750) {
  stopOscillateButtonAuxLED();
  delay(10);
  buttonAuxLEDEvent = t.oscillate(BUTTON_AUX_LED_PIN, speed, HIGH);  
}

void stopOscillateButtonAuxLED() {
#ifdef DEBUG
  Serial.print(F("stopOscillateButtonAuxLED(): buttonAuxLEDEvent: "));
  Serial.println(buttonAuxLEDEvent);
#endif
  if (buttonAuxLEDEvent > 0) {
    t.stop(buttonAuxLEDEvent);
    buttonAuxLEDEvent = 0;
  }
}

/* -------------------------------------------------------------------------------------------------- */

void waitForSoundToFinish() {
  if (soundAvailable) { while (audio.isPlaying()) { }; }
}

void getCurrentSoundFontDirectory(char *soundFontDirectory) {
  char name[MAX_SOUND_FONT_FOLDER_LENGTH] = "";

#ifdef DEBUG
  Serial.println(F("getCurrentSoundFontDirectory(): HERE"));
#endif 

  getStoredSoundFontName(name);

#ifdef DEBUG
  Serial.print(F("getCurrentSoundFontDirectory(): name: "));
  Serial.println(name);
#endif 
  
  sprintf(soundFontDirectory, "/fonts/%s", name);
}

void getFullSoundFontFilePath(char *filename, char *fullFilename) {
  char soundFontDirectory[80] = "";
  char name[MAX_SOUND_FONT_FOLDER_LENGTH] = "";

#ifdef DEBUG
  Serial.println(F("getFullSoundFontFilePath(): HERE"));
#endif 

  getCurrentSoundFontDirectory(soundFontDirectory);
  getStoredSoundFontName(name);

#ifdef DEBUG
  Serial.print(F("getFullSoundFontFilePath(): name: "));
  Serial.println(name);
#endif 
  
  sprintf(fullFilename, "%s/%s.wav", soundFontDirectory, filename);
}

void getFullSoundInternalFilePath(char *filename, char *fullFilename) { 
  sprintf(fullFilename, "/internal/%s.wav", filename);
}

void stopSound(boolean stopLooping=false) {
  audio.stopPlayback();
  if (stopLooping) { audio.loop(false); }
}

void playSound(char *filename, boolean force=false, boolean loopIt=false) {
  char str[100];

#ifdef DEBUG
  sprintf(str, "playSound(): %s (force: %d, loop: %d)", filename, force, loopIt);
  Serial.println(str);
#endif
  
  if (force) {
    if (soundAvailable) { stopSound(); }
  } else {
    waitForSoundToFinish();
  }

  if (playSounds && soundAvailable) {
    audio.play(filename);
    audio.loop(loopIt);
  }
}

void playFontSound(char *filename, boolean force=false, boolean loopIt=false) {
  char fullFilename[80];

#ifdef DEBUG_WIP
  Serial.print(F("playFontSound(): filename: "));
  Serial.print(filename);
  Serial.print(F(", force: "));
  Serial.print(force);
  Serial.print(F(", loopIt: "));
  Serial.println(loopIt);
#endif 

  getFullSoundFontFilePath(filename, fullFilename);

#ifdef DEBUG
  Serial.print(F("playFontSound(): fullFilename: "));
  Serial.println(fullFilename);
#endif 
  
  playSound(fullFilename, force, loopIt);
}

void playInternalSound(char *filename, boolean force=false) {
  char fullFilename[80];

#ifdef DEBUG
  Serial.print(F("playInternalSound(): filename: "));
  Serial.print(filename);
  Serial.print(F(", force: "));
  Serial.println(force);
#endif 

  getFullSoundInternalFilePath(filename, fullFilename);

#ifdef DEBUG
  Serial.print(F("playInternalSound(): fullFilename: "));
  Serial.println(fullFilename);
#endif 
  
  playSound(fullFilename, force);
}

void loopSound(char *filename, boolean force=false) {
  playSound(filename, force, true);
}

void loopFontSound(char *filename, boolean force=false) {
  playFontSound(filename, force, true);
}

void playOKSound() {
  playInternalSound(OKAY_SOUND_FILE_NAME);
}

void playBootSound() {
  playInternalSound(BOOT_SOUND_FILE_NAME);
}

void playPoweringDownSound() {
  playInternalSound(POWERING_DOWN_SOUND_FILE_NAME);
}

void playSoundFontIdleSound(boolean force=false) {
  loopFontSound(IDLE_SOUND_FILE_NAME, force);
}

void playLockupSound() {
  if (!playingLockupSound) {
    playingLockupSound = true;
    startOscillateButtonAuxLED(100);
    loopSound(LOCKUP_SOUND_FILE_NAME, true);
  } else {
    playingLockupSound = false;    
    turnOnButtonAuxLED();
    playSoundFontIdleSound(true);
  }
}

void playSoundFontPowerOnSound() {
  playFontSoundWithHum(POWER_ON_SOUND_FILE_NAME);
}

void playSoundFontPowerOffSound() {
  playFontSound(POWER_OFF_SOUND_FILE_NAME, true);
}

boolean fileExistsOnSDCard(char *file) {
  stopSound(true);
//  delay(100);
  return sd.exists(file);  
}

boolean nextSoundFile(char *format, byte *number, char *filename) { 
  char tmpFilename[MAX_SOUND_FONT_FOLDER_LENGTH] = "";
  char fullFilename[80] = "";

  while (true) {
    sprintf(tmpFilename, format, *number);
    getFullSoundFontFilePath(tmpFilename, fullFilename);

#ifdef DEBUG
    Serial.print(F("nextSoundFile(): fullFilename: "));
    Serial.println(fullFilename);
#endif

    if (fileExistsOnSDCard(fullFilename)) {
#ifdef DEBUG
      Serial.println(F("nextSoundFile(): exists!"));
#endif  
      sprintf(filename, tmpFilename);
      break;
    } else {
#ifdef DEBUG
      Serial.println(F("nextSoundFile(): does not exist"));
#endif
      if (*number == 1) {
#ifdef DEBUG
      Serial.println(F("nextSoundFile(): Cannot find a file to play :("));
#endif
        return false;
      }
      
      *number = 1;
    }
  }
  
  *number = *number + 1;

  return true;
}

void playSoundFontSwingSound() {
  char filename[80] = "";

  if (nextSoundFile(SWING_SOUND_FILE_FORMAT, &currentSwingSound, filename)) {
#ifdef DEBUG_WIP
    Serial.print(F("playSoundFontSwingSound(): filename: "));
    Serial.println(filename);
    playFontSoundWithHum(filename);
#endif
  } else {
#ifdef DEBUG
    Serial.println(F("playSoundFontSwingSound(): NO filename found :("));
    playSoundFontIdleSound(true);
#endif
  }
}

void playSoundFontClashSound() {
  char filename[80] = "";

  if (nextSoundFile(CLASH_SOUND_FILE_FORMAT, &currentClashSound, filename)) {
#ifdef DEBUG_WIP
    Serial.print(F("playSoundFontClashSound(): filename: "));
    Serial.println(filename);
    playFontSoundWithHum(filename);
#endif
  } else {
#ifdef DEBUG
    Serial.println(F("playSoundFontClashSound(): NO filename found :("));
    playSoundFontIdleSound(true);
#endif
  }
}

void playSoundFontNameSound() {
  playFontSound(NAME_SOUND_FILE_NAME);
}

void playFontSoundWithHum(char *filename) {
  playFontSound(filename, true);
  playSoundFontIdleSound();
}

/* -------------------------------------------------------------------------------------------------- */

void turnSaberOn() {
  if (!saberOn) {
#ifdef DEBUG
  Serial.println(F("turnSaberOn(): Turning saber ON!"));
#endif
    stopIdleMonitor();
    
    turnOnButtonMainLED();
    turnOnButtonAuxLED();
    
    playSoundFontPowerOnSound();
    saberOn = true;
  }
}

void turnSaberOff() {
  if (saberOn) {
#ifdef DEBUG
  Serial.println(F("turnSaberOff(): Turning saber OFF!"));
#endif        
    playSoundFontPowerOffSound();

    waitForSoundToFinish();

    startOscillateButtonMainLED();
    startOscillateButtonAuxLED();
    
    startIdleMonitor();
    
    saberOn = false;
  }
}

void selectNextSoundFont() {
  if (!saberOn) {
#ifdef DEBUG
  Serial.println(F("selectNextSoundFont(): HERE"));
#endif        
    selectNextSoundFontName();
    playSoundFontNameSound();
  }
}

void selectNextSoundFontName() {
  char name[MAX_SOUND_FONT_FOLDER_LENGTH];
  char availableSoundFontNames[MAX_SOUND_FONT_COUNT][MAX_SOUND_FONT_FOLDER_LENGTH];

#ifdef DEBUG
  Serial.println(F("selectNextSoundFontName(): HERE"));
#endif

  getNextSoundFontName(name);

#ifdef DEBUG
  Serial.print(F("selectNextSoundFontName(): name: "));
  Serial.println(name);
#endif
  
  storeSelectedSoundFontName(name);
}

void getNextSoundFontName(char *name) {
  byte index = 0;
  char availableSoundFontNames[MAX_SOUND_FONT_COUNT][MAX_SOUND_FONT_FOLDER_LENGTH];

  if (getIndexForSoundFontName(&index, name)) {
    if (index < (MAX_SOUND_FONT_COUNT - 1)) {
#ifdef DEBUG
      Serial.print(F("getNextSoundFontName(): index: "));
      Serial.print(index);
#endif
      index = index + 1;
#ifdef DEBUG
      Serial.print(F(", index: "));
      Serial.println(index);
#endif      
    } else {
#ifdef DEBUG
      Serial.println(F("getNextSoundFontName(): index: 0 (forced)"));
#endif
      index = 0;
    }
  } else {
#ifdef DEBUG
    Serial.println(F("getNextSoundFontName(): index: 0 (returned false)"));
#endif    
    index = 0;
  }
  
  getAvailableSoundFontNames(availableSoundFontNames);

#ifdef DEBUG
  Serial.print(F("getNextSoundFontName(): availableSoundFontNames["));
  Serial.print(index);
  Serial.print(F("]: "));
  Serial.println(availableSoundFontNames[index]);
#endif  

  sprintf(name, availableSoundFontNames[index]);
}

boolean getIndexForSoundFontName(byte *index, char *name) {
  char storedSoundFontName[MAX_SOUND_FONT_FOLDER_LENGTH] = "";
  char availableSoundFontNames[MAX_SOUND_FONT_COUNT][MAX_SOUND_FONT_FOLDER_LENGTH];

  getStoredSoundFontName(storedSoundFontName);
  getAvailableSoundFontNames(availableSoundFontNames);

#ifdef DEBUG
  Serial.print(F("getIndexForSoundFontName(): storedSoundFontName: "));
  Serial.println(storedSoundFontName);
#endif

  if (!storedSoundFontName) {
#ifdef DEBUG
    Serial.println(F("getIndexForSoundFontName(): !storedSoundFontName - return false"));
#endif    
    return false;
  }

  for (byte i = 0 ; i < MAX_SOUND_FONT_COUNT ; i++) {
#ifdef DEBUG
    Serial.print(F("getIndexForSoundFontName(): availableSoundFontNames["));
    Serial.print(i);
    Serial.print(F("]: "));
    Serial.println(availableSoundFontNames[i]);
#endif
    
    if (strcmp(availableSoundFontNames[i], storedSoundFontName) == 0) {
      *index = i;

#ifdef DEBUG
      Serial.print(F("getIndexForSoundFontName(): index: "));
      Serial.print(i);
      Serial.println(F(", return true"));
#endif
      return true;
    }
  }

#ifdef DEBUG
  Serial.println(F("getIndexForSoundFontName(): return false"));
#endif
  
  return false;
}

void getCurrentSoundFontName(char *name) {
  byte index = 0;
  boolean nameValid;

#ifdef DEBUG
  Serial.println(F("getCurrentSoundFontName(): HERE"));
#endif

  getStoredSoundFontName(name);

#ifdef DEBUG
  Serial.print(F("getCurrentSoundFontName(): name: "));
  Serial.println(name);
#endif

  nameValid = getIndexForSoundFontName(&index, name);

#ifdef DEBUG
  Serial.print(F("getCurrentSoundFontName(): strlen(name) == 0: "));
  Serial.print(strlen(name) == 0);
  Serial.print(F(" || !nameValid: "));
  Serial.println(nameValid);
#endif

  if (strlen(name) == 0 || !nameValid) {

#ifdef DEBUG
  Serial.println(F("getCurrentSoundFontName(): (strlen(name) == 0 || !nameValid) == true "));
#endif
  
    getNextSoundFontName(name);
    storeSelectedSoundFontName(name);
  }

#ifdef DEBUG
  Serial.print(F("getCurrentSoundFontName(): name: "));
  Serial.println(name);
#endif
}

void getStoredSoundFontName(char *name) {
  char str[MAX_SOUND_FONT_FOLDER_LENGTH] = "";

  EEPROM.writeBlock<char>(addressCharArray, "classic", MAX_SOUND_FONT_FOLDER_LENGTH);
  EEPROM.readBlock<char>(addressCharArray, str, MAX_SOUND_FONT_FOLDER_LENGTH);

#ifdef DEBUG
  Serial.print(F("getStoredSoundFontName(): str: "));
  Serial.println(str);
#endif
  
  sprintf(name, str);
}

void storeSelectedSoundFontName(char *name) {
#ifdef DEBUG
  Serial.print(F("storeSelectedSoundFontName(): Storing font: "));
  Serial.println(name);
#endif
  
  EEPROM.writeBlock<char>(addressCharArray, name, MAX_SOUND_FONT_FOLDER_LENGTH);
}

void getAvailableSoundFontNames(char soundFonts[][MAX_SOUND_FONT_FOLDER_LENGTH]) {
  FatFile root;
  FatFile file;
  byte counter = 0;
  char buffer[MAX_SOUND_FONT_FOLDER_LENGTH] = "";

  waitForSoundToFinish();
  root.open("/fonts");

  while (file.openNext(&root, O_READ)) {
    file.getName(buffer, MAX_SOUND_FONT_FOLDER_LENGTH);
#ifdef DEBUG
    Serial.print(F("getAvailableSoundFontNames(): "));
    Serial.println(buffer);
#endif    
    sprintf(soundFonts[counter++], buffer);
    file.close();
  }

  root.close();
}

/* -------------------------------------------------------------------------------------------------- */

unsigned int soundVolume() {
  int currentVolume = 4;

  return currentVolume;
}

//bool readConfig(char *section, char *key) {
//  char buf[MAX_SOUND_FONT_FOLDER_LENGTH];
//
//  return false;
//
//  if (configFile.getValue(section, key, buf, MAX_SOUND_FONT_FOLDER_LENGTH)) {
//#ifdef DEBUG
//    Serial.print(F("readConfig(): "));
//    Serial.println(buf);
//#endif
//    return true;
//  } else {
//#ifdef DEBUG
//    Serial.print(F("readConfig(): Could not read value"));
//#endif
//    return false;
//  }
//}

/* -------------------------------------------------------------------------------------------------- */

//void processMovement() {
//  long multiplier = 100000;
//  VectorInt16 aa;
//  VectorInt16 aaReal;
//
//  VectorFloat gravity;
//
//  while (!mpuInterrupt && fifoCount < packetSize) { }
//
//  mpuInterrupt = false;
//  mpuIntStatus = mpu.getIntStatus();
//
//  fifoCount = mpu.getFIFOCount();
//
//  if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
//    mpu.resetFIFO();
//
//  } else if (mpuIntStatus & 0x02) {
//    while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();
//
//    mpu.getFIFOBytes(fifoBuffer, packetSize);
//    fifoCount -= packetSize;
//
//    ql = qr;
//    aaWorld_last = aaWorld_reading;
//
//    mpu.dmpGetQuaternion(&qr, fifoBuffer);
//    mpu.dmpGetGravity(&gravity, &qr);
//    mpu.dmpGetAccel(&aa, fifoBuffer);
//    mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
//    mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &qr);
//
//    q.w = qr.w * multiplier - ql.w * multiplier;
//    q.x = qr.x * multiplier - ql.x * multiplier;
//    q.y = qr.y * multiplier - ql.y * multiplier;
//    q.z = qr.z * multiplier - ql.z * multiplier;
//
//    magnitude = 1000 * sqrt(sq(aaWorld.x / 1000) + sq(aaWorld.y / 1000) + sq(aaWorld.z / 1000));
//  }
//}
//
//void checkForMovement() {
//  char str[24];
//  unsigned int val = 1200;
//
//#ifdef DEBUG
//  sprint(str, "%d %d %d %d", (abs(q.w) > val), (aaWorld.z < 0), (abs(q.z) < (9 / 2) * val), ((abs(q.x) > (3 * val)) or (abs(q.y) > (3 * val))));
//  Serial.println(str);
//#endif
//
//  if (
//      abs(q.w) > val
//      && aaWorld.x < 0
//      && abs(q.x) < (9 / 2) * val
//      && ((abs(q.z) > (3 * val)) || (abs(q.y) > (3 * val)))
//     )
//  {
//#ifdef DEBUG
//    Serial.println(F("checkForMovement(): Movement!"));
//#endif
//    playSoundFontSwingSound();
//  }
//}

/* -------------------------------------------------------------------------------------------------- */

#ifdef ENABLE_SLEEP
void wakeUpNow() {
  sleeping = false;
  justBackFromSleep = true;
}

void checkIfShouldSleep() {
  if (secondsIdle < MAX_SECONDS_BEFORE_SLEEP) {
#ifdef DEBUG
  Serial.print(F("checkIfShouldSleep(): Not sleeping, "));
  Serial.print(secondsIdle);
  Serial.print(F(" < "));
  Serial.println(MAX_SECONDS_BEFORE_SLEEP);  
#endif    
    secondsIdle++;
  } else {
#ifdef ENABLE_SLEEP    
    goToSleep(false);
#endif    
  }
}

void goToSleep(boolean force) {
  long counter = 0;

#ifdef DEBUG
  Serial.println(F("goToSleep(): HERE"));
#endif
  delay(100);

  secondsIdle = 0;
  sleeping = true;

  turnOffButtonMainLED();
  turnOffButtonAuxLED();

  stopIdleMonitor(); 

  while (sleeping) {  
    counter = counter + 8;
    attachInterrupt(0, wakeUpNow, LOW);

    if (force || counter >= MAX_SECONDS_BEFORE_DEEP_SLEEP) {
#ifdef DEBUG
  Serial.println(F("goToSleep(): Sleeping FOREVER."));
  delay(100);
#endif              
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);   
    } else {
#ifdef DEBUG
  Serial.print(F("goToSleep(): Sleeping for 8 secs, counter: "));
  Serial.print(counter);
  Serial.print(F(" < "));
  Serial.println(MAX_SECONDS_BEFORE_DEEP_SLEEP);
  delay(100);
#endif        
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
    }
    
#ifdef DEBUG
    Serial.println(F("goToSleep(): flashButtonMainLED()"));
    delay(100);
#endif    
    flashButtonMainLED();
    detachInterrupt(0);
  }

  startIdleMonitor();

  startOscillateButtonMainLED();
  startOscillateButtonAuxLED();
}
#endif

/* -------------------------------------------------------------------------------------------------- */

void setup() {
  setupSerial();
  
#ifdef DEBUG
  Serial.println(F("setup(): Starting.."));
#endif
  
  setupLED();
  setupSwitches();
  setupSDCard();
//  setupConfigFile();
//  setupAccelerometer();
  setupAudio();

  playSoundFontNameSound();

  t.every(10, processButtonPresses);
  
  startIdleMonitor();
  
  startOscillateButtonMainLED();
  startOscillateButtonAuxLED(); 

#ifdef DEBUG
  Serial.println(F("setup(): Ready!"));
#endif
}

void loop() {  
  if (saberOn) {
//    processMovement();
//    checkForMovement();
  }

  t.update();
}
