#include <SdFat.h>
#include <TMRpcm.h>
#include <OneButton.h>
#include <EEPROMex.h>

//#include <MPU6050_6Axis_MotionApps20.h>

//#include <IniFile.h>

#define SD_SELECT_PIN    4
#define BUTTON_MAIN_PIN  5
#define BUTTON_AUX_PIN   6
#define SPEAKER_PIN      9
#define LED_PIN          13

#define BUTTON_CLICK_TICKS  200
#define BUTTON_PRESS_TICKS  700

#define MAX_SOUND_FONT_COUNT 4

#define SOUND_FONTS_BASE "/fonts"

#define OKAY_SOUND_FILE_NAME "/internal/okay.wav"
//#define BOOT_SOUND_FILE_NAME "/internal/boot.wav"
#define LOCKUP_SOUND_FILE_NAME "/internal/lockup.wav"

#define NAME_SOUND_FILE_NAME "name.wav"
#define IDLE_SOUND_FILE_NAME "idle.wav"
#define POWER_ON_SOUND_FILE_NAME "poweron.wav"
#define POWER_OFF_SOUND_FILE_NAME "poweroff.wav"
#define SWING_SOUND_FILE_NAME "swing.wav"
#define CLASH_SOUND_FILE_NAME "clash.wav"

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

//volatile bool mpuInterrupt = false;
//
//void dmpDataReady() {
//  mpuInterrupt = true;
//}

/* -------------------------------------------------------------------------------------------------- */

unsigned int addressInt;
unsigned int addressCharArray;

unsigned int saberOn = 0;
unsigned int soundAvailable = 0;
unsigned int playingLockupSound = 0;

long availableSoundFontCount = 0;
long currentSoundFontIndex = 0;
char currentSoundFontName[20];
char availableSoundFonts[MAX_SOUND_FONT_COUNT][20];

/* -------------------------------------------------------------------------------------------------- */

void StreamPrint_progmem(Print &out, PGM_P format, ...) {
  if (Serial.available()) { return; }
  char formatString[128], *ptr;
  strncpy_P(formatString, format, sizeof(formatString));
  formatString[sizeof(formatString) - 2] = '\0'; 
  ptr = &formatString[strlen(formatString) + 1];
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

void setupLED() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  
}

//void setupAccelerometer() {
//  Wire.begin();
//  mpu.initialize();
//
//  if (!mpu.testConnection()) {
////    Serialprint("MPU6050 failed\n");
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
////  Serialprint("MPU6050 ready!\n");
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
//    Serialprint("sd.begin() failed\n");
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
//  audio.setVolume(volume);
  soundAvailable = 1;
}

/* -------------------------------------------------------------------------------------------------- */

void mainButtonPress() {
  //  Serialprint("MAIN press!\n");
  (saberOn) ? playFontSoundWithHum(SWING_SOUND_FILE_NAME) : turnSaberOn();
}

void auxButtonPress() {
//  Serialprint("AUX press!\n");
  (saberOn) ? playFontSoundWithHum(CLASH_SOUND_FILE_NAME) : changeSoundFont();
}

void auxButtonLongPress() {
  if (saberOn) {
    playLockupSound();
  }
}

/* -------------------------------------------------------------------------------------------------- */

void waitForSoundToFinish() {
  if (soundAvailable) { while (audio.isPlaying()) { };
  }
}

void currentSoundFontDirectory(char *filename, char *fullFilename) {  
  sprintf(fullFilename, "/fonts/%s/%s", currentSoundFontName, filename);
}

