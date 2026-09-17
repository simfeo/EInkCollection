// E-paper frame receiver for ESP32-C3 Super Mini + WeAct Studio 2.9" DEPG0290BS.
//
// No Wi-Fi: the radio is never started, which is the whole point of this
// variant. A phone pushes a ready-made 1-bit frame over BLE, the frame is
// stored in LittleFS and shown again after a reset.

#include <FS.h>
#include <LittleFS.h>
#include <SPI.h>
#include <GxEPD2_BW.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>


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

constexpr char DEVICE_NAME[] = "E-Paper-C3";
#define SERVICE_UUID "a1b2c000-5e1f-4a7d-9c3b-2f6e8d0a4b71"
#define CONTROL_UUID "a1b2c001-5e1f-4a7d-9c3b-2f6e8d0a4b71"
#define DATA_UUID    "a1b2c002-5e1f-4a7d-9c3b-2f6e8d0a4b71"

enum : uint8_t {
  CMD_BEGIN  = 0x01,
  CMD_COMMIT = 0x02,
  CMD_ABORT  = 0x03,
  CMD_STATUS = 0x04
};

enum : uint8_t {
  EVT_STATE    = 0x10,
  EVT_PROGRESS = 0x11,
  EVT_OK       = 0x12,
  EVT_ERROR    = 0x13
};

enum : uint8_t {
  STATE_IDLE       = 0,
  STATE_RECEIVING  = 1,
  STATE_COMMITTING = 2,
  STATE_REFRESHING = 3
};

enum : uint8_t {
  ERR_NO_TRANSFER = 1,
  ERR_OVERFLOW    = 2,
  ERR_LENGTH      = 3,
  ERR_CRC         = 4,
  ERR_STORAGE     = 5,
  ERR_BUSY        = 6
};

uint8_t screenBitmap[BITMAP_BYTES];

BLECharacteristic *controlCharacteristic = nullptr;
bool fsReady = false;
bool clientConnected = false;

// The incoming frame is assembled in RAM. BLE write callbacks run on the
// NimBLE host task, whose stack is 5120 bytes, and the littlefs write path can
// recurse while allocating blocks or collecting garbage -- too close to that
// limit to risk. Nothing in a callback touches the filesystem; loop() does.
uint8_t receiveBuffer[BITMAP_BYTES];

bool transferOpen = false;
uint32_t expectedLength = 0;
uint32_t expectedCrc = 0;
uint32_t receivedBytes = 0;
uint16_t chunkCounter = 0;

volatile bool commitPending = false;
bool displayBusy = false;

// -----------------------------------------------------------------------------
// Notifications
// -----------------------------------------------------------------------------

void notifyRaw(const uint8_t *data, size_t length)
{
  if (!controlCharacteristic || !clientConnected)
  {
    return;
  }

  controlCharacteristic->setValue(const_cast<uint8_t *>(data), length);
  controlCharacteristic->notify();
}

void notifyState(uint8_t state)
{
  uint8_t frame[6];
  frame[0] = EVT_STATE;
  frame[1] = state;
  memcpy(frame + 2, &receivedBytes, sizeof(receivedBytes));
  notifyRaw(frame, sizeof(frame));
}

void notifyProgress()
{
  uint8_t frame[5];
  frame[0] = EVT_PROGRESS;
  memcpy(frame + 1, &receivedBytes, sizeof(receivedBytes));
  notifyRaw(frame, sizeof(frame));
}

void notifyOk()
{
  const uint8_t frame[1] = { EVT_OK };
  notifyRaw(frame, sizeof(frame));
}

void notifyError(uint8_t reason)
{
  const uint8_t frame[2] = { EVT_ERROR, reason };
  notifyRaw(frame, sizeof(frame));
}

// -----------------------------------------------------------------------------
// CRC32 (same polynomial as zlib, computed on the fly)
// -----------------------------------------------------------------------------

uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length)
{
  crc = ~crc;
  while (length--)
  {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
    }
  }
  return ~crc;
}

uint32_t runningCrc = 0;

// -----------------------------------------------------------------------------
// Storage
// -----------------------------------------------------------------------------

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

void recoverInterruptedTransfer()
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

// Writes the received frame to the temp file. Called from loop(), never from a
// BLE callback.
bool storeReceivedBuffer()
{
  if (!fsReady)
  {
    return false;
  }

  LittleFS.remove(TEMP_PATH);
  File file = LittleFS.open(TEMP_PATH, FILE_WRITE);
  if (!file)
  {
    return false;
  }

  const size_t written = file.write(receiveBuffer, BITMAP_BYTES);
  file.close();

  if (written != BITMAP_BYTES)
  {
    LittleFS.remove(TEMP_PATH);
    return false;
  }
  return true;
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

// -----------------------------------------------------------------------------
// Display
// -----------------------------------------------------------------------------

void drawBitmapOnDisplay()
{
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(0, 0, screenBitmap, SCREEN_WIDTH, SCREEN_HEIGHT, GxEPD_BLACK);
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
    display.print("E-Paper BLE");

    display.setTextSize(1);
    display.setCursor(10, 52);
    display.print("Bluetooth name:");

    display.setCursor(10, 70);
    display.print(DEVICE_NAME);

    display.setCursor(10, 94);
    display.print("Waiting for a phone...");

    display.drawRect(5, 5, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 10, GxEPD_BLACK);
  }
  while (display.nextPage());

  display.powerOff();
}

// -----------------------------------------------------------------------------
// Transfer
// -----------------------------------------------------------------------------

