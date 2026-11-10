#include "ble_service.h"

#include "config.h"

#if FEATURE_BLE
#include <NimBLEDevice.h>

#include "imu_monitor.h"

static NimBLECharacteristic* s_status = nullptr;
static NimBLECharacteristic* s_alert = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { stateSetBleConnected(true); }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    stateSetBleConnected(false);
    NimBLEDevice::startAdvertising();  // let the phone reconnect
  }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    String cmd = c->getValue().c_str();
    cmd.trim();
    cmd.toUpperCase();
    if (cmd == "CLEAR") {
      imuRequestCrashClear();
      eventPost(EVT_BLE_CMD, 1);
    }
  }
};

void bleInit() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = server->createService(BLE_SERVICE_UUID);
  s_status = svc->createCharacteristic(BLE_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  s_alert = svc->createCharacteristic(BLE_ALERT_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* cmd = svc->createCharacteristic(BLE_COMMAND_UUID, NIMBLE_PROPERTY::WRITE);
  cmd->setCallbacks(new CommandCallbacks());
  s_alert->setValue("BOOT,1");
  server->start();  // NimBLE 2.x starts all services with the server

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(BLE_DEVICE_NAME);
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->enableScanResponse(true);
  adv->start();
}

static void put16(uint8_t* p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = v >> 8;
}

void bleSendStatus(const SystemState& s, uint32_t bits) {
  uint8_t buf[18];
  uint8_t flags = 0;
  if (s.helmetOn) flags |= 1 << 0;
  if (s.ignition != IGN_LOCKED) flags |= 1 << 1;
  if (s.ignition == IGN_GRACE) flags |= 1 << 2;
  if (bits & ALERT_CRASH) flags |= 1 << 3;
  if (bits & ALERT_DROWSY) flags |= 1 << 4;
  if (bits & ALERT_BLIND_L) flags |= 1 << 5;
  if (bits & ALERT_BLIND_R) flags |= 1 << 6;
  if (bits & ALERT_FIT_FAULT) flags |= 1 << 7;

  buf[0] = 1;
  buf[1] = flags;
  put16(buf + 2, static_cast<uint16_t>(s.distLcm));
  put16(buf + 4, static_cast<uint16_t>(s.distRcm));
  put16(buf + 6, static_cast<uint16_t>(static_cast<int16_t>((s.pitchDeg - s.pitchBaseDeg) * 10.0f)));
  put16(buf + 8, s.fsrRaw);
  buf[10] = s.servoDeg;
  buf[11] = s.heartBpm;
  put16(buf + 12, s.irRaw);
  const uint32_t up = millis();
  memcpy(buf + 14, &up, 4);  // ESP32 is little-endian

  s_status->setValue(buf, sizeof(buf));
  if (s.bleConnected) s_status->notify();
}

void bleSendEvent(const AppEvent& ev) {
  char msg[20];
  snprintf(msg, sizeof(msg), "%s,%ld", eventName(ev.code), static_cast<long>(ev.value));
  s_alert->setValue(reinterpret_cast<const uint8_t*>(msg), strlen(msg));
  s_alert->notify();
}

#else  // FEATURE_BLE == 0
void bleInit() {}
void bleSendStatus(const SystemState&, uint32_t) {}
void bleSendEvent(const AppEvent&) {}
#endif
