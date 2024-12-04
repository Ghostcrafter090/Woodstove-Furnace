#include <EEPROM.h>

int primaryFan = 2;
int livingRoomFan = 3;
int airConditioner = 4;
int acCompressor = 5;
int draftInducer = 6;

int buttonPin = 7;
bool buttonPressed = false;

unsigned long forceDraftInducer = 0;
bool draftInducerOverride = false;

bool temperatureMode = false; // cooling is false

int speakerPin = 12;

double temperatureSmoothingConstant = 10000;

bool modeList[5] = {false, false, false, false, false};
bool modeSetup[5] = {false, false, false, false, false};

bool diningRoomVentIsOn = false;
bool kitchenVentIsOn = false;

bool livingRoomFanIsOn = false;

double primaryFanWattage = 150.0;
double livingRoomFanWattage = 75.0;
double airConditionerWattage = 500.0;
double acCompressorWattage = 1.0;
double draftInducerWattage = 10.0;

unsigned long lastPrimaryFanWattageCheck = 0;
unsigned long lastLivingRoomFanWattageCheck = 0;
unsigned long lastAirConditionerWattageCheck = 0;
unsigned long lastACCompressorWattageCheck = 0;
unsigned long lastDraftInducerWattageCheck = 0;

unsigned long lastOfficeBeep = 0;
unsigned long lastLivingRoomBeep = 0;
unsigned long lastAirConditionerBeep = 0;
unsigned long lastACCompressorBeep = 0;
unsigned long lastDraftInducerBeep = 0;
unsigned long lastKitchenVentBeep = 0;
unsigned long lastDiningRoomVentBeep = 0;

unsigned long lastAirConditionerPowerOn = 0;

bool alarmBool = false;

int outsideTemperatureCalibrationSize[2] = {432, 511};

double totalWattageSincePowerOn = 0;

double officeTemperature = 0;
double intakeTemperature = 0;
double livingRoomTemperature = 0;
double kitchenTemperature = 20.0;
double diningRoomTemperature = 20.0;

double outsideWindGusts = 0.0;
double outsideWindChill = 20.0;

unsigned long lastKitchenTemperatureUpdate = 0;
unsigned long lastDiningRoomTemperatureUpdate = 0;
unsigned long lastWindChillTemperatureUpdate = 0;
unsigned long lastWindGustsUpdate = 0;

double airConditionerMoistureVoltage = 0;
double airConditionerHumidity = 50;
double sunroomHumidityVoltage = 0;
double sunroomHumidity = 50;

double outsideTemperatureVoltage = 0;
double outsideTemperature = 0;
double outsideTemperatureWork = 0;
double outsideTemperatureWork2 = 0;

double acCompressorPowerOnSecond = 0;
unsigned long lastCompressorReset = 0;

unsigned long dataTransferTic = 0;
unsigned long systemControlTic = 0;

class motorControl {

  public:

    motorControl() {}

    motorControl(const int &onTime, const int &offTime, const int &onPin, const int &offPin, const int &eepromIndex) {
      this->onTime = onTime;
      this->offTime = offTime;

      this->onPin = onPin;
      this->offPin = offPin;
      pinMode(onPin, OUTPUT);
      pinMode(offPin, OUTPUT);

      this->eepromIndex = eepromIndex;
      this->isOpen = EEPROM.read(eepromIndex);
    }

    void open() {
      if (!this->isOpen) {
        digitalWrite(this->onPin, HIGH);
        delay(this->onTime);
        digitalWrite(this->onPin, LOW);
      }
      this->isOpen = true;
      EEPROM.update(this->eepromIndex, 1);
    }

    void close() {
      if (this->isOpen) {
        digitalWrite(this->offPin, HIGH);
        delay(this->offTime);
        digitalWrite(this->offPin, LOW);
      }
      this->isOpen = false;
      EEPROM.update(this->eepromIndex, 0);
    }

  private:
    int onTime;
    int offTime;

    int onPin;
    int offPin;

    int eepromIndex;
    bool isOpen;
};

motorControl flueDamper;

void setup() {

  temperatureMode = EEPROM.read(0);
  livingRoomFanIsOn = EEPROM.read(1);

  pinMode(primaryFan, OUTPUT);
  pinMode(livingRoomFan, OUTPUT);
  pinMode(airConditioner, OUTPUT);
  pinMode(acCompressor, OUTPUT);
  pinMode(draftInducer, OUTPUT);
  pinMode(speakerPin, OUTPUT);

  pinMode(buttonPin, INPUT);

  Serial.begin(115200);

  flueDamper =  motorControl(350, 3000, 11, 10, 2);

  flueDamper.close();

  tone(speakerPin, 440, 800);
  delay(800);
  noTone(speakerPin);
  tone(speakerPin, 587, 140);
  delay(140);
  noTone(speakerPin);
  delay(330);
  tone(speakerPin, 147, 200);
  delay(200);
  noTone(speakerPin);
}

// For controlling onboard devices
class mode {
  public:
    void setMode(int type, bool state) {
      if ((modeList[type - 2] != state  ) || (!modeSetup[type - 2])) {
        digitalWrite(type, !state);
        modeList[type - 2] = state;
        modeSetup[type - 2] = true;
      }
    }

    bool getMode(int type) {
      return modeList[type - 2];
    }
};

mode devices;

String readString = "";