void abortTransfer()
{
  transferOpen = false;
  receivedBytes = 0;
  chunkCounter = 0;
  runningCrc = 0;
}

void handleBegin(const uint8_t *payload, size_t length)
{
  if (length < 8)
  {
    notifyError(ERR_LENGTH);
    return;
  }

  if (displayBusy || commitPending)
  {
    notifyError(ERR_BUSY);
    return;
  }

  // A phone that lost its connection mid-transfer just begins again.
  abortTransfer();

  memcpy(&expectedLength, payload, sizeof(expectedLength));
  memcpy(&expectedCrc, payload + 4, sizeof(expectedCrc));

  if (expectedLength != BITMAP_BYTES)
  {
    notifyError(ERR_LENGTH);
    return;
  }

  if (!fsReady)
  {
    notifyError(ERR_STORAGE);
    return;
  }

  transferOpen = true;
  notifyState(STATE_RECEIVING);
}

void handleData(const uint8_t *payload, size_t length)
{
  if (!transferOpen)
  {
    notifyError(ERR_NO_TRANSFER);
    return;
  }

  if (receivedBytes + length > expectedLength)
  {
    abortTransfer();
    notifyError(ERR_OVERFLOW);
    return;
  }

  memcpy(receiveBuffer + receivedBytes, payload, length);
  runningCrc = crc32Update(runningCrc, payload, length);
  receivedBytes += length;

  if (++chunkCounter % 8 == 0)
  {
    notifyProgress();
  }
}

void handleCommit()
{
  if (!transferOpen)
  {
    notifyError(ERR_NO_TRANSFER);
    return;
  }

  transferOpen = false;

  if (receivedBytes != expectedLength)
  {
    abortTransfer();
    notifyError(ERR_LENGTH);
    return;
  }

  if (runningCrc != expectedCrc)
  {
    abortTransfer();
    notifyError(ERR_CRC);
    return;
  }

  notifyState(STATE_COMMITTING);

  // Storing and refreshing both happen in loop(): the flash write must stay off
  // the NimBLE task's stack, and the refresh takes seconds.
  commitPending = true;
}

// -----------------------------------------------------------------------------
// BLE callbacks
// -----------------------------------------------------------------------------

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *server) override
  {
    clientConnected = true;
    Serial.println(F("BLE client connected"));
  }

  void onDisconnect(BLEServer *server) override
  {
    clientConnected = false;
    Serial.println(F("BLE client disconnected"));

    // A half-finished frame is worthless; drop it and let the next phone start
    // clean rather than leaving a stale temp file behind.
    if (transferOpen)
    {
      abortTransfer();
    }

    server->startAdvertising();
  }
};

class ControlCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic) override
  {
    // auto, because getValue() is std::string on core 2.x and String on 3.x.
    auto value = characteristic->getValue();
    if (value.length() < 1)
    {
      return;
    }

    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(value.c_str());
    const uint8_t opcode = bytes[0];

    switch (opcode)
    {
      case CMD_BEGIN:
        handleBegin(bytes + 1, value.length() - 1);
        break;

      case CMD_COMMIT:
        handleCommit();
        break;

      case CMD_ABORT:
        abortTransfer();
        notifyState(STATE_IDLE);
        break;

      case CMD_STATUS:
        notifyState(displayBusy ? STATE_REFRESHING
                                : (transferOpen ? STATE_RECEIVING : STATE_IDLE));
        break;

      default:
        break;
    }
  }
};

class DataCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic) override
  {
    auto value = characteristic->getValue();
    if (value.length() == 0)
    {
      return;
    }

    handleData(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
  }
};

void startBle()
{
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(247);

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);

  controlCharacteristic = service->createCharacteristic(
    CONTROL_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );
  controlCharacteristic->setCallbacks(new ControlCallbacks());
  // No explicit 0x2902 descriptor: the NimBLE-backed stack in core 3.x adds the
  // CCCD itself for a characteristic that declares NOTIFY.

  BLECharacteristic *dataCharacteristic = service->createCharacteristic(
    DATA_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  dataCharacteristic->setCallbacks(new DataCallbacks());

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.print(F("BLE advertising as "));
  Serial.println(DEVICE_NAME);
}

void setup()
{
  Serial.begin(115200);
  delay(800);

  // There is deliberately no WiFi.h and no WiFi call anywhere in this sketch.
  // The ESP32 does not bring the Wi-Fi radio up on its own, so never starting
  // it is what keeps it off -- calling esp_wifi_stop() on a stack that was
  // never initialised would only return an error.

  fsReady = LittleFS.begin(true);
  Serial.println(fsReady ? F("LittleFS ready") : F("LittleFS initialization failed"));
  recoverInterruptedTransfer();

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
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

  startBle();
}

void loop()
{
  if (commitPending)
  {
    commitPending = false;
    displayBusy = true;
    notifyState(STATE_REFRESHING);

    // The frame is already verified, so a storage failure here leaves the
    // previous image on screen rather than a half-written one.
    if (storeReceivedBuffer() && installTemporaryBitmap())
    {
      memcpy(screenBitmap, receiveBuffer, BITMAP_BYTES);
      Serial.println(F("Refreshing e-paper..."));
      drawBitmapOnDisplay();
      Serial.println(F("E-paper refresh complete"));
      displayBusy = false;
      notifyOk();
    }
    else
    {
      LittleFS.remove(TEMP_PATH);
      displayBusy = false;
      notifyError(ERR_STORAGE);
    }

    receivedBytes = 0;
    chunkCounter = 0;
    runningCrc = 0;
  }

  delay(10);
}
