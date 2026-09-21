#include <WiFi.h>
#include <Preferences.h>
#include <USB.h>
#include <esp_system.h>

// LOLIN S2 Mini board option: "USB CDC On Boot" must be Enabled.

namespace {

constexpr char WIFI_SSID[] = "WirelessShare-Link";
constexpr uint16_t WIFI_PORT = 42800;
constexpr uint8_t PROTOCOL_VERSION = 2;
constexpr size_t HEADER_SIZE = 16;
constexpr uint32_t MAX_PAYLOAD_SIZE = 4096;
constexpr uint32_t STATUS_INTERVAL_MS = 2000;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 30000;
constexpr uint32_t WIFI_RESTART_DELAY_MS = 250;
constexpr uint32_t TCP_RECONNECT_INTERVAL_MS = 1000;
constexpr uint32_t USB_HOST_ACTIVE_MS = 5000;
constexpr size_t USB_RX_BUFFER_SIZE = 8192;

enum FrameType : uint8_t {
  FRAME_CONFIGURE = 1,
  FRAME_GET_STATUS = 2,
  FRAME_DATA = 3,
  FRAME_STATUS = 4,
  FRAME_DATA_ACK = 5,
  FRAME_DATA_NACK = 6
};

enum DeviceRole : uint8_t {
  ROLE_NONE = 0,
  ROLE_ACCESS_POINT = 1,
  ROLE_STATION = 2
};

const uint8_t MAGIC[4] = {'W', 'S', 'H', '1'};

uint32_t readU32(const uint8_t *data) {
  return uint32_t(data[0]) | (uint32_t(data[1]) << 8) |
         (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
}

void writeU32(uint8_t *data, uint32_t value) {
  data[0] = uint8_t(value);
  data[1] = uint8_t(value >> 8);
  data[2] = uint8_t(value >> 16);
  data[3] = uint8_t(value >> 24);
}

uint32_t crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffffU;
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

class FrameParser {
public:
  bool feed(uint8_t value) {
    if (_headerPosition < 4) {
      if (value == MAGIC[_headerPosition]) {
        _header[_headerPosition++] = value;
      } else {
        _headerPosition = value == MAGIC[0] ? 1 : 0;
        if (_headerPosition == 1)
          _header[0] = value;
      }
      return false;
    }

    if (_headerPosition < HEADER_SIZE) {
      _header[_headerPosition++] = value;
      if (_headerPosition != HEADER_SIZE)
        return false;
      _payloadLength = readU32(_header + 8);
      _expectedCrc = readU32(_header + 12);
      if (_header[4] != PROTOCOL_VERSION || _payloadLength > MAX_PAYLOAD_SIZE) {
        reset();
        _failed = true;
        return false;
      }
      _payloadPosition = 0;
      if (_payloadLength == 0) {
        _complete = _expectedCrc == crc32(nullptr, 0);
        if (!_complete) {
          reset();
          _failed = true;
        }
        return _complete;
      }
      return false;
    }

    _payload[_payloadPosition++] = value;
    if (_payloadPosition != _payloadLength)
      return false;
    _complete = crc32(_payload, _payloadLength) == _expectedCrc;
    if (!_complete) {
      reset();
      _failed = true;
    }
    return _complete;
  }

  uint8_t type() const { return _header[5]; }
  const uint8_t *payload() const { return _payload; }
  uint32_t payloadLength() const { return _payloadLength; }
  bool takeFailure() {
    const bool failed = _failed;
    _failed = false;
    return failed;
  }

  void next() { reset(); }

  void reset() {
    _headerPosition = 0;
    _payloadPosition = 0;
    _payloadLength = 0;
    _expectedCrc = 0;
    _complete = false;
  }

private:
  uint8_t _header[HEADER_SIZE] = {};
  uint8_t _payload[MAX_PAYLOAD_SIZE] = {};
  size_t _headerPosition = 0;
  uint32_t _payloadPosition = 0;
  uint32_t _payloadLength = 0;
  uint32_t _expectedCrc = 0;
  bool _complete = false;
  bool _failed = false;
};

template <typename T>
bool writeAll(T &output, const uint8_t *data, size_t length) {
  const uint32_t started = millis();
  size_t written = 0;
  while (written < length) {
    const size_t count = output.write(data + written, length - written);
    if (count > 0) {
      written += count;
      continue;
    }
    if (millis() - started > 5000)
      return false;
    delay(1);
  }
  return true;
}

template <typename T>
bool writeFrame(T &output, uint8_t type, const uint8_t *payload, uint32_t length) {
  if (length > MAX_PAYLOAD_SIZE)
    return false;
  uint8_t header[HEADER_SIZE] = {};
  memcpy(header, MAGIC, sizeof(MAGIC));
  header[4] = PROTOCOL_VERSION;
  header[5] = type;
  writeU32(header + 8, length);
  writeU32(header + 12, crc32(payload, length));
  return writeAll(output, header, sizeof(header))
      && (length == 0 || writeAll(output, payload, length));
}

Preferences preferences;
WiFiServer server(WIFI_PORT);
WiFiClient peer;
FrameParser usbParser;
FrameParser wifiParser;
DeviceRole role = ROLE_NONE;
String wifiPassword;
uint32_t lastStatusAt = 0;
uint32_t lastWifiReconnectAt = 0;
uint32_t lastTcpReconnectAt = 0;
uint32_t lastUsbActivityAt = 0;
uint32_t wifiReconnectCount = 0;
uint32_t tcpReconnectCount = 0;
uint8_t lastWifiDisconnectReason = 0;
esp_reset_reason_t resetReason = ESP_RST_UNKNOWN;
bool usbHostSeen = false;
bool previousPeerConnected = false;
volatile uint32_t usbRxDroppedBytes = 0;
volatile bool usbRxOverflowed = false;
uint32_t usbTxFailureCount = 0;
uint32_t usbCrcFailureCount = 0;
uint32_t tcpTxFailureCount = 0;
uint32_t lastUsbDataSequence = 0;
bool hasLastUsbDataSequence = false;
bool lastUsbDataSucceeded = false;

void onUsbCdcEvent(void *, esp_event_base_t, int32_t eventId, void *eventData) {
  if (eventId != ARDUINO_USB_CDC_RX_OVERFLOW_EVENT || eventData == nullptr)
    return;
  const arduino_usb_cdc_event_data_t *data =
      static_cast<const arduino_usb_cdc_event_data_t *>(eventData);
  usbRxDroppedBytes += data->rx_overflow.dropped_bytes;
  usbRxOverflowed = true;
}

bool writeUsbFrame(uint8_t type, const uint8_t *payload, uint32_t length) {
  if (writeFrame(Serial, type, payload, length))
    return true;
  ++usbTxFailureCount;
  return false;
}

void sendUsbDataResult(uint8_t type, uint32_t sequence) {
  uint8_t payload[4] = {};
  writeU32(payload, sequence);
  writeUsbFrame(type, payload, sizeof(payload));
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    lastWifiDisconnectReason = info.wifi_sta_disconnected.reason;
}

bool usbHostActive() {
  return usbHostSeen && millis() - lastUsbActivityAt <= USB_HOST_ACTIVE_MS;
}

void sendStatus() {
  if (!usbHostActive())
    return;
  uint8_t state = 0;
  int8_t rssi = 0;
  const char *stateText = "Not configured";

  if (role == ROLE_ACCESS_POINT) {
    state = peer.connected() ? 3 : 1;
    stateText = peer.connected() ? "Peer connected" : "Waiting for peer";
  } else if (role == ROLE_STATION) {
    if (peer.connected()) {
      state = 3;
      stateText = "Peer connected";
      rssi = int8_t(WiFi.RSSI());
    } else {
      state = WiFi.status() == WL_CONNECTED ? 4 : 2;
      stateText = state == 4 ? "Wi-Fi connected; connecting TCP" : "Connecting to Wi-Fi";
    }
  }

  char message[256] = {};
  snprintf(message, sizeof(message), "%s; reset=%u; disconnect=%u; wifi_retry=%lu; tcp_retry=%lu; usb_rx_drop=%lu; usb_tx_fail=%lu; usb_crc_fail=%lu; tcp_tx_fail=%lu",
           stateText, unsigned(resetReason), unsigned(lastWifiDisconnectReason),
           static_cast<unsigned long>(wifiReconnectCount),
           static_cast<unsigned long>(tcpReconnectCount),
           static_cast<unsigned long>(usbRxDroppedBytes),
           static_cast<unsigned long>(usbTxFailureCount),
           static_cast<unsigned long>(usbCrcFailureCount),
           static_cast<unsigned long>(tcpTxFailureCount));
  uint8_t payload[288] = {};
  payload[0] = state;
  payload[1] = role;
  payload[2] = uint8_t(rssi);
  const size_t messageLength = min(strlen(message), sizeof(payload) - 3);
  memcpy(payload + 3, message, messageLength);
  writeUsbFrame(FRAME_STATUS, payload, 3 + messageLength);
  lastStatusAt = millis();
}

void stopWireless() {
  peer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  wifiParser.reset();
  delay(100);
}

void beginStationConnection() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.config(IPAddress(192, 168, 4, 2), IPAddress(192, 168, 4, 1),
              IPAddress(255, 255, 255, 0));
  WiFi.begin(WIFI_SSID, wifiPassword.c_str(), 1);
  lastWifiReconnectAt = millis();
  lastTcpReconnectAt = 0;
}

void startWireless() {
  stopWireless();
  if (role == ROLE_ACCESS_POINT) {
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));
    WiFi.softAP(WIFI_SSID, wifiPassword.c_str(), 1);
    server.begin();
    server.setNoDelay(true);
  } else if (role == ROLE_STATION) {
    wifiReconnectCount = 0;
    tcpReconnectCount = 0;
    lastWifiDisconnectReason = 0;
    beginStationConnection();
  }
  previousPeerConnected = false;
  sendStatus();
}

