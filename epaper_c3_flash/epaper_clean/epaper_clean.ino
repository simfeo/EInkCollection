// Minimal diagnostic: clear the panel to white and stop.
// No Wi-Fi, no LittleFS, no uploaded data - if this leaves snow on the screen,
// the fault is below the application layer (wiring, SPI, or driver class).

#include <SPI.h>
#include <GxEPD2_BW.h>

constexpr int EPD_SCK  = 4;
constexpr int EPD_MOSI = 6;
constexpr int EPD_CS   = 7;
constexpr int EPD_DC   = 1;
constexpr int EPD_RST  = 10;
constexpr int EPD_BUSY = 3;

using Panel = GxEPD2_290_BS;

GxEPD2_BW<Panel, Panel::HEIGHT> display(
  Panel(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

void setup()
{
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println(F("epaper_clean: start"));

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(115200, true, 50, false, SPI, SPISettings(2000000, MSBFIRST, SPI_MODE0));
  display.setRotation(1);

  Serial.println(F("epaper_clean: clearing"));
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
  }
  while (display.nextPage());

  display.hibernate();
  Serial.println(F("epaper_clean: done"));
}

void loop()
{
}
