#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <SPI.h>
#include <GxEPD2_BW.h>

#include "web_page.h"

// -----------------------------------------------------------------------------
// Wi-Fi access point. Change these two values if needed.
// The WPA2 password must contain at least 8 characters.
// -----------------------------------------------------------------------------
constexpr char AP_SSID[] = "E-Paper-C3";
constexpr char AP_PASSWORD[] = "epaper123";

// ESP32-C3 Super Mini -> WeAct Studio 2.9" DEPG0290BS
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

constexpr uint16_t SCREEN_WIDTH = 296;
constexpr uint16_t SCREEN_HEIGHT = 128;
constexpr size_t ROW_BYTES = (SCREEN_WIDTH + 7) / 8;
constexpr size_t BITMAP_BYTES = ROW_BYTES * SCREEN_HEIGHT;
static_assert(BITMAP_BYTES == 4736, "Unexpected e-paper bitmap size");

constexpr char BITMAP_PATH[] = "/screen.bin";
constexpr char TEMP_PATH[] = "/screen.tmp";
constexpr char BACKUP_PATH[] = "/screen.bak";

constexpr uint16_t DNS_PORT = 53;
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

WebServer server(80);
DNSServer dnsServer;

uint8_t screenBitmap[BITMAP_BYTES];

File uploadFile;
bool fsReady = false;
bool uploadStarted = false;
bool uploadSucceeded = false;
size_t uploadBytes = 0;
String uploadError;

bool refreshPending = false;
bool displayBusy = false;
uint32_t refreshAt = 0;

bool storedBitmapIsValid()
{
  if (!fsReady || !LittleFS.exists(BITMAP_PATH))
  {
    return false;
  }

  File file = LittleFS.open(BITMAP_PATH, FILE_READ);
  if (!file)
  {
    return false;
  }

  const bool valid = file.size() == BITMAP_BYTES;
  file.close();
  return valid;
}

void recoverInterruptedUpload()
{
  if (!fsReady)
  {
    return;
  }

  if (!storedBitmapIsValid() && LittleFS.exists(BACKUP_PATH))
  {
    File backup = LittleFS.open(BACKUP_PATH, FILE_READ);
    const bool backupIsValid = backup && backup.size() == BITMAP_BYTES;
    backup.close();

    if (backupIsValid)
    {
      LittleFS.remove(BITMAP_PATH);
      LittleFS.rename(BACKUP_PATH, BITMAP_PATH);
    }
  }

  // These files are only needed while a request is being committed.
  LittleFS.remove(TEMP_PATH);
  if (storedBitmapIsValid())
  {
    LittleFS.remove(BACKUP_PATH);
  }
}

bool loadStoredBitmap()
{
  if (!storedBitmapIsValid())
  {
    return false;
  }

  File file = LittleFS.open(BITMAP_PATH, FILE_READ);
  if (!file)
  {
    return false;
  }

  const size_t bytesRead = file.read(screenBitmap, BITMAP_BYTES);
  file.close();
  return bytesRead == BITMAP_BYTES;
}

void drawBitmapOnDisplay()
{
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(
      0,
      0,
      screenBitmap,
      SCREEN_WIDTH,
      SCREEN_HEIGHT,
      GxEPD_BLACK
    );
  }
  while (display.nextPage());

  display.powerOff();
}

void drawSetupScreen()
{
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setTextWrap(false);

    display.setTextSize(2);
    display.setCursor(10, 24);
    display.print("E-Paper uploader");

    display.setTextSize(1);
    display.setCursor(10, 52);
    display.print("Wi-Fi: ");
    display.print(AP_SSID);

    display.setCursor(10, 70);
    display.print("Password: ");
    display.print(AP_PASSWORD);

    display.setCursor(10, 88);
    display.print("Open: http://192.168.4.1");

    display.drawRect(5, 5, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 10, GxEPD_BLACK);
  }
  while (display.nextPage());

  display.powerOff();
}

bool installTemporaryBitmap()
{
  if (!fsReady || !LittleFS.exists(TEMP_PATH))
  {
    return false;
  }

  if (LittleFS.exists(BACKUP_PATH))
  {
    LittleFS.remove(BACKUP_PATH);
  }

  const bool hadOldBitmap = LittleFS.exists(BITMAP_PATH);
  if (hadOldBitmap && !LittleFS.rename(BITMAP_PATH, BACKUP_PATH))
  {
    return false;
  }

  if (!LittleFS.rename(TEMP_PATH, BITMAP_PATH))
  {
    if (hadOldBitmap)
    {
      LittleFS.rename(BACKUP_PATH, BITMAP_PATH);
    }
    return false;
  }

  if (hadOldBitmap)
  {
    LittleFS.remove(BACKUP_PATH);
  }

  return true;
}