// grabs a substring at a delimiter location
bool getStringToken(String &from, String &to, uint8_t index, char separator)
{
  uint16_t start = 0, idx = 0;
  uint8_t cur = 0;
  while (idx < from.length())
  {
    if (from.charAt(idx) == separator)
    {
      if (cur == index)
      {
        to = from.substring(start, idx);
        return true;
      }
      cur++;
      while ((idx < from.length() - 1) && (from.charAt(idx + 1) == separator)) idx++;
      start = idx + 1;
    }
    idx++;
  }
  if ((cur == index) && (start < from.length()))
  {
    to = from.substring(start, from.length());
    return true;
  }
  return false;
}

// Splits a string and returns the value of the split string at an index
String splitStringGetIndex(String tokens, char seperator, int intf) {
  uint8_t token_idx = 0;
  String token;

  while (getStringToken(tokens, token, token_idx, seperator))
  {
    if (token_idx == intf) {
      return token;
    }
    token_idx++;
  }
  return "";
}

// PC COM Link Handler
void commandHandler() {
  if (Serial.available())    {
    char c = Serial.read();    //gets one byte from serial buffer
    if (c == ',') {

      if (readString.length() > 1) {
        Serial.println("");
        // Serial.print(readString); //prints string to serial port out
        if (readString == F("\nhelp")) {
          Serial.println(F(""));
          Serial.println(F("Help Menu"));
          Serial.println(F("---------"));
          Serial.println(F("set_temperature {sensor} {value}"));
          Serial.println(F(" -----> - <device_name> <float>"));
          Serial.println(F(""));
          Serial.println(F("Include EOL char \",\" at end of each command"));
        }

        if (splitStringGetIndex(readString, ' ', 0) == F("\nset_temperature")) {
          if (splitStringGetIndex(readString, ' ', 1) == F("kitchen")) {
            kitchenTemperature = splitStringGetIndex(readString, ' ', 2).toFloat();
            lastKitchenTemperatureUpdate = millis();
          }
        }

        if (splitStringGetIndex(readString, ' ', 0) == F("\nset_temperature")) {
          if (splitStringGetIndex(readString, ' ', 1) == F("diningRoom")) {
            diningRoomTemperature = splitStringGetIndex(readString, ' ', 2).toFloat();
            lastDiningRoomTemperatureUpdate = millis();
          }
        }

        if (splitStringGetIndex(readString, ' ', 0) == F("\nset_temperature")) {
          if (splitStringGetIndex(readString, ' ', 1) == F("windChill")) {
            outsideWindChill = splitStringGetIndex(readString, ' ', 2).toFloat();
            lastWindChillTemperatureUpdate = millis();
          }
        }

        if (splitStringGetIndex(readString, ' ', 0) == F("\nset_wind")) {
          if (splitStringGetIndex(readString, ' ', 1) == F("gusts")) {
            outsideWindGusts = splitStringGetIndex(readString, ' ', 2).toFloat();
            lastWindGustsUpdate = millis();
          }
        }

        if (splitStringGetIndex(readString, ' ', 0) == F("\nmodify_temperature")) {
          if (splitStringGetIndex(readString, ' ', 1) == F("outside")) {
            outsideTemperature = splitStringGetIndex(readString, ' ', 2).toFloat();
            // if (millis() > 240000) {
              // double _actualOutsideTemperature = splitStringGetIndex(readString, ' ', 2).toFloat();
              // int _index = floor(outsideTemperatureWork2) + 40 + outsideTemperatureCalibrationSize[0];
              // if (_index > outsideTemperatureCalibrationSize[1]) {
              //   _index = outsideTemperatureCalibrationSize[1];
              // }
              // double oldValue = EEPROM.read(_index);
              // double _currentCalibrationValue = ((oldValue * 10) + (((_actualOutsideTemperature - outsideTemperatureWork2) * 3) + 127)) / 11;
              // EEPROM.update(_index, (int)_currentCalibrationValue);
            // }
          }
        }

        if (splitStringGetIndex(readString, ' ', 0) == F("\nget_eeprom")) {
          int _index = (int)(splitStringGetIndex(readString, ' ', 1).toFloat());
          Serial.println(EEPROM.read(_index));
        }

        if (splitStringGetIndex(readString, ' ', 0) == F("\nmodify_temperature")) {
          if (splitStringGetIndex(readString, ' ', 1) == F("reset_outside")) {
            int _i = outsideTemperatureCalibrationSize[0];
            while (_i < outsideTemperatureCalibrationSize[1]) {
              EEPROM.update(_i, 127);
              _i = _i + 1;
            }
          }
        }

        // Serial.println(','); //prints delimiting ","
        //do stuff with the captured readString
        readString = ""; //clears variable for new input
      }
    }
    else {
      readString += c; //makes the string readString
    }
  }
}

void buttonHandler() {
  if (!digitalRead(buttonPin)) {
    if (!buttonPressed) {
      buttonPressed = true;
      if (forceDraftInducer < millis()) {
        tone(speakerPin, 1318, 100);
        delay(100);
        noTone(speakerPin);
        tone(speakerPin, 2637, 100);
        delay(100);
        noTone(speakerPin);

        forceDraftInducer = millis() + 900000;
      } else {
        tone(speakerPin, 2637, 100);
        delay(100);
        noTone(speakerPin);
        tone(speakerPin, 1318, 100);
        delay(100);
        noTone(speakerPin);

        forceDraftInducer = millis();
      }
    }
  } else {
    buttonPressed = false;
  }
  if (forceDraftInducer > millis()) {
    draftInducerOverride = true;
  } else {
    if (draftInducerOverride) {

      tone(speakerPin, 2637, 100);
      delay(100);
      noTone(speakerPin);
      tone(speakerPin, 1318, 100);
      delay(100);
      noTone(speakerPin);

      draftInducerOverride = false;
    }
  }
}

