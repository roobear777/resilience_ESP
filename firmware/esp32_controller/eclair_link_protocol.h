#ifndef TARDI_ECLAIR_LINK_PROTOCOL_H
#define TARDI_ECLAIR_LINK_PROTOCOL_H

#include <Arduino.h>

// Dedicated full-duplex UART link. Native USB remains on GPIO19/GPIO20.
constexpr uint32_t ECLAIR_LINK_BAUD = 2000000;
constexpr uint32_t ECLAIR_STATE_MAGIC = 0x314C4345;  // "ECL1" on the wire
constexpr uint32_t ECLAIR_STATUS_MAGIC = 0x3154415A; // "ZAT1" on the wire
constexpr uint8_t ECLAIR_PROTOCOL_VERSION = 1;
constexpr uint8_t ECLAIR_PACKET_STATE = 1;
constexpr uint8_t ECLAIR_PACKET_STATUS = 2;
constexpr uint8_t ECLAIR_WIRE_ZONE_COUNT = 7;
constexpr uint8_t ECLAIR_WIRE_LOOK_COUNT = 2;
constexpr uint32_t ECLAIR_STATE_INTERVAL_MS = 20;
constexpr uint32_t ECLAIR_LINK_TIMEOUT_MS = 500;
constexpr uint32_t ECLAIR_STATUS_INTERVAL_MS = 250;

constexpr uint8_t ECLAIR_FLAG_PRESSURE_TEST = 0x01;
constexpr uint8_t ECLAIR_FLAG_ALL_GREEN = 0x02;
constexpr uint8_t ECLAIR_FLAG_STARTUP_TEST = 0x04;

constexpr uint8_t ECLAIR_STATUS_LINK_VALID = 0x01;
constexpr uint8_t ECLAIR_STATUS_FIRST_SHOW_ATTEMPTED = 0x02;
constexpr uint8_t ECLAIR_STATUS_OUTPUT_ACTIVE = 0x04;

struct __attribute__((packed)) EclairWireLookSettings {
  uint8_t brightness;
  uint8_t saturation;
  uint8_t speedPercent;
  uint8_t paletteMode;
  uint8_t behaviorMode;
};

struct __attribute__((packed)) EclairStatePacket {
  uint32_t magic;
  uint8_t protocolVersion;
  uint8_t packetType;
  uint16_t packetSize;
  uint32_t sequence;
  uint32_t senderNowMs;
  uint8_t activeZoneMask;
  uint8_t outputMode;
  uint8_t validationLane;
  uint8_t validationColor;
  uint8_t flags;
  uint8_t masterBrightness;
  uint8_t saturationScale;
  uint8_t ambientLevel;
  uint8_t activeLevel;
  uint8_t speedPercent;
  uint16_t animationDurationSeconds;
  uint8_t paletteMode;
  uint8_t behaviorMode;
  uint8_t zoneBrightness[ECLAIR_WIRE_ZONE_COUNT];
  EclairWireLookSettings globalLook[ECLAIR_WIRE_LOOK_COUNT];
  EclairWireLookSettings zoneLook[ECLAIR_WIRE_LOOK_COUNT][ECLAIR_WIRE_ZONE_COUNT];
  uint16_t crc16;
};

struct __attribute__((packed)) EclairStatusPacket {
  uint32_t magic;
  uint8_t protocolVersion;
  uint8_t packetType;
  uint16_t packetSize;
  uint32_t acknowledgedSequence;
  uint32_t receiverNowMs;
  uint32_t renderedFrames;
  uint32_t lastShowMicros;
  uint32_t crcErrorCount;
  uint32_t linkTimeoutCount;
  uint8_t flags;
  uint8_t reserved[3];
  uint16_t crc16;
};

static_assert(sizeof(EclairStatePacket) == 119, "State packet layout changed; update the protocol version and both targets");
static_assert(sizeof(EclairStatusPacket) == 38, "Status packet layout changed; update the protocol version and both targets");

inline uint16_t eclairLinkCrc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

template <typename Packet>
inline void eclairLinkFinalizePacket(Packet &packet) {
  packet.crc16 = eclairLinkCrc16(reinterpret_cast<const uint8_t *>(&packet), sizeof(Packet) - sizeof(packet.crc16));
}

template <typename Packet>
inline bool eclairLinkPacketCrcIsValid(const Packet &packet) {
  return packet.crc16 == eclairLinkCrc16(reinterpret_cast<const uint8_t *>(&packet), sizeof(Packet) - sizeof(packet.crc16));
}

#endif
