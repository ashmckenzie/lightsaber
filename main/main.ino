#include <avr/sleep.h>

#include <SdFat.h>
#include <TMRpcm.h>
#include <OneButton.h>
#include <EEPROMex.h>
#include <Timer.h>

//#include <MPU6050_6Axis_MotionApps20.h>
//#include <IniFile.h>

//#define DEBUG
//#define DEBUG_MORE
#define DEBUG_WIP

#define SD_SELECT_PIN       4
#define BUTTON_MAIN_PIN     2
#define BUTTON_MAIN_LED_PIN 7
#define BUTTON_AUX_PIN      6
#define BUTTON_AUX_LED_PIN  8
#define SPEAKER_PIN         9
#define LED_PIN            13

#define BUTTON_CLICK_TICKS  200
#define BUTTON_PRESS_TICKS  700

#define MAX_SECONDS_BEFORE_SLEEP 30
#define MAX_SECONDS_BEFORE_DEEP_SLEEP 30

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
byte goToSleepEvent = 0;
byte secondsIdle = 0;

boolean playSounds = true;

boolean saberOn = false;
boolean soundAvailable = false;
boolean playingLockupSound = false;
boolean backFromSleep = false;

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
//#ifdef DEBUG_MORE
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
//#ifdef DEBUG_MORE
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

  if (!sd.begin(SD_SELECT_PIN)) {
#ifdef DEBUG_MORE    
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
//#ifdef DEBUG_MORE
//    Serial.println(F("setupConfigFile(): INI file does not exist"));
//#endif
//    return;
//  }
//
//  if (!configFile.validate(buffer, bufferLen)) {
//#ifdef DEBUG_MORE
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

void processButtonPresses() {
  buttonMain.tick();
  buttonAux.tick();
}

boolean amBackFromSleep() {
#ifdef DEBUG_MORE
  Serial.print(F("amBackFromSleep(): press! backFromSleep: "));
  Serial.println(backFromSleep);
#endif
  if (backFromSleep) { 
    backFromSleep = false;
    return true;
  } else {
    return false;
  }
}

void mainButtonPress() {
#ifdef DEBUG_MORE
  Serial.print(F("mainButtonPress(): press! saberOn: "));
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
#ifdef DEBUG_MORE
  Serial.print(F("mainButtonLongPress(): press! saberOn: "));
  Serial.println(saberOn);
#endif
  if (amBackFromSleep()) { return; }
  
  if (saberOn) { turnSaberOff(); }
}

void auxButtonPress() {
#ifdef DEBUG_MORE
  Serial.print(F("auxButtonPress(): press! saberOn: "));
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
#ifdef DEBUG_MORE
  Serial.print(F("auxButtonLongPress(): press! saberOn: "));
  Serial.println(saberOn);
#endif
  if (amBackFromSleep()) { return; }
  
  if (saberOn) { playLockupSound(); }
}

/* -------------------------------------------------------------------------------------------------- */

void turnOnButtonMainLED() {
  stopOscillateButtonMainLED();
  digitalWrite(BUTTON_MAIN_LED_PIN, HIGH);
}

void flashButtonMainLED(byte wait=100) {
  digitalWrite(BUTTON_MAIN_LED_PIN, HIGH);
  delay(wait);
  digitalWrite(BUTTON_MAIN_LED_PIN, LOW);
}

void turnOffButtonMainLED() {
  stopOscillateButtonMainLED();
  digitalWrite(BUTTON_MAIN_LED_PIN, LOW);
}

void startSleepWatchDog() {
  goToSleepEvent = t.every(1000, goToSleep);
}

void stopSleepWatchDog() {
#ifdef DEBUG_MORE
  Serial.print(F("stopSleepWatchDog(): goToSleepEvent: "));
  Serial.println(goToSleepEvent);
#endif
  if (goToSleepEvent > 0) {
    t.stop(goToSleepEvent);
    goToSleepEvent = 0;
  }
}

void startOscillateButtonMainLED(unsigned int speed=750) {
  stopOscillateButtonMainLED();
  delay(10);
  buttonMainLEDEvent = t.oscillate(BUTTON_MAIN_LED_PIN, speed, HIGH);  
}

void stopOscillateButtonMainLED() {
#ifdef DEBUG_MORE
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
#ifdef DEBUG_MORE
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

void getCurrentSoundFontDirectory(char *filename, char *fullFilename) {
  char name[MAX_SOUND_FONT_FOLDER_LENGTH] = "";

#ifdef DEBUG_MORE
  Serial.println(F("getCurrentSoundFontDirectory(): HERE"));
#endif 

  getStoredSoundFontName(name);

#ifdef DEBUG_MORE
  Serial.print(F("getCurrentSoundFontDirectory(): name: "));
  Serial.println(name);
#endif 
  
  sprintf(fullFilename, "/fonts/%s/%s", name, filename);
}

void playSound(char *filename, boolean force=false, boolean loopIt=false) {
  char str[100];

#ifdef DEBUG || DEBUG_MORE
  sprintf(str, "playSound(): Playing '%s' (playSounds: %d, force: %d, loop: %d)", filename, playSounds, force, loopIt);
  Serial.println(str);
#endif
  
  if (force) {
    if (soundAvailable) { audio.stopPlayback(); }
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

#ifdef DEBUG_MORE
  Serial.print(F("playFontSound(): filename: "));
  Serial.print(filename);
  Serial.print(F(", force: "));
  Serial.print(force);
  Serial.print(F(", loopIt: "));
  Serial.println(loopIt);
#endif 

  getCurrentSoundFontDirectory(filename, fullFilename);

#ifdef DEBUG_MORE
  Serial.print(F("playFontSound(): fullFilename: "));
  Serial.println(fullFilename);
#endif 
  
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
#ifdef DEBUG || DEBUG_MORE
  Serial.println(F("turnSaberOn(): Turning saber ON!"));
#endif
    stopSleepWatchDog();
    
    turnOnButtonMainLED();
    turnOnButtonAuxLED();
    
    playSoundFontPowerOnSound();
    saberOn = true;
  }
}

void turnSaberOff() {
  if (saberOn) {
#ifdef DEBUG || DEBUG_MORE
  Serial.println(F("turnSaberOff(): Turning saber OFF!"));
#endif        
    playSoundFontPowerOffSound();

    waitForSoundToFinish();

    startOscillateButtonMainLED();
    startOscillateButtonAuxLED();
    startSleepWatchDog();
    
    saberOn = false;
  }
}

void selectNextSoundFont() {
  if (!saberOn) {
#ifdef DEBUG_MORE
  Serial.println(F("selectNextSoundFont(): HERE"));
#endif        
    selectNextSoundFontName();
    playSoundFontNameSound();
  }
}

void selectNextSoundFontName() {
  char name[MAX_SOUND_FONT_FOLDER_LENGTH];
  char availableSoundFontNames[MAX_SOUND_FONT_COUNT][MAX_SOUND_FONT_FOLDER_LENGTH];

#ifdef DEBUG_MORE
  Serial.println(F("selectNextSoundFontName(): HERE"));
#endif

  getNextSoundFontName(name);

#ifdef DEBUG_MORE
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
#ifdef DEBUG_MORE      
      Serial.print(F("getNextSoundFontName(): index: "));
      Serial.print(index);
#endif
      index = index + 1;
#ifdef DEBUG_MORE      
      Serial.print(F(", index: "));
      Serial.println(index);
#endif      
    } else {
#ifdef DEBUG_MORE
      Serial.println(F("getNextSoundFontName(): index: 0 (forced)"));
#endif
      index = 0;
    }
  } else {
#ifdef DEBUG_MORE    
    Serial.println(F("getNextSoundFontName(): index: 0 (returned false)"));
#endif    
    index = 0;
  }
  
  getAvailableSoundFontNames(availableSoundFontNames);

#ifdef DEBUG_MORE
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

#ifdef DEBUG_MORE
  Serial.print(F("getIndexForSoundFontName(): storedSoundFontName: "));
  Serial.println(storedSoundFontName);
#endif

  if (!storedSoundFontName) {
#ifdef DEBUG_MORE    
    Serial.println(F("getIndexForSoundFontName(): !storedSoundFontName - return false"));
#endif    
    return false;
  }

  for (byte i = 0 ; i < MAX_SOUND_FONT_COUNT ; i++) {
#ifdef DEBUG_MORE    
    Serial.print(F("getIndexForSoundFontName(): availableSoundFontNames["));
    Serial.print(i);
    Serial.print(F("]: "));
    Serial.println(availableSoundFontNames[i]);
#endif
    
    if (strcmp(availableSoundFontNames[i], storedSoundFontName) == 0) {
      *index = i;

#ifdef DEBUG_MORE
      Serial.print(F("getIndexForSoundFontName(): index: "));
      Serial.print(i);
      Serial.println(F(", return true"));
#endif
      return true;
    }
  }

#ifdef DEBUG_MORE
  Serial.println(F("getIndexForSoundFontName(): return false"));
#endif
  
  return false;
}