void runHeatingMode() {
  // Draft Inducer

  double addLogModifier = -outsideWindChill;

  if (((intakeTemperature > 20) && (((intakeTemperature < (55 + addLogModifier)) and (devices.getMode(primaryFan))) or ((intakeTemperature < (50 + addLogModifier)) and (!devices.getMode(primaryFan))))) or (forceDraftInducer > millis())) {
    if (!devices.getMode(draftInducer)) {
      tone(speakerPin, 754, 670);
      delay(670);
      noTone(speakerPin);
      delay(150);
      tone(speakerPin, 1309, 380);
      delay(380);
      noTone(speakerPin);
      delay(70);
      tone(speakerPin, 940, 875);
      delay(831);
      noTone(speakerPin);
    }

    flueDamper.open();
    devices.setMode(draftInducer, HIGH);

    if (lastDraftInducerWattageCheck > micros()) {
      lastDraftInducerWattageCheck = micros();
    }

    totalWattageSincePowerOn = totalWattageSincePowerOn + (draftInducerWattage * ((micros() - lastDraftInducerWattageCheck) / (3600.0 * 1000000.0)));
    lastDraftInducerWattageCheck = micros();
  } else {

    if ((((millis() > ((pow((-(intakeTemperature) + (20)), 2.5) * 100) + lastDraftInducerBeep))) && ((pow((-(intakeTemperature) + (20)), 2.5) * 100) > 10)) || (((millis() > ((pow((-(30) + (intakeTemperature)), 2.5) * 100) + lastDraftInducerBeep))) && ((pow((-(30) + (intakeTemperature)), 2.5) * 100) > 10))) {
      tone(speakerPin, 990, 10);
      delay(10);
      noTone(speakerPin);
      lastDraftInducerBeep = millis();
    }

    if (devices.getMode(draftInducer)) {
      tone(speakerPin, 1309, 680);
      delay(680);
      noTone(speakerPin);
      delay(60);
      tone(speakerPin, 940, 720);
      delay(720);
      noTone(speakerPin);
      delay(85);
      tone(speakerPin, 754, 675);
      delay(675);
      noTone(speakerPin);
    }

    if (!devices.getMode(primaryFan)) {
      if (outsideWindGusts < 12.0) {
        flueDamper.close();
      }
    }

    devices.setMode(draftInducer, LOW);
    lastDraftInducerWattageCheck = micros();

  }

  // Air Conditioner
  if ((intakeTemperature > 7) && (intakeTemperature < (officeTemperature + 6.5)) && (airConditionerHumidity < 21.0) && (livingRoomTemperature < 19.5) && (millis() > 120000)) {
    if (!devices.getMode(airConditioner)) {
      tone(speakerPin, 554, 670);
      delay(670);
      noTone(speakerPin);
      delay(150);
      tone(speakerPin, 1109, 380);
      delay(380);
      noTone(speakerPin);
      delay(70);
      tone(speakerPin, 740, 875);
      delay(831);
      noTone(speakerPin);
      lastAirConditionerPowerOn = millis() + 180000;
    }

    devices.setMode(airConditioner, HIGH);

    if (lastAirConditionerWattageCheck > micros()) {
      lastAirConditionerWattageCheck = micros();
    }

    totalWattageSincePowerOn = totalWattageSincePowerOn + (airConditionerWattage * ((micros() - lastAirConditionerWattageCheck) / (3600.0 * 1000000.0)));
    lastAirConditionerWattageCheck = micros();
  } else {

    if (lastAirConditionerPowerOn < millis()) {

      if ((((millis() > ((pow((-(intakeTemperature) + (5)), 2.5) * 100) + lastAirConditionerBeep))) && ((pow((-(intakeTemperature) + (5)), 2.5) * 100) > 10)) || (((millis() > ((pow((-(officeTemperature + 6.5) + (intakeTemperature)), 2.5) * 100) + lastAirConditionerBeep))) && ((pow((-(officeTemperature + 6.5) + (intakeTemperature)), 2.5) * 100) > 10))) {
        tone(speakerPin, 770, 10);
        delay(10);
        noTone(speakerPin);
        lastAirConditionerBeep = millis();
      }

      if (devices.getMode(airConditioner)) {
        tone(speakerPin, 1109, 680);
        delay(680);
        noTone(speakerPin);
        delay(60);
        tone(speakerPin, 740, 720);
        delay(720);
        noTone(speakerPin);
        delay(85);
        tone(speakerPin, 554, 675);
        delay(675);
        noTone(speakerPin);
      }

      devices.setMode(airConditioner, LOW);
      lastAirConditionerWattageCheck = micros();

    } else {
      if (lastAirConditionerWattageCheck > micros()) {
        lastAirConditionerWattageCheck = micros();
      }

      totalWattageSincePowerOn = totalWattageSincePowerOn + (airConditionerWattage * ((micros() - lastAirConditionerWattageCheck) / (3600.0 * 1000000.0)));
      lastAirConditionerWattageCheck = micros();
    }

  }

  // Kitchen Vent
  if (((lastKitchenTemperatureUpdate > 0) && (millis() < (lastKitchenTemperatureUpdate + 60000)) && (millis() > 60000)) && ((intakeTemperature > (kitchenTemperature + 6.5)) && ((kitchenTemperature < 19) or ((kitchenTemperature < 19.5) && (diningRoomTemperature > 19) && (officeTemperature > 19))))) {
    if (!kitchenVentIsOn) {
      tone(speakerPin, 449, 670);
      delay(670);
      noTone(speakerPin);
      delay(150);
      tone(speakerPin, 798, 380);
      delay(380);
      noTone(speakerPin);
      delay(70);
      tone(speakerPin, 623, 875);
      delay(875);
      noTone(speakerPin);
    }

    kitchenVentIsOn = true;
  } else {

    if (((millis() > ((pow((-(intakeTemperature) + (kitchenTemperature + 6.5)), 2.5) * 100) + lastKitchenVentBeep))) && ((pow((-(intakeTemperature) + (kitchenTemperature + 6.5)), 2.5) * 100) > 10)) {
      tone(speakerPin, 440, 10);
      delay(10);
      noTone(speakerPin);
      lastKitchenVentBeep = millis();
    }

    if (kitchenVentIsOn) {
      tone(speakerPin, 798, 680);
      delay(680);
      noTone(speakerPin);
      delay(60);
      tone(speakerPin, 623, 720);
      delay(720);
      noTone(speakerPin);
      delay(85);
      tone(speakerPin, 449, 675);
      delay(675);
      noTone(speakerPin);
    }

    kitchenVentIsOn = false;
  }

  // Dining Room Vent
  if (((lastDiningRoomTemperatureUpdate > 0) && (millis() < (lastDiningRoomTemperatureUpdate + 60000)) && (millis() > 60000)) && ((intakeTemperature > (diningRoomTemperature + 6.5)) && ((diningRoomTemperature < 19) or ((diningRoomTemperature < 19.5) && (kitchenTemperature > 19) && (officeTemperature > 19))))) {
    if (!diningRoomVentIsOn) {
      tone(speakerPin, 549, 670);
      delay(670);
      noTone(speakerPin);
      delay(150);
      tone(speakerPin, 898, 380);
      delay(380);
      noTone(speakerPin);
      delay(70);
      tone(speakerPin, 723, 875);
      delay(875);
      noTone(speakerPin);
    }

    diningRoomVentIsOn = true;
  } else {

    if (((millis() > ((pow((-(intakeTemperature) + (diningRoomTemperature + 6.5)), 2.5) * 100) + lastDiningRoomVentBeep))) && ((pow((-(intakeTemperature) + (diningRoomTemperature + 6.5)), 2.5) * 100) > 10)) {
      tone(speakerPin, 330, 10);
      delay(10);
      noTone(speakerPin);
      lastDiningRoomVentBeep = millis();
    }

    if (diningRoomVentIsOn) {
      tone(speakerPin, 898, 680);
      delay(680);
      noTone(speakerPin);
      delay(60);
      tone(speakerPin, 723, 720);
      delay(720);
      noTone(speakerPin);
      delay(85);
      tone(speakerPin, 549, 675);
      delay(675);
      noTone(speakerPin);
    }

    diningRoomVentIsOn = false;
  }

  // Primary Fan
  if (devices.getMode(livingRoomFan) or kitchenVentIsOn or diningRoomVentIsOn or ((intakeTemperature > (officeTemperature + 6.5)) && ((officeTemperature < 19) or ((officeTemperature < 19.5) && (diningRoomTemperature > 19) && (kitchenTemperature > 19)))) or (intakeTemperature > 102.0) or ((intakeTemperature < 4) && (millis() > 60000))) {

    if (!devices.getMode(primaryFan)) {
      tone(speakerPin, 349, 670);
      delay(670);
      noTone(speakerPin);
      delay(150);
      tone(speakerPin, 698, 380);
      delay(380);
      noTone(speakerPin);
      delay(70);
      tone(speakerPin, 523, 875);
      delay(875);
      noTone(speakerPin);
    }

    if (intakeTemperature < 85.0) {
      flueDamper.open();
    } else {
      if ((outsideWindGusts < 12.0) or ((outsideWindGusts > 12.0) && (intakeTemperature > 100))) {
        flueDamper.close();
      }
    }

    devices.setMode(primaryFan, HIGH);

    if (lastPrimaryFanWattageCheck > micros()) {
      lastPrimaryFanWattageCheck = micros();
    }

    totalWattageSincePowerOn = totalWattageSincePowerOn + (primaryFanWattage * ((micros() - lastPrimaryFanWattageCheck) / (3600.0 * 1000000.0)));
    lastPrimaryFanWattageCheck = micros();
  } else {

    if (((millis() > ((pow((-(intakeTemperature) + (officeTemperature + 6.5)), 2.5) * 100) + lastOfficeBeep))) && ((pow((-(intakeTemperature) + (officeTemperature + 6.5)), 2.5) * 100) > 10)) {
      tone(speakerPin, 550, 10);
      delay(10);
      noTone(speakerPin);
      lastOfficeBeep = millis();
    }

    if (devices.getMode(primaryFan)) {
      tone(speakerPin, 698, 680);
      delay(680);
      noTone(speakerPin);
      delay(60);
      tone(speakerPin, 523, 720);
      delay(720);
      noTone(speakerPin);
      delay(85);
      tone(speakerPin, 349, 675);
      delay(675);
      noTone(speakerPin);
    }

    if (!devices.getMode(draftInducer)) {
      if (outsideWindGusts < 12.0) {
        flueDamper.close();
      }
    }

    devices.setMode(primaryFan, LOW);
    lastPrimaryFanWattageCheck = micros();
  }

  // Living Room Fan
  if ((!livingRoomFanIsOn && (intakeTemperature > (livingRoomTemperature + 20))) || (livingRoomFanIsOn && (intakeTemperature > (livingRoomTemperature + 15)))) {

    livingRoomFanIsOn = true;
    EEPROM.update(1, true);

    if (!livingRoomFanIsOn) {
      tone(speakerPin, 220, 340);
      delay(340);
      noTone(speakerPin);
      delay(65);
      tone(speakerPin, 277, 310);
      delay(310);
      noTone(speakerPin);
      delay(75);
      tone(speakerPin, 330, 710);
      delay(710);
      noTone(speakerPin);
    }

    // living room temp control
    if ((livingRoomTemperature < 19.5) || ((livingRoomTemperature < 20.0) && ((diningRoomTemperature < 19) || (kitchenTemperature < 19)) && (intakeTemperature > (addLogModifier + 10)))) {

      if (!devices.getMode(livingRoomFan)) {
        tone(speakerPin, 220, 85);
        delay(85);
        noTone(speakerPin);
        delay(16);
        tone(speakerPin, 277, 77);
        delay(77);
        noTone(speakerPin);
        delay(19);
        tone(speakerPin, 330, 178);
        delay(178);
        noTone(speakerPin);
      }

      devices.setMode(livingRoomFan, HIGH);

      totalWattageSincePowerOn = totalWattageSincePowerOn + (livingRoomFanWattage * ((micros() - lastLivingRoomFanWattageCheck) / (3600.0 * 1000000.0)));
      lastLivingRoomFanWattageCheck = micros();

      if (lastLivingRoomFanWattageCheck > micros()) {
        lastLivingRoomFanWattageCheck = micros();
      }
    } else {
      if ((millis() > ((pow((-(livingRoomTemperature) + (19.5)), 2.5) * 100) + lastLivingRoomBeep)) && ((pow((-(livingRoomTemperature) + (19.5)), 2.5) * 100) > 10)) {
        tone(speakerPin, 660, 10);
        delay(10);
        noTone(speakerPin);
        lastLivingRoomBeep = millis();
      }

      if (devices.getMode(livingRoomFan)) {
        tone(speakerPin, 330, 85);
        delay(85);
        noTone(speakerPin);
        delay(65);
        tone(speakerPin, 277, 77);
        delay(77);
        noTone(speakerPin);
        delay(75);
        tone(speakerPin, 220, 178);
        delay(178);
        noTone(speakerPin);
      }

      devices.setMode(livingRoomFan, LOW);
      lastLivingRoomFanWattageCheck = micros();
    }
  } else {

    if (millis() > 60000) {
      livingRoomFanIsOn = false;
      EEPROM.update(1, false);
    }

    if ((millis() > ((pow((-(intakeTemperature) + (livingRoomTemperature + 20)), 2.5) * 100) + lastLivingRoomBeep)) && ((pow((-(intakeTemperature) + (livingRoomTemperature + 20)), 2.5) * 100) > 10)) {
      tone(speakerPin, 660, 10);
      delay(10);
      noTone(speakerPin);
      lastLivingRoomBeep = millis();
    }

    if (devices.getMode(livingRoomFan)) {
      tone(speakerPin, 330, 340);
      delay(340);
      noTone(speakerPin);
      delay(65);
      tone(speakerPin, 277, 310);
      delay(310);
      noTone(speakerPin);
      delay(75);
      tone(speakerPin, 220, 710);
      delay(710);
      noTone(speakerPin);
    }

    devices.setMode(livingRoomFan, LOW);
    lastLivingRoomFanWattageCheck = micros();
  }
}