void handleRoot()
{
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleStatus()
{
  String json;
  json.reserve(128);
  json += F("{\"ok\":true,\"hasImage\":");
  json += storedBitmapIsValid() ? F("true") : F("false");
  json += F(",\"busy\":");
  json += (displayBusy || refreshPending) ? F("true") : F("false");
  json += F(",\"clients\":");
  json += String(WiFi.softAPgetStationNum());
  json += F("}");

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleCurrentBitmap()
{
  if (!storedBitmapIsValid())
  {
    server.send(404, "application/json", F("{\"ok\":false,\"error\":\"No stored image\"}"));
    return;
  }

  File file = LittleFS.open(BITMAP_PATH, FILE_READ);
  if (!file)
  {
    server.send(500, "application/json", F("{\"ok\":false,\"error\":\"Cannot open image\"}"));
    return;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(file, "application/octet-stream");
  file.close();
}

void handleUploadData()
{
  HTTPUpload &upload = server.upload();

  switch (upload.status)
  {
    case UPLOAD_FILE_START:
      if (uploadFile)
      {
        uploadFile.close();
      }

      uploadStarted = true;
      uploadSucceeded = false;
      uploadBytes = 0;
      uploadError = "";

      if (!fsReady)
      {
        uploadError = "LittleFS is not available";
        break;
      }

      if (displayBusy || refreshPending)
      {
        uploadError = "Display is busy";
        break;
      }

      LittleFS.remove(TEMP_PATH);
      uploadFile = LittleFS.open(TEMP_PATH, FILE_WRITE);
      if (!uploadFile)
      {
        uploadError = "Cannot create temporary file";
      }
      break;

    case UPLOAD_FILE_WRITE:
      if (uploadError.length() > 0)
      {
        break;
      }

      if (uploadBytes + upload.currentSize > BITMAP_BYTES)
      {
        uploadError = "Uploaded image is too large";
        uploadFile.close();
        LittleFS.remove(TEMP_PATH);
        break;
      }

      {
        const size_t written = uploadFile.write(upload.buf, upload.currentSize);
        uploadBytes += written;
        if (written != upload.currentSize)
        {
          uploadError = "Cannot write image to flash";
          uploadFile.close();
          LittleFS.remove(TEMP_PATH);
        }
      }
      break;

    case UPLOAD_FILE_END:
      if (uploadFile)
      {
        uploadFile.close();
      }

      if (uploadError.length() == 0 && uploadBytes != BITMAP_BYTES)
      {
        uploadError = "Wrong image size";
      }

      if (uploadError.length() == 0 && !installTemporaryBitmap())
      {
        uploadError = "Cannot install uploaded image";
      }

      uploadSucceeded = uploadError.length() == 0;
      if (!uploadSucceeded)
      {
        LittleFS.remove(TEMP_PATH);
      }
      break;

    case UPLOAD_FILE_ABORTED:
      if (uploadFile)
      {
        uploadFile.close();
      }
      LittleFS.remove(TEMP_PATH);
      uploadError = "Upload aborted";
      uploadSucceeded = false;
      break;

    default:
      break;
  }
}

void handleUploadFinished()
{
  server.sendHeader("Cache-Control", "no-store");

  if (uploadStarted && uploadSucceeded)
  {
    server.send(202, "application/json", F("{\"ok\":true,\"message\":\"Image accepted\"}"));

    // Let the HTTP response leave the socket before the slow e-paper refresh.
    refreshPending = true;
    refreshAt = millis() + 400;
  }
  else
  {
    if (!uploadStarted && uploadError.length() == 0)
    {
      uploadError = "No image received";
    }

    String json = F("{\"ok\":false,\"error\":\"");
    json += uploadError;
    json += F("\"}");
    server.send(400, "application/json", json);
  }

  uploadStarted = false;
  uploadSucceeded = false;
  uploadBytes = 0;
  uploadError = "";
}

void handleNotFound()
{
  // Together with the wildcard DNS server this makes most captive-portal
  // checks open the uploader automatically. Direct access always works at
  // http://192.168.4.1.
  if (server.method() == HTTP_GET)
  {
    handleRoot();
  }
  else
  {
    server.send(404, "application/json", F("{\"ok\":false,\"error\":\"Not found\"}"));
  }
}

void startAccessPoint()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  if (!WiFi.softAP(AP_SSID, AP_PASSWORD))
  {
    Serial.println(F("Failed to start Wi-Fi access point"));
    return;
  }

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", AP_IP);

  Serial.println();
  Serial.println(F("Wi-Fi access point started"));
  Serial.print(F("SSID: "));
  Serial.println(AP_SSID);
  Serial.print(F("Password: "));
  Serial.println(AP_PASSWORD);
  Serial.print(F("Open: http://"));
  Serial.println(WiFi.softAPIP());
}

void startWebServer()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/current", HTTP_GET, handleCurrentBitmap);
  server.on("/upload", HTTP_POST, handleUploadFinished, handleUploadData);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println(F("HTTP server started"));
}

void setup()
{
  Serial.begin(115200);
  delay(800);

  fsReady = LittleFS.begin(true);
  Serial.println(fsReady ? F("LittleFS ready") : F("LittleFS initialization failed"));
  recoverInterruptedUpload();

  startAccessPoint();
  startWebServer();

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

  // These settings are known to work with WeAct Studio DEPG0290BS.
  display.init(115200, true, 50, false);
  display.setRotation(1); // landscape: 296 x 128

  if (loadStoredBitmap())
  {
    drawBitmapOnDisplay();
  }
  else
  {
    drawSetupScreen();
  }
}

void loop()
{
  dnsServer.processNextRequest();
  server.handleClient();

  if (refreshPending && static_cast<int32_t>(millis() - refreshAt) >= 0)
  {
    refreshPending = false;
    displayBusy = true;

    if (loadStoredBitmap())
    {
      Serial.println(F("Refreshing e-paper..."));
      drawBitmapOnDisplay();
      Serial.println(F("E-paper refresh complete"));
    }
    else
    {
      Serial.println(F("Stored bitmap is missing or invalid"));
    }

    displayBusy = false;
  }

  delay(50);
}