void applyConfiguration(const uint8_t *payload, uint32_t length) {
  if (length < 2)
    return;
  const DeviceRole requestedRole = DeviceRole(payload[0]);
  const uint8_t passwordLength = payload[1];
  if ((requestedRole != ROLE_ACCESS_POINT && requestedRole != ROLE_STATION)
      || passwordLength < 8 || passwordLength > 63 || length != uint32_t(2 + passwordLength))
    return;

  String requestedPassword;
  requestedPassword.reserve(passwordLength);
  for (uint8_t index = 0; index < passwordLength; ++index)
    requestedPassword += char(payload[2 + index]);

  hasLastUsbDataSequence = false;
  lastUsbDataSucceeded = false;

  if (role == requestedRole && wifiPassword == requestedPassword) {
    sendStatus();
    return;
  }

  role = requestedRole;
  wifiPassword = requestedPassword;
  preferences.putUChar("role", uint8_t(role));
  preferences.putString("password", wifiPassword);
  startWireless();
}

void handleUsbFrame() {
  if (usbParser.type() == FRAME_CONFIGURE) {
    applyConfiguration(usbParser.payload(), usbParser.payloadLength());
  } else if (usbParser.type() == FRAME_GET_STATUS) {
    sendStatus();
  } else if (usbParser.type() == FRAME_DATA) {
    if (usbParser.payloadLength() < 4) {
      ++usbCrcFailureCount;
      writeUsbFrame(FRAME_DATA_NACK, nullptr, 0);
      return;
    }
    const uint32_t sequence = readU32(usbParser.payload());
    if (hasLastUsbDataSequence && sequence == lastUsbDataSequence && lastUsbDataSucceeded) {
      sendUsbDataResult(FRAME_DATA_ACK, sequence);
      return;
    }
    hasLastUsbDataSequence = true;
    lastUsbDataSequence = sequence;
    lastUsbDataSucceeded = false;
    if (!peer.connected()) {
      sendUsbDataResult(FRAME_DATA_NACK, sequence);
      return;
    }
    if (writeFrame(peer, FRAME_DATA, usbParser.payload() + 4, usbParser.payloadLength() - 4)) {
      lastUsbDataSucceeded = true;
      sendUsbDataResult(FRAME_DATA_ACK, sequence);
    } else {
      ++tcpTxFailureCount;
      sendUsbDataResult(FRAME_DATA_NACK, sequence);
      peer.stop();
    }
  }
}

