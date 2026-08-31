#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <RadioLib.h>
#include "TinyGPSPlus.h"

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

//each board name
#define DEVICE_NAME "NodeA"

volatile bool receivedFlag = false;
void onReceive() { receivedFlag = true; }

unsigned long lastSend = 0;
unsigned long lastReceivedMillis = 0;
unsigned long down_time = 0;
const unsigned long sendInterval = 5000; //time between each transmission (in milliseconds)
String lastMsg = "waiting...";

void setup() {
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

  tft.initR(INITR_MINI160x80_PLUGIN);
  tft.invertDisplay(false);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(5, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.println(DEVICE_NAME);

  //Radio ---
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int state = radio.begin(915.0);
  if (state != RADIOLIB_ERR_NONE) {
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
}


void loop() {


  down_time = (millis() - lastReceivedMillis) / 1000;
  tft.setCursor(5, 70);
  tft.fillRect(5, 70, 150, 30, ST77XX_BLACK);
  tft.setTextColor(ST77XX_RED);
  tft.println(down_time);

  if (receivedFlag) {
    tft.fillRect(5, 40, 150, 30, ST77XX_BLACK);
    receivedFlag = false;
    String str;
    int state = radio.readData(str);
    if (state == RADIOLIB_ERR_NONE) {
      lastMsg = str;
      lastReceivedMillis = millis();
    }
    radio.startReceive(); //go back to listening
  }

  //Periodically transmit
  if (millis() - lastSend > sendInterval) {
    lastSend = millis();
    radio.transmit("Hello from " + String(DEVICE_NAME));
    receivedFlag = false; //clear flag to avoid reading our own message
    radio.startReceive(); //resume listening
  }


  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  //SAT DEBUG AND INFO
  static unsigned long lastSatDisplay = 0;
  if (millis() - lastSatDisplay > 1000) {
    lastSatDisplay = millis();

    //Build the status string. Two pieces of info: satellite count,
    //and whether we have a valid fix yet.
    int satCount = gps.satellites.value();
    bool hasFix = gps.location.isValid();

    String satStr = "Sats: " + String(satCount);
    if (hasFix) {
      satStr += " OK";
    } else {
      satStr += " ...";
    }

    //Same measure-then-right-align approach as before, so it stays
    //pinned to the right edge regardless of digit count.
    int16_t x1, y1;
    uint16_t textW, textH;
    tft.setTextSize(1);
    tft.getTextBounds(satStr, 0, 0, &x1, &y1, &textW, &textH);

    int satX = 160 - textW - 5;
    int satY = 0;
    tft.fillRect(satX - 2, satY, textW + 4, textH + 2, ST77XX_BLACK);

    tft.setCursor(satX, satY);
    //Color-code it: red while searching, green once we have a fix —
    //gives you an at-a-glance status without reading the text.
    tft.setTextColor(hasFix ? ST77XX_GREEN : ST77XX_RED);
    tft.println(satStr);

    //print long and lat to serial
    Serial.print(F("Location: ")); 
    if (gps.location.isValid()){
      Serial.print(gps.location.lat(), 6);
      Serial.print(F(","));
      Serial.print(gps.location.lng(), 6);
    }
    else{
      Serial.print(F("INVALID"));
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