void runCoolingMode() {
  // Draft Inducer
  if (((intakeTemperature > outsideTemperature) && (outsideTemperature < 20)) or (forceDraftInducer > millis())) {
    if (!devices.getMode(draftInducer)) {
      tone(speakerPin, 754, 670);
      delay(670);
      noTone(speakerPin);
      delay(150);
      tone(speakerPin, 1309, 380);
      delay(380);
      noTone(speakerPin);
      delay(70);
      tone(speakerPin, 940, 875);
      delay(831);
      noTone(speakerPin);
    }

    flueDamper.open();

    devices.setMode(draftInducer, HIGH);

    if (lastDraftInducerWattageCheck > micros()) {
      lastDraftInducerWattageCheck = micros();
    }

    totalWattageSincePowerOn = totalWattageSincePowerOn + (draftInducerWattage * ((micros() - lastDraftInducerWattageCheck) / (3600.0 * 1000000.0)));
    lastDraftInducerWattageCheck = micros();
  } else {

    if ((((millis() > ((pow((-(intakeTemperature) + (50)), 2.5) * 100) + lastDraftInducerBeep))) && ((pow((-(intakeTemperature) + (50)), 2.5) * 100) > 10)) || (((millis() > ((pow((-(outsideTemperature) + (intakeTemperature)), 2.5) * 100) + lastDraftInducerBeep))) && ((pow((-(outsideTemperature) + (intakeTemperature)), 2.5) * 100) > 10))) {
      tone(speakerPin, 990, 10);
      delay(10);
      noTone(speakerPin);
      lastDraftInducerBeep = millis();
    }

    if (devices.getMode(draftInducer)) {
      tone(speakerPin, 1309, 680);
      delay(680);
      noTone(speakerPin);
      delay(60);
      tone(speakerPin, 940, 720);
      delay(720);
      noTone(speakerPin);
      delay(85);
      tone(speakerPin, 754, 675);
      delay(675);
      noTone(speakerPin);
    }

    if (outsideWindGusts < 12.0) {
        flueDamper.close();
    }
    
    devices.setMode(draftInducer, LOW);
    lastDraftInducerWattageCheck = micros();

  }

  // Air Conditioner Shutoff
  if (devices.getMode(airConditioner)) {
    tone(speakerPin, 1109, 680);
    delay(680);
    noTone(speakerPin);
    delay(60);
    tone(speakerPin, 740, 720);
    delay(720);
    noTone(speakerPin);
    delay(85);
    tone(speakerPin, 554, 675);
    delay(675);
    noTone(speakerPin);
  }

  devices.setMode(airConditioner, LOW);
  lastAirConditionerWattageCheck = micros();

  // Kitchen Vent
  if (((lastKitchenTemperatureUpdate > 0) && (millis() < (lastKitchenTemperatureUpdate + 60000)) && (millis() > 60000)) && ((intakeTemperature < (kitchenTemperature - 4.5)) && (kitchenTemperature > 19.5))) {
    if (!kitchenVentIsOn) {
      tone(speakerPin, 449, 670);
      delay(670);
      noTone(speakerPin);
      delay(150);
      tone(speakerPin, 798, 380);
      delay(380);
      noTone(speakerPin);
      delay(70);
      tone(speakerPin, 623, 875);
      delay(875);
      noTone(speakerPin);
    }

    kitchenVentIsOn = true;
  } else {
    if (kitchenVentIsOn) {
      tone(speakerPin, 798, 680);
      delay(680);
      noTone(speakerPin);
      delay(60);
      tone(speakerPin, 623, 720);
      delay(720);
      noTone(speakerPin);
      delay(85);
      tone(speakerPin, 449, 675);
      delay(675);
      noTone(speakerPin);
    }

    kitchenVentIsOn = false;
  }

  // Dining Room Vent
  if (((lastDiningRoomTemperatureUpdate > 0) && (millis() < (lastDiningRoomTemperatureUpdate + 60000)) && (millis() > 60000)) && ((intakeTemperature < (diningRoomTemperature - 5.5)) && (diningRoomTemperature > 20.5))) {
    if (!diningRoomVentIsOn) {
      tone(speakerPin, 549, 670);
      delay(670);
      noTone(speakerPin);
      delay(150);
      tone(speakerPin, 898, 380);
      delay(380);
      noTone(speakerPin);
      delay(70);
      tone(speakerPin, 723, 875);
      delay(875);
      noTone(speakerPin);
    }

    diningRoomVentIsOn = true;
  } else {
    if (diningRoomVentIsOn) {
      tone(speakerPin, 898, 680);
      delay(680);
      noTone(speakerPin);
      delay(60);
      tone(speakerPin, 723, 720);
      delay(720);
      noTone(speakerPin);
      delay(85);
      tone(speakerPin, 549, 675);
      delay(675);
      noTone(speakerPin);
    }

    diningRoomVentIsOn = false;
  }

  // Living Room Fan
  if (((intakeTemperature < (livingRoomTemperature - 3)) && (livingRoomTemperature > 18.5))) {
    if (!devices.getMode(livingRoomFan)) {
      tone(speakerPin, 220, 340);
      delay(340);
      noTone(speakerPin);
      delay(65);
      tone(speakerPin, 277, 310);
      delay(310);
      noTone(speakerPin);
      delay(75);
      tone(speakerPin, 330, 710);
      delay(710);
      noTone(speakerPin);
    }

    devices.setMode(livingRoomFan, HIGH);

    if (lastLivingRoomFanWattageCheck > micros()) {
      lastLivingRoomFanWattageCheck = micros();
    }

    totalWattageSincePowerOn = totalWattageSincePowerOn + (livingRoomFanWattage * ((micros() - lastLivingRoomFanWattageCheck) / (3600.0 * 1000000.0)));
    lastLivingRoomFanWattageCheck = micros();
  } else {
    if (devices.getMode(livingRoomFan)) {
      tone(speakerPin, 330, 340);
      delay(340);
      noTone(speakerPin);
      delay(65);
      tone(speakerPin, 277, 310);
      delay(310);
      noTone(speakerPin);
      delay(75);
      tone(speakerPin, 220, 710);
      delay(710);
      noTone(speakerPin);
    }

    devices.setMode(livingRoomFan, LOW);
    lastLivingRoomFanWattageCheck = micros();
  }

  // Primary Fan
  if (devices.getMode(livingRoomFan) or kitchenVentIsOn or diningRoomVentIsOn or (intakeTemperature > 65.0) or ((intakeTemperature < 4) && (millis() > 60000)) or ((intakeTemperature < (officeTemperature - 3)) && ((officeTemperature > 17))) or ((sunroomHumidity > 80) && (officeTemperature > 19.6))) { //  or ((outsideTemperature < 22) && (officeTemperature < livingRoomTemperature) && (livingRoomTemperature > (intakeTemperature - 3)))) {

    if (!devices.getMode(primaryFan) and (sunroomHumidity > 81)) {
      tone(speakerPin, 349, 670);
      delay(670);
      noTone(speakerPin);
      delay(150);
      tone(speakerPin, 698, 380);
      delay(380);
      noTone(speakerPin);
      delay(70);
      tone(speakerPin, 523, 875);
      delay(875);
      noTone(speakerPin);
    }

    devices.setMode(primaryFan, HIGH);

    if (lastPrimaryFanWattageCheck > micros()) {
      lastPrimaryFanWattageCheck = micros();
    }

    totalWattageSincePowerOn = totalWattageSincePowerOn + (primaryFanWattage * ((micros() - lastPrimaryFanWattageCheck) / (3600.0 * 1000000.0)));
    lastPrimaryFanWattageCheck = micros();

  } else {

    if (((millis() > ((pow((-(intakeTemperature) + (livingRoomTemperature - 1)), 2.5) * 100) + lastOfficeBeep))) && ((pow((-(intakeTemperature) + (livingRoomTemperature - 1)), 2.5) * 100) > 10)) {
      tone(speakerPin, 550, 10);
      delay(10);
      noTone(speakerPin);
      lastOfficeBeep = millis();
    }

    if (devices.getMode(primaryFan)) {
      tone(speakerPin, 698, 680);
      delay(680);
      noTone(speakerPin);
      delay(60);
      tone(speakerPin, 523, 720);
      delay(720);
      noTone(speakerPin);
      delay(85);
      tone(speakerPin, 349, 675);
      delay(675);
      noTone(speakerPin);
    }

    devices.setMode(primaryFan, LOW);
    lastPrimaryFanWattageCheck = micros();
  }
}

