#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <RadioLib.h>
#include "TinyGPSPlus.h"
#include <Wire.h>
#include <QMC5883L.h>
#include "mbedtls/aes.h"
#include "crypto.h"
#include "iostream"
#include "sstream"
#include "string"

#define QMC5883P_ADDR 0x2C 

#define COMPASS_SDA 17
#define COMPASS_SCL 18

#define LORA_NSS   8
#define LORA_SCK   9
#define LORA_MOSI  10
#define LORA_MISO  11
#define LORA_RST   12
#define LORA_BUSY  13
#define LORA_DIO1  14

//reading battery voltage
#define VBAT_READ  1   //ADC1_CH0, reads the battery voltage
#define ADC_CTRL   37  //Enables the voltage-divider circuit before reading


#define GNSS_RX 33   //MCU receives GPS data here
#define GNSS_TX 34   //MCU sends commands to GPS here (rarely used)

constexpr uint8_t VEXT_CTRL = 3;
constexpr uint8_t TFT_BL    = 21;
constexpr uint8_t TFT_CS    = 38;
constexpr uint8_t TFT_DC    = 40;
constexpr uint8_t TFT_RST   = 39;
constexpr uint8_t TFT_MOSI  = 42;
constexpr uint8_t TFT_SCLK  = 41;

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
HardwareSerial gpsSerial(1);
TinyGPSPlus gps;

//compass object
QMC5883L compass;


//screen meassurements
int SCREEN_WIDTH = 160;
int SCREEN_HEIGHT = 80;


//each board name
#define DEVICE_NAME "NodeB"

volatile bool receivedFlag = false;
void onReceive() { receivedFlag = true; }

unsigned long lastSend = 0;
unsigned long lastReceivedMillis = 0;
unsigned long down_time = 0;
const unsigned long sendInterval = 5000; //time between each transmission (in milliseconds)
String lastMsg = "waiting...";
const unsigned long calibration_time = 30000;

bool calibrateCompass();



void qmcInit() {
  Wire.beginTransmission(QMC5883P_ADDR);
  Wire.write(0x0B);  // config register
  Wire.write(0x08);  // set mode continuous
  Wire.endTransmission();

  Wire.beginTransmission(QMC5883P_ADDR);
  Wire.write(0x29);
  Wire.write(0x06);
  Wire.endTransmission();

  Wire.beginTransmission(QMC5883P_ADDR);
  Wire.write(0x0A);
  Wire.write(0xC3); //continuous mode, ODR = 10Hz
  Wire.endTransmission();
}



void setup() {
  //for random to be actually random 
  randomSeed(esp_random());
  
  Serial.begin(115200);
  delay(1000);
  
  //Power up display
  pinMode(VEXT_CTRL, OUTPUT);
  digitalWrite(VEXT_CTRL, HIGH);
  delay(100);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  delay(100);

  gpsSerial.begin(115200, SERIAL_8N1, GNSS_RX, GNSS_TX);



  Wire.begin(COMPASS_SDA, COMPASS_SCL);
  qmcInit();

  Serial.println("QMC5883P initialized");

  Serial.println("Calibrating compass for 30 seconds");
  unsigned long calibrationStart = millis();
  while (millis() - calibrationStart < calibration_time) {
    calibrateCompass();
    delay(100);
  }
  Serial.println("Compass calibration complete");


  Serial.println("Initializing TFT");
  tft.initR(INITR_MINI160x80_PLUGIN);
  tft.invertDisplay(false);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(5, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.println(DEVICE_NAME);

  //Radio ---
  Serial.println("Initializing radio");
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int state = radio.begin(915.0);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Radio init failed, code: ");
    Serial.println(state);
    tft.setCursor(5, 20);
    tft.setTextColor(ST77XX_RED);
    tft.println("Radio FAIL");
    while (true) delay(1000);
  }

  radio.setDio1Action(onReceive);
  radio.startReceive();

  tft.setCursor(5, 20);
  tft.setTextColor(ST77XX_GREEN);
  tft.println("Radio OK");

  // Run once in setup(), after your Wire/crypto init:
  uint8_t testNonce[16];
  generateNonce(testNonce);
  uint8_t testBuf[32];
  String testPayload = "TestNode:43.123456:-79.123456";
  uint8_t nonceForEncrypt[16];
  uint8_t nonceForDecrypt[16];

  memcpy(nonceForEncrypt, testNonce, 16);
  memcpy(nonceForDecrypt, testNonce, 16);


  memcpy(testBuf, testPayload.c_str(), testPayload.length());
  
  aesCtrCrypt(testBuf, testPayload.length(), nonceForEncrypt);
  aesCtrCrypt(testBuf, testPayload.length(), nonceForDecrypt);
  
  testBuf[testPayload.length()] = '\0';
  Serial.print("Round-trip result: ");
  Serial.println(String((char*)testBuf));

}

