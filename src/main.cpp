#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

constexpr uint8_t VEXT_CTRL = 3;   // powers GNSS + display
constexpr uint8_t TFT_BL    = 21;  // backlight

constexpr uint8_t TFT_CS   = 38;
constexpr uint8_t TFT_DC   = 40;
constexpr uint8_t TFT_RST  = 39;
constexpr uint8_t TFT_MOSI = 42;
constexpr uint8_t TFT_SCLK = 41;

//Software SPI constructor
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(VEXT_CTRL, OUTPUT);
  digitalWrite(VEXT_CTRL, HIGH);   //power on display + GNSS rail
  delay(100);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);      //backlight on

  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);              //adjust 0-3 if orientation looks wrong
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.println("Hello watch!");
  Serial.println("Display initialized");
}

void loop() {
  tft.fillRect(0, 10, 160, 10, ST77XX_BLACK); //clear just this line to avoid flicker
  tft.setCursor(0, 10);
  tft.setTextColor(ST77XX_RED);
  tft.println("Alive: " + String(millis() / 1000));
  Serial.println("Alive...");
  delay(1000);
}