void loop() {

  // Grab Sensor Data
  double referenceVoltage = (((((5 * ((double)(analogRead(A0) * 2) / 1024.0)) / 5) - 1) * 2) + 1) * 5;

  double modifier = ((referenceVoltage / 5) - 0.984) * 62.5;

  double intakeVoltage = (referenceVoltage * ((double)analogRead(A1) / 1024.0));
  double intakeResistance = (((referenceVoltage * 10000) - (10000 * intakeVoltage)) / intakeVoltage) / 1000;
  intakeTemperature = (((intakeTemperature * temperatureSmoothingConstant) + (((log(intakeResistance + 7.0772) / (0.00343906 * log(3.09547))) - 706.407) + (2.32 * modifier))) / (temperatureSmoothingConstant + 1)); // 0.000 =  8.318

  double officeVoltage = (referenceVoltage * ((double)analogRead(A2) / 1024.0));
  double officeResistance = (((referenceVoltage * 10000) - (10000 * officeVoltage)) / officeVoltage) / 1000;
  officeTemperature = (((officeTemperature * temperatureSmoothingConstant) + (((log(officeResistance + 7.0772) / (0.00343906 * log(3.09547))) - 706.407) + (2.32 * modifier) + 1.5)) / (temperatureSmoothingConstant + 1)); // 0.000 =  7.921

  double livingRoomVoltage = (referenceVoltage * ((double)analogRead(A4) / 1024.0));
  double livingRoomResistance = (((referenceVoltage * 10000) - (10000 * livingRoomVoltage)) / livingRoomVoltage) / 1000;
  livingRoomTemperature = (((livingRoomTemperature * temperatureSmoothingConstant) + (((log(livingRoomResistance + 7.0772) / (0.00343906 * log(3.09547))) - 706.407) + (2.3745376344086018 * modifier))) / (temperatureSmoothingConstant + 1)); // 0.000 = 8.715

  airConditionerMoistureVoltage = (referenceVoltage * ((double)analogRead(A5) / 1024.0));
  airConditionerHumidity = ((airConditionerHumidity * temperatureSmoothingConstant) + (((-250.0 * (airConditionerMoistureVoltage)) + 730) + (modifier))) / (temperatureSmoothingConstant + 1);

  sunroomHumidityVoltage = (referenceVoltage * ((double)analogRead(A6) / 1024.0)) - ((referenceVoltage - 4.98) / (5.0 / 3.3));
  sunroomHumidity = ((sunroomHumidity * temperatureSmoothingConstant) + ((-330.669 * (sunroomHumidityVoltage)) + 1179.24 + (modifier))) / (temperatureSmoothingConstant + 1);

  // https://www.desmos.com/calculator/exav5pwzwt
  // outsideTemperatureVoltage = (referenceVoltage * ((double)analogRead(A7) / 1024.0));
  // outsideTemperatureWork = ((outsideTemperatureWork * temperatureSmoothingConstant) + (((-pow(4.38608, (0.0178602 * (outsideTemperatureVoltage + 364.546))) + 15244.2) - 15) + 7)) / (temperatureSmoothingConstant + 1);
  // outsideTemperatureWork2 = -pow(0.953274, (5.74444 * outsideTemperatureWork - 81.3149)) + 22.1756;
  // int _index = floor(outsideTemperatureWork2) + 40 + outsideTemperatureCalibrationSize[0];
  // if (_index > outsideTemperatureCalibrationSize[1]) {
  //   _index = outsideTemperatureCalibrationSize[1];
  // }
  // outsideTemperature = outsideTemperatureWork2 + (((((double)EEPROM.read(_index) - 127.0) / 3.0) * (1 - (outsideTemperatureWork2 - floor(outsideTemperatureWork2)))) + ((((double)EEPROM.read(_index + 1) - 127.0) / 3.0) * (outsideTemperatureWork2 - floor(outsideTemperatureWork2))));

  if (officeTemperature > 999) {
    officeTemperature = 999;
  }

  // PC COM Link
  commandHandler();

  // Button Handler
  buttonHandler();

  // Grab temperature type
  if (millis() > 120000) {
    if ((outsideTemperature < 8) or (intakeTemperature > 42.5)) {
      if (!temperatureMode) {
        tone(speakerPin, 466, 400);
        delay(400);
        noTone(speakerPin);
        delay(100);
        tone(speakerPin, 440, 400);
        delay(400);
        noTone(speakerPin);
        delay(100);
        tone(speakerPin, 466, 400);
        delay(400);
        noTone(speakerPin);
        delay(100);
        tone(speakerPin, 392, 400);
        delay(400);
        noTone(speakerPin);
        temperatureMode = true;
        EEPROM.update(0, 1);
      }
    } else if (outsideTemperature > 14) {
      if (temperatureMode) {
        tone(speakerPin, 311, 400);
        delay(400);
        noTone(speakerPin);
        delay(100);
        tone(speakerPin, 349, 400);
        delay(400);
        noTone(speakerPin);
        delay(100);
        tone(speakerPin, 370, 400);
        delay(400);
        noTone(speakerPin);
        delay(100);
        tone(speakerPin, 392, 400);
        delay(400);
        noTone(speakerPin);
        temperatureMode = false;
        EEPROM.update(0, 0);
      }
    }
  }

  // HVAC Control
  if (systemControlTic < millis()) {
    if (temperatureMode) {
      runHeatingMode();
    } else {
      runCoolingMode();
    }
    systemControlTic = millis() + 100;
  }

  // Transfer data to PC
  if (dataTransferTic < millis()) {

    // Overfire and Lightstove alarms
    if (!alarmBool) {
      alarmBool = true;

      if (((outsideWindGusts < 12.0) && (intakeTemperature > 85)) or ((outsideWindGusts > 12.0) && (intakeTemperature > 100))) {
        tone(speakerPin, 2000, 1000);
        if (!devices.getMode(draftInducer)) {
          flueDamper.close();
        }
      } else if (intakeTemperature > 75) {
        tone(speakerPin, 2000, 400);
      } else if (intakeTemperature > 65) {
        tone(speakerPin, 2000, 200);
      } else if (intakeTemperature > 55) {
        tone(speakerPin, 2000, 100);
      } else if (intakeTemperature < 4.5) {
        tone(speakerPin, 1000, 100);
        if (!devices.getMode(draftInducer) && !devices.getMode(primaryFan)) {
          flueDamper.close();
        }
      } else if (temperatureMode && (intakeTemperature < 15)) {
        tone(speakerPin, 100, 10);
        if (!devices.getMode(draftInducer) && !devices.getMode(primaryFan)) {
          flueDamper.close();
        }
      }
    } else {
      noTone(speakerPin);
      alarmBool = false;
    }

    // Com Data
    Serial.println("");
    Serial.print("{\"sensorInfo\":{\"airIntakeTemperature\":");
    Serial.print(String(intakeTemperature, 3));
    Serial.print(",\"livingRoomTemperature\":");
    Serial.print(String(livingRoomTemperature, 3));
    Serial.print(",\"officeTemperature\":");
    Serial.print(String(officeTemperature, 3));
    Serial.print(",\"airConditionerHumidity\":");
    Serial.print(String(airConditionerHumidity, 3));
    Serial.print(",\"sunroomHumidity\":");
    Serial.print(String(sunroomHumidity, 3));
    Serial.print(",\"kitchenTemperature\":");
    Serial.print(String(kitchenTemperature, 3));
    Serial.print(",\"diningRoomTemperature\":");
    Serial.print(String(diningRoomTemperature, 3));
    Serial.print(",\"outsideTemperature\":");
    Serial.print(String(outsideTemperature, 3));
    Serial.print("},\"voltages\":[");
    Serial.print(referenceVoltage);
    Serial.print(",");
    Serial.print(intakeVoltage);
    Serial.print(",");
    Serial.print(livingRoomVoltage);
    Serial.print(",");
    Serial.print(officeVoltage);
    Serial.print(",");
    Serial.print(airConditionerMoistureVoltage);
    Serial.print(",");
    Serial.print(sunroomHumidityVoltage);
    Serial.print(",");
    Serial.print(outsideTemperatureVoltage);
    Serial.print("],\"deviceStatus\":{\"airConditioner\":");
    Serial.print(devices.getMode(airConditioner));
    Serial.print(",\"acCompressor\":");
    Serial.print(devices.getMode(acCompressor));
    Serial.print(",\"primaryFan\":");
    Serial.print(devices.getMode(primaryFan));
    Serial.print(",\"livingRoomFan\":");
    Serial.print(devices.getMode(livingRoomFan));
    Serial.print(",\"draftInducer\":");
    Serial.print(devices.getMode(draftInducer));
    Serial.print(",\"kitchenVent\":");
    Serial.print(kitchenVentIsOn);
    Serial.print(",\"diningRoomVent\":");
    Serial.print(diningRoomVentIsOn);
    Serial.print("},\"wattageUsedSincePowerOn\":");
    Serial.print(totalWattageSincePowerOn);
    Serial.print(",\"heatingMode\":");
    Serial.print(temperatureMode);
    Serial.print(",\"windGusts\":");
    Serial.print(outsideWindGusts);
    Serial.print("}");

    dataTransferTic = millis() + 1000;
  }
}
