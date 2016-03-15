#include <SdFat.h>
#include <TMRpcm.h>
#include <OneButton.h>
#include <EEPROMex.h>

//#include <MPU6050_6Axis_MotionApps20.h>
//#include <IniFile.h>

#define DEBUG

#define SD_SELECT_PIN    4
#define BUTTON_MAIN_PIN  5
#define BUTTON_AUX_PIN   6
#define SPEAKER_PIN      9
#define LED_PIN          13

#define BUTTON_CLICK_TICKS  200
#define BUTTON_PRESS_TICKS  700

#define MAX_SOUND_FONT_COUNT 4
#define MAX_SOUND_FONT_FOLDER_LENGTH  24

#define SOUND_FONTS_BASE "/fonts"

#define OKAY_SOUND_FILE_NAME   "/internal/okay.wav"
#define BOOT_SOUND_FILE_NAME   "/internal/boot.wav"
#define LOCKUP_SOUND_FILE_NAME "/internal/lockup.wav"

#define NAME_SOUND_FILE_NAME      "name.wav"
#define IDLE_SOUND_FILE_NAME      "idle.wav"
#define POWER_ON_SOUND_FILE_NAME  "poweron.wav"
#define POWER_OFF_SOUND_FILE_NAME "poweroff.wav"
#define SWING_SOUND_FILE_NAME     "swing.wav"
#define CLASH_SOUND_FILE_NAME     "clash.wav"

OneButton buttonMain(BUTTON_MAIN_PIN, true);
OneButton buttonAux(BUTTON_AUX_PIN, true);

TMRpcm audio;
SdFat sd;

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

boolean saberOn = false;
boolean soundAvailable = false;
boolean playingLockupSound = false;

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
  Serial.begin(38400);
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
  buttonMain.setClickTicks(BUTTON_CLICK_TICKS);
  buttonMain.setPressTicks(BUTTON_PRESS_TICKS);
  buttonMain.attachClick(mainButtonPress);
  buttonMain.attachLongPressStart(turnSaberOff);

  buttonAux.setClickTicks(BUTTON_CLICK_TICKS);
  buttonAux.setPressTicks(BUTTON_PRESS_TICKS);
  buttonAux.attachClick(auxButtonPress);
  buttonAux.attachLongPressStart(auxButtonLongPress);
}

void setupSDCard() {
  pinMode(SD_SELECT_PIN, OUTPUT);
  digitalWrite(SD_SELECT_PIN, HIGH);

  if (!sd.begin(SD_SELECT_PIN)) {
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
//  audio.setVolume(soundVolume());
  soundAvailable = true;
}

/* -------------------------------------------------------------------------------------------------- */

void mainButtonPress() {
#ifdef DEBUG
  Serial.println(F("mainButtonPress(): press!"));
#endif
  (saberOn) ? playSoundFontSwingSound() : turnSaberOn();
}

void auxButtonPress() {
#ifdef DEBUG
  Serial.println(F("auxButtonPress(): press!"));
#endif  
  (saberOn) ? playSoundFontClashSound() : selectNextSoundFont();
}

void auxButtonLongPress() {
  if (saberOn) {
    playLockupSound();
  }
}

/* -------------------------------------------------------------------------------------------------- */

void waitForSoundToFinish() {
  if (soundAvailable) { while (audio.isPlaying()) { }; }
}

void getCurrentSoundFontDirectory(char *filename, char *fullFilename) {
  char currentSoundFontName[MAX_SOUND_FONT_FOLDER_LENGTH];

  getCurrentSoundFontName(currentSoundFontName);
  sprintf(fullFilename, "/fonts/%s/%s", currentSoundFontName, filename);
}

void playSound(char *filename, boolean force=false, boolean loopIt=false) {
  char str[100];
  
  if (force) {
    if (soundAvailable) { audio.stopPlayback(); }
  } else {
    waitForSoundToFinish();
  }

#ifdef DEBUG
  sprintf(str, "playSound(): Playing '%s' (force: %d, loop: %d)", filename, force, loopIt);
  Serial.println(str);
#endif

  if (soundAvailable) {
    audio.play(filename);
    audio.loop(loopIt);
  }
}

void playFontSound(char *filename, boolean force=false, boolean loopIt=false) {
  char fullFilename[60];

  getCurrentSoundFontDirectory(filename, fullFilename);
  playSound(fullFilename, force, loopIt);
}

void loopSound(char *filename, boolean force=false) {
  playSound(filename, force, true);
}

void loopFontSound(char *filename, boolean force=false) {
  playFontSound(filename, force, true);
}

void playOKSound() {
 playSound(OKAY_SOUND_FILE_NAME);
}

void playBootSound() {
 playSound(BOOT_SOUND_FILE_NAME);
}

void playSoundFontIdleSound(boolean force=false) {
  loopFontSound("idle.wav", force);
}

void playLockupSound() {
  if (!playingLockupSound) {
    playingLockupSound = true;
    loopSound(LOCKUP_SOUND_FILE_NAME, true);
  } else {
    playingLockupSound = false;
    playSoundFontIdleSound(true);
  }
}

void playSoundFontPowerOnSound() {
 playFontSoundWithHum(POWER_ON_SOUND_FILE_NAME);
}

void playSoundFontPowerOffSound() {
 playFontSound(POWER_OFF_SOUND_FILE_NAME, true);
}

void playSoundFontSwingSound() {
 playFontSoundWithHum(SWING_SOUND_FILE_NAME);
}

void playSoundFontClashSound() {
 playFontSoundWithHum(CLASH_SOUND_FILE_NAME);
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
    playFontSoundWithHum(POWER_ON_SOUND_FILE_NAME);
    setLED(HIGH);
    saberOn = true;
  }
}

void turnSaberOff() {
  if (saberOn) {
    playFontSound(POWER_OFF_SOUND_FILE_NAME, true);
    setLED(LOW);
    saberOn = false;
  }
}

void selectNextSoundFont() {
  if (!saberOn) {
    selectNextSoundFontName();
    playSoundFontNameSound();
  }
}

void selectNextSoundFontName() {
  char nextSoundFontName[MAX_SOUND_FONT_FOLDER_LENGTH];
  char availableSoundFontNames[MAX_SOUND_FONT_COUNT][MAX_SOUND_FONT_FOLDER_LENGTH];

  getNextSoundFontName(nextSoundFontName);
  storeSelectedSoundFontName(nextSoundFontName);
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

  getStoredSoundFontName(name);

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
  root.open(SOUND_FONTS_BASE);

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

#ifdef DEBUG  
  Serial.println(F("setup(): Ready!"));
#endif
}

void loop() {
  buttonMain.tick();
  buttonAux.tick();

//  if (saberOn) {
//    processMovement();
//    checkForMovement();
//  }
}