void handleWifiFrame() {
  if (wifiParser.type() == FRAME_DATA && usbHostActive())
    writeUsbFrame(FRAME_DATA, wifiParser.payload(), wifiParser.payloadLength());
}

void serviceUsb() {
  if (usbRxOverflowed) {
    usbRxOverflowed = false;
    usbParser.reset();
    writeUsbFrame(FRAME_DATA_NACK, nullptr, 0);
  }
  while (Serial.available() > 0) {
    usbHostSeen = true;
    lastUsbActivityAt = millis();
    const bool complete = usbParser.feed(uint8_t(Serial.read()));
    if (usbParser.takeFailure()) {
      ++usbCrcFailureCount;
      writeUsbFrame(FRAME_DATA_NACK, nullptr, 0);
    }
    if (complete) {
      handleUsbFrame();
      usbParser.next();
    }
  }
}

void serviceWifi() {
  if (!peer.connected())
    return;
  while (peer.available() > 0) {
    if (wifiParser.feed(uint8_t(peer.read()))) {
      handleWifiFrame();
      wifiParser.next();
    }
  }
}

void maintainConnection() {
  if (role == ROLE_ACCESS_POINT) {
    if (!peer.connected()) {
      peer.stop();
      WiFiClient incoming = server.accept();
      if (incoming) {
        peer.stop();
        peer = incoming;
        peer.setNoDelay(true);
        wifiParser.reset();
      }
    }
  } else if (role == ROLE_STATION) {
    if (WiFi.status() != WL_CONNECTED) {
      if (peer)
        peer.stop();
      if (millis() - lastWifiReconnectAt >= WIFI_RECONNECT_INTERVAL_MS) {
        WiFi.disconnect(true, false);
        delay(WIFI_RESTART_DELAY_MS);
        ++wifiReconnectCount;
        beginStationConnection();
        wifiParser.reset();
      }
    } else if (!peer.connected() && millis() - lastTcpReconnectAt >= TCP_RECONNECT_INTERVAL_MS) {
      peer.stop();
      ++tcpReconnectCount;
      peer.connect(IPAddress(192, 168, 4, 1), WIFI_PORT, 1000);
      if (peer.connected())
        peer.setNoDelay(true);
      wifiParser.reset();
      lastTcpReconnectAt = millis();
    }
  }

  const bool connected = peer.connected();
  if (connected != previousPeerConnected) {
    previousPeerConnected = connected;
    sendStatus();
  }
}

} // namespace

void setup() {
  Serial.setRxBufferSize(USB_RX_BUFFER_SIZE);
  Serial.onEvent(ARDUINO_USB_CDC_RX_OVERFLOW_EVENT, onUsbCdcEvent);
  Serial.begin(921600);
  resetReason = esp_reset_reason();
  WiFi.onEvent(onWiFiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  preferences.begin("wirelessShare", false);
  role = DeviceRole(preferences.getUChar("role", ROLE_NONE));
  wifiPassword = preferences.getString("password", "");
  if ((role == ROLE_ACCESS_POINT || role == ROLE_STATION)
      && wifiPassword.length() >= 8 && wifiPassword.length() <= 63)
    startWireless();
}

void loop() {
  serviceUsb();
  maintainConnection();
  serviceWifi();
  if (millis() - lastStatusAt >= STATUS_INTERVAL_MS)
    sendStatus();
  delay(1);
}