//other player infos
struct Player {
  String name;
  double lat;
  double lng;
};

void PlayerID() {
}

Player handleReceivedData(const String& str) {
  Player player;
  player.name = "Unknown";
  player.lat = 0.0;
  player.lng = 0.0;

  int firstColon = str.indexOf(':');
  if (firstColon < 0) {
    return player;
  }

  player.name = str.substring(0, firstColon);

  int secondColon = str.indexOf(':', firstColon + 1);
  String latStr = (secondColon > firstColon)
      ? str.substring(firstColon + 1, secondColon)
      : str.substring(firstColon + 1);
  player.lat = latStr.toFloat();

  if (secondColon > firstColon) {
    player.lng = str.substring(secondColon + 1).toFloat();
  }

  return player;
}



char hexDigit(uint8_t value) {
  return value < 10 ? ('0' + value) : ('A' + value - 10);
}

String encodeHex(const uint8_t* data, size_t len) {
  String encoded;
  encoded.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    encoded += hexDigit(data[i] >> 4);
    encoded += hexDigit(data[i] & 0x0F);
  }
  return encoded;
}

int8_t hexValue(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

bool decodeHex(const String& encoded, uint8_t* data, size_t capacity, size_t& len) {
  if ((encoded.length() % 2) != 0 || encoded.length() / 2 > capacity) {
    return false;
  }

  len = encoded.length() / 2;
  for (size_t i = 0; i < len; i++) {
    int8_t high = hexValue(encoded[i * 2]);
    int8_t low = hexValue(encoded[i * 2 + 1]);
    if (high < 0 || low < 0) return false;
    data[i] = (high << 4) | low;
  }
  return true;
}

bool isValidPayload(const String& payload) {
  int firstColon = payload.indexOf(':');
  int secondColon = payload.indexOf(':', firstColon + 1);
  return firstColon > 0
      && secondColon > firstColon + 1
      && secondColon < static_cast<int>(payload.length()) - 1
      && payload.indexOf(':', secondColon + 1) < 0;
}

void arrowDraw(double myLat, double otherLat, double myLng, double otherLng, bool error) {
  const int16_t x = 90;
  const int16_t y = 40;
  const uint16_t w = 70;
  const uint16_t h = 16;

  tft.fillRect(x, y, w, h, ST77XX_BLACK);
  tft.setCursor(x, y);
  tft.setTextColor(ST77XX_WHITE);

  if (!gps.location.isValid()) {
    tft.print("No fix");
    return;
  }

  if(error) {
    tft.print("Not Found");
    return;
  }

  double latDiff = otherLat - myLat;
  double lngDiff = otherLng - myLng;
  if (latDiff > 0.000005) { //was 0.0005
    if(lngDiff > 0.000005) {
      tft.print("NE");
    } else {
      tft.print("NW");
    }
  } else if (latDiff < -0.000005) {
    if(lngDiff > 0.000005) {
      tft.print("SE");
    } else {
      tft.print("SW");
    }
  } else {
    if(lngDiff > 0.000005) {
      tft.print("E");
    } else if (lngDiff < -0.000005) {
      tft.print("W");
    } else {
    tft.print("Same");
    }
  }
}


int16_t xMin = 32767, xMax = -32768;
int16_t yMin = 32767, yMax = -32768;

int16_t x_offset = 0, y_offset = 0;

bool calibrateCompass(){
  Wire.beginTransmission(QMC5883P_ADDR);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.requestFrom(QMC5883P_ADDR, 6);
  if (Wire.available() < 6) return false;

  int16_t x = Wire.read() | (Wire.read() << 8);
  int16_t y = Wire.read() | (Wire.read() << 8);
  Wire.read(); Wire.read(); //discard Z

  if (x < xMin) xMin = x;
  if (x > xMax) xMax = x;
  if (y < yMin) yMin = y;
  if (y > yMax) yMax = y;

  Serial.print("xMin: "); Serial.print(xMin);
  Serial.print(" xMax: "); Serial.print(xMax);
  Serial.print(" yMin: "); Serial.print(yMin);
  Serial.print(" yMax: "); Serial.println(yMax);
  return true;
}

//just NSEW on X axis
float qmcReadHeadingCardinal(){
  Wire.beginTransmission(QMC5883P_ADDR);
  Wire.write(0x01);
  Wire.endTransmission();

  Wire.requestFrom(QMC5883P_ADDR, 6);
  if (Wire.available() < 6) return -1;

  int16_t x = Wire.read() | (Wire.read() << 8);
  int16_t y = Wire.read() | (Wire.read() << 8);
  int16_t z = Wire.read() | (Wire.read() << 8);

  x_offset = (xMax + xMin) / 2;
  y_offset = (yMax + yMin) / 2;


  float heading = atan2((float)y - y_offset, (float)x - x_offset) * 180.0 / PI;
  if (heading < 0) heading += 360;

  return heading; 
}

int16_t radar_size = 6;
int16_t arrowx = SCREEN_WIDTH / 2;
int16_t arrowy = SCREEN_HEIGHT - radar_size - 1;

void northPointer(float heading) {
  if (heading == -1) return;

  // heading is in degrees, but the trigonometric functions use radians.
  float angle = heading * PI / 180.0;
  int16_t x3 = radar_size * sin(angle);
  int16_t y3 = radar_size * cos(angle);


  //start x and y, end x and y, color
  tft.drawLine(arrowx, arrowy, arrowx + x3, arrowy - y3, ST77XX_WHITE);

}


void radar_circle(float heading){ //circle size of 4 right now
  // Remove the previous pointer before drawing it at its new angle.
  tft.fillRect(arrowx - radar_size - 1, arrowy - radar_size - 1, radar_size * 2 + 3, radar_size * 2 + 3, ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.drawCircle(arrowx, arrowy, radar_size, ST77XX_WHITE);

  northPointer(heading);
}


void loop(){

  down_time = (millis() - lastReceivedMillis) / 1000;
  
  tft.setCursor(5, 70);
  tft.fillRect(5, 70, 45, 10, ST77XX_BLACK);
  tft.setTextColor(ST77XX_RED);
  tft.println(down_time);

  if (receivedFlag) {
    tft.fillRect(5, 40, 150, 30, ST77XX_BLACK);
    receivedFlag = false;
    String packet;
    int state = radio.readData(packet);
    if (state == RADIOLIB_ERR_NONE) {
      int separator = packet.indexOf(':');
      uint8_t nonce[16];
      uint8_t encrypted[65];
      size_t nonceLength = 0;
      size_t encryptedLength = 0;
      bool validPacket = separator > 0
          && decodeHex(packet.substring(0, separator), nonce, sizeof(nonce), nonceLength)
          && nonceLength == sizeof(nonce)
          && decodeHex(packet.substring(separator + 1), encrypted, sizeof(encrypted) - 1, encryptedLength)
          && encryptedLength > 0;

      if (validPacket) {
        aesCtrCrypt(encrypted, encryptedLength, nonce);
        encrypted[encryptedLength] = '\0';
        String str = String((char*)encrypted);
        if (!isValidPayload(str)) {
          Serial.println("Decrypt failed; check that both devices use the same teamKey");
        } else {
          lastMsg = str;
          lastReceivedMillis = millis();

          if (gps.location.isValid()) {
            Player player = handleReceivedData(str);
            Serial.print("Received from: " + player.name + " | " + String(player.lat, 6) + ", " + String(player.lng, 6));
            arrowDraw(gps.location.lat(), player.lat, gps.location.lng(), player.lng, false);
          } else {
            arrowDraw(0.0, 0.0, 0.0, 0.0, true);
          }
        }
      }
    }
    radio.startReceive(); //go back to listening
  }

  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  //SAT DEBUG AND INFO
  static unsigned long lastSatDisplay = 0;
  if (millis() - lastSatDisplay > 1000) {
    lastSatDisplay = millis();

    int satCount = gps.satellites.value();
    bool hasFix = gps.location.isValid();

    String satStr = "Sats: " + String(satCount);
    if (hasFix) {
      satStr += " OK";
    } else {
      satStr += " ...";
    }

    int16_t x1, y1;
    uint16_t textW, textH;
    tft.setTextSize(1);
    tft.getTextBounds(satStr, 0, 0, &x1, &y1, &textW, &textH);

    int satX = 160 - textW - 5;
    int satY = 0;
    tft.fillRect(satX - 2, satY, textW + 4, textH + 2, ST77XX_BLACK);

    tft.setCursor(satX, satY);


    tft.setTextColor(hasFix ? ST77XX_GREEN : ST77XX_RED);
    tft.println(satStr);

  }

  //Periodically transmit
  if (millis() - lastSend > sendInterval) {

    lastSend = millis();


    uint8_t nonce[16];
    generateNonce(nonce);
      
    uint8_t nonceForTransmit[16];
    memcpy(nonceForTransmit, nonce, 16); // save the ORIGINAL value before it gets mutated
      
    String payload = String(DEVICE_NAME) + ":" + String(gps.location.lat(), 6) + ":" + String(gps.location.lng(), 6);
      
    uint8_t buffer[64];
    size_t len = payload.length();
    memcpy(buffer, payload.c_str(), len);
    aesCtrCrypt(buffer, len, nonce); // mutates `nonce`, but that's fine now — we don't need it anymore
      
    String message = encodeHex(nonceForTransmit, sizeof(nonceForTransmit)) + ":" + encodeHex(buffer, len); // send the SAVED original

    Serial.print("Sending: ");
    Serial.println(message);
    radio.transmit(message);  

    receivedFlag = false; //clear flag to avoid reading our own message
    radio.startReceive(); //resume listening
  }


  static unsigned long lastCompassCheck = 0;
  if (millis() - lastCompassCheck > 200) { 
    lastCompassCheck = millis();
    float heading = qmcReadHeadingCardinal();
    
    radar_circle(heading);

    if (heading == -1) {
      Serial.println("Read failed, no data available");

    } else {

      String headingText = "H: " + String(heading, 1);
      int16_t headingX = 0;
      int16_t headingY = 0;
      uint16_t headingWidth = 0;
      uint16_t headingHeight = 0;
      tft.setTextSize(1);
      tft.getTextBounds(headingText, 0, 70, &headingX, &headingY, &headingWidth, &headingHeight);
      tft.fillRect(90, 70, 70, 10, ST77XX_BLACK);
      tft.setCursor(160 - headingWidth - 2, 70);
      tft.setTextColor(ST77XX_WHITE);
      tft.print(headingText);
    } 

  }
  


  //Display Update
  //tft.fillRect(5, 40, 150, 30, ST77XX_BLACK);
  tft.setCursor(5, 40);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("Last RX:");
  tft.setCursor(5, 50);
  tft.setTextColor(ST77XX_RED);
  tft.println(lastMsg);
}