void playSound(char *filename, boolean force=false, boolean loopIt=false) {
  if (force) {
    if (soundAvailable) { audio.stopPlayback(); }
  } else {
    waitForSoundToFinish();
  }

  Serialprint("Playing '%s' (force: %d, loop: %d)\n", filename, force, loopIt);
 
  if (soundAvailable) {
    audio.play(filename);
    audio.loop(loopIt);
  }
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


//void playOKSound() {
//  playSound(OKAY_SOUND_FILE_NAME);
//}

//void playBootSound() {
//  playSound(BOOT_SOUND_FILE_NAME);
//}

void playSoundFontIdleSound(boolean force=false) {
  loopFontSound("idle.wav", force);
}

void playLockupSound() {  
  if (!playingLockupSound) {
    playingLockupSound = 1;
    loopSound(LOCKUP_SOUND_FILE_NAME, true);
  } else {
    playingLockupSound = 0;
    playSoundFontIdleSound(true);
  }
}

//void playSoundFontPowerOnSound() {
//  playFontSoundWithHum(POWER_ON_SOUND_FILE_NAME);
//}

//void playSoundFontPowerOffSound() {
//  playFontSound(POWER_OFF_SOUND_FILE_NAME, true);
//}

//void playSoundFontSwingSound() {
//  playFontSoundWithHum(SWING_SOUND_FILE_NAME);
//}

//void playSoundFontClashSound() {
//  playFontSoundWithHum(CLASH_SOUND_FILE_NAME);
//}

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
    saberOn = 1;
  }
}

void turnSaberOff() {
  if (saberOn) {    
    playFontSound(POWER_OFF_SOUND_FILE_NAME, true);
    setLED(LOW);
    saberOn = 0;
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
  EEPROM.writeBlock<char>(addressCharArray, currentSoundFontName, 20); 
}

void getCurrentSoundFont(char *fontName) {
  EEPROM.readBlock<char>(addressCharArray, fontName, 20);
  sprintf(currentSoundFontName, fontName);
  fontName = "classic";
}

long getCurrentSoundFontIndex() {
  long index;
  
  index = EEPROM.readInt(addressInt);
  currentSoundFontIndex = index;
  
  return index;
}

void setFirstSoundFontIfNotSet() {
  long soundFontIndex;
  char soundFontName[20];
  char *soundFonts[MAX_SOUND_FONT_COUNT];

  soundFontIndex = getCurrentSoundFontIndex();
  getCurrentSoundFont(soundFontName);

  if (strcmp(availableSoundFonts[soundFontIndex], soundFontName) != 0) {
    selectSoundFont(0);
  }
}

/* -------------------------------------------------------------------------------------------------- */

unsigned int soundVolume() {
  int currentVolume = 4;
  
  return currentVolume;
}

//bool readConfig(char *section, char *key) { 
//  char buf[20];
//
//  return false;
//  
//  if (configFile.getValue(section, key, buf, 20)) {
//    Serial.println(buf);
//    return true;
//  } else {
//    Serialprint("%s\n", "Could not read value");
//    return false;
//  }
//}

void discoverSoundFonts() {
  FatFile root;
  FatFile file;
  unsigned int counter = 0;
  char buffer[20];

  waitForSoundToFinish();

  root.open(SOUND_FONTS_BASE);

  while (file.openNext(&root, O_READ)) {
    file.getName(buffer, 20);
//    Serialprint("Found sound font %d: '%s'\n", counter, buffer);
    strncpy(availableSoundFonts[counter++], buffer, strlen(buffer));
    file.close();    
  }

  root.close();

  availableSoundFontCount = counter;
}

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
//  unsigned int val = 1200;
//
////  Serialprint("%d %d %d %d\n", (abs(q.w) > val), (aaWorld.z < 0), (abs(q.z) < (9 / 2) * val), ((abs(q.x) > (3 * val)) or (abs(q.y) > (3 * val))));
//   
//  if (
//      abs(q.w) > val
//      && aaWorld.x < 0 
//      && abs(q.x) < (9 / 2) * val 
//      && ((abs(q.z) > (3 * val)) || (abs(q.y) > (3 * val)))
//     )
//  {
//    //Serialprint("Movement!\n");
//    playFontSoundWithHum(SWING_SOUND_FILE_NAME);
//  }
//}

/* -------------------------------------------------------------------------------------------------- */

void setup() {
  Serial.begin(38400);
  
  addressInt = EEPROM.getAddress(sizeof(int));
  addressCharArray = EEPROM.getAddress(sizeof(char) * 20); 

  setupLED();
  setupSwitches();
  setupSDCard();
//  setupConfigFile();
  setupAudio(soundVolume());
//  setupAccelerometer();
  
  discoverSoundFonts();
  setFirstSoundFontIfNotSet();
  
  playSoundFontNameSound();
}

void loop() {  
  buttonMain.tick();
  buttonAux.tick();
  
//  if (saberOn) {
//    processMovement();
//    checkForMovement();
//  }
}