void getCurrentSoundFontName(char *name) {
  byte index = 0;
  boolean nameValid;

#ifdef DEBUG_MORE
  Serial.println(F("getCurrentSoundFontName(): HERE"));
#endif

  getStoredSoundFontName(name);

#ifdef DEBUG_MORE
  Serial.print(F("getCurrentSoundFontName(): name: "));
  Serial.println(name);
#endif

  nameValid = getIndexForSoundFontName(&index, name);

#ifdef DEBUG_MORE
  Serial.print(F("getCurrentSoundFontName(): strlen(name) == 0: "));
  Serial.print(strlen(name) == 0);
  Serial.print(F(" || !nameValid: "));
  Serial.println(nameValid);
#endif

  if (strlen(name) == 0 || !nameValid) {

#ifdef DEBUG_MORE
  Serial.println(F("getCurrentSoundFontName(): (strlen(name) == 0 || !nameValid) == true "));
#endif
  
    getNextSoundFontName(name);
    storeSelectedSoundFontName(name);
  }

#ifdef DEBUG_MORE
  Serial.print(F("getCurrentSoundFontName(): name: "));
  Serial.println(name);
#endif
}

void getStoredSoundFontName(char *name) {
  char str[MAX_SOUND_FONT_FOLDER_LENGTH] = "";

  EEPROM.readBlock<char>(addressCharArray, str, MAX_SOUND_FONT_FOLDER_LENGTH);

#ifdef DEBUG_MORE
  Serial.print(F("getStoredSoundFontName(): str: "));
  Serial.println(str);
#endif
  
  sprintf(name, str);
}

