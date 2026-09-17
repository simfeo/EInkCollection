// Minimal diagnostic: draw text and a few primitives, then stop.
// Companion to epaper_clean. No Wi-Fi, no LittleFS, no uploaded data - every
// pixel here comes from GFX, so a correct render proves the whole display path
// short of the uploader.

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

constexpr int16_t SCREEN_WIDTH = 296;
constexpr int16_t SCREEN_HEIGHT = 128;

void setup()
{
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println(F("epaper_text: start"));

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(115200, true, 50, false, SPI, SPISettings(2000000, MSBFIRST, SPI_MODE0));
  display.setRotation(1);

  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setTextWrap(false);

    display.setTextSize(2);
    display.setCursor(10, 20);
    display.print("Display OK");

    display.setTextSize(1);
    display.setCursor(10, 48);
    display.print("GxEPD2_290_BS  296x128");
    display.setCursor(10, 62);
    display.print("SPI 2 MHz  CS7 DC1 RST10 BUSY3");

    // Corner ticks expose a wrong width or rotation: all four must be visible.
    display.fillRect(0, 0, 8, 8, GxEPD_BLACK);
    display.fillRect(SCREEN_WIDTH - 8, 0, 8, 8, GxEPD_BLACK);
    display.fillRect(0, SCREEN_HEIGHT - 8, 8, 8, GxEPD_BLACK);
    display.fillRect(SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8, 8, 8, GxEPD_BLACK);

    // 1px vertical stripes: any SPI corruption turns these into visible noise.
    for (int16_t x = 10; x < SCREEN_WIDTH - 10; x += 2)
    {
      display.drawFastVLine(x, 80, 16, GxEPD_BLACK);
    }

    display.drawRect(5, 5, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 10, GxEPD_BLACK);
  }
  while (display.nextPage());

  display.hibernate();
  Serial.println(F("epaper_text: done"));
}

void loop()
{
}
