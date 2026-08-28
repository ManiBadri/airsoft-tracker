#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <RadioLib.h>

#define LORA_NSS   8
#define LORA_SCK   9
#define LORA_MOSI  10
#define LORA_MISO  11
#define LORA_RST   12
#define LORA_BUSY  13
#define LORA_DIO1  14

constexpr uint8_t VEXT_CTRL = 3;
constexpr uint8_t TFT_BL    = 21;
constexpr uint8_t TFT_CS    = 38;
constexpr uint8_t TFT_DC    = 40;
constexpr uint8_t TFT_RST   = 39;
constexpr uint8_t TFT_MOSI  = 42;
constexpr uint8_t TFT_SCLK  = 41;

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

//each board name
#define DEVICE_NAME "NodeB"

volatile bool receivedFlag = false;
void onReceive() { receivedFlag = true; }

unsigned long lastSend = 0;
const unsigned long sendInterval = 5000; //time between each transmission (in milliseconds)
String lastMsg = "waiting...";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // --- Power up display FIRST ---
  pinMode(VEXT_CTRL, OUTPUT);
  digitalWrite(VEXT_CTRL, HIGH);
  delay(100);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.initR(INITR_MINI160x80_PLUGIN);
  tft.invertDisplay(false);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(5, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.println(DEVICE_NAME);

  // --- Radio ---
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
  //Check for incoming packet
  if (receivedFlag) {
    receivedFlag = false;
    String str;
    int state = radio.readData(str);
    if (state == RADIOLIB_ERR_NONE) {
      lastMsg = str;
      Serial.print("Received: ");
      Serial.println(str);
      Serial.print("RSSI: ");
      Serial.println(radio.getRSSI());
    }
    radio.startReceive(); //go back to listening
  }

  //Periodically transmit
  if (millis() - lastSend > sendInterval) {
    lastSend = millis();
    Serial.println("Transmitting...");
    radio.transmit("Hello from " + String(DEVICE_NAME));
    radio.startReceive(); //resume listening
  }

  //Display Update
  tft.fillRect(5, 40, 150, 30, ST77XX_BLACK);
  tft.setCursor(5, 40);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("Last RX:");
  tft.setCursor(5, 50);
  tft.setTextColor(ST77XX_RED);
  tft.println(lastMsg);
}