void storeSelectedSoundFontName(char *name) {
#ifdef DEBUG_MORE
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
#ifdef DEBUG_MORE    
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
//#ifdef DEBUG_MORE
//    Serial.print(F("readConfig(): "));
//    Serial.println(buf);
//#endif
//    return true;
//  } else {
//#ifdef DEBUG_MORE
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
//#ifdef DEBUG_MORE
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
//#ifdef DEBUG_MORE
//    Serial.println(F("checkForMovement(): Movement!"));
//#endif
//    playSoundFontSwingSound();
//  }
//}

/* -------------------------------------------------------------------------------------------------- */

void wakeUpNow() {
  backFromSleep = true;

  startOscillateButtonMainLED();
  startOscillateButtonAuxLED();
}

ISR(WDT_vect) {
  if (secondsIdle < MAX_SECONDS_BEFORE_DEEP_SLEEP) {
    flashButtonMainLED();
    secondsIdle++;
  }
}

void goToSleep() {
  if (secondsIdle < MAX_SECONDS_BEFORE_SLEEP) {
#ifdef DEBUG_MORE
  Serial.print(F("goToSleep(): Not sleeping, secondsIdle: "));
  Serial.println(secondsIdle);
#endif    
    secondsIdle++;
    return;
  }
  
#ifdef DEBUG_MORE
  Serial.println(F("goToSleep(): Sleeping.."));
#endif
  delay(150);

  turnOffButtonMainLED();
  turnOffButtonAuxLED();

  secondsIdle = 0;

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  // http://citizen-sensing.org/2013/07/arduino-watchdog/
  MCUSR &= ~(1 << WDRF);
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  WDTCSR = (1<< WDP0) | (1 << WDP1) | (1 << WDP2);
  WDTCSR |= (1 << WDIE);
  
  sleep_enable();
  attachInterrupt(0, wakeUpNow, LOW);
  sleep_mode();
  sleep_disable();
  detachInterrupt(0);
}

void setup() {
  setupSerial();
  
#ifdef DEBUG_MORE
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
  
  startSleepWatchDog();
  startOscillateButtonMainLED();
  startOscillateButtonAuxLED();

#ifdef DEBUG_MORE
  Serial.println(F("setup(): Ready!"));
#endif
}

void loop() {  
//  if (saberOn) {
//    processMovement();
//    checkForMovement();
//  }

  t.update();
}
