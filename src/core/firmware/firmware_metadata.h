#pragma once

#include <Arduino.h>

namespace firmware_meta {

constexpr uint32_t kDeviceDescriptorMagic = 0x44565034u;  // "DVP4"
constexpr uint32_t kSiliconRevisionDescriptorMagic = 0x53525634u;
constexpr size_t kProjectKeyMaxLen = 32;
constexpr size_t kDeviceKeyMaxLen = 32;
constexpr size_t kDisplayNameMaxLen = 32;
constexpr size_t kSiliconVariantMaxLen = 16;
constexpr size_t kDeviceDescriptorImageOffset = 24 + 8 + 256;

struct DeviceDescriptor {
  uint32_t magic_word;
  char project_key[kProjectKeyMaxLen];
  char device_key[kDeviceKeyMaxLen];
  char display_name[kDisplayNameMaxLen];
} __attribute__((packed));

struct SiliconRevisionDescriptor {
  uint32_t magic_word;
  uint16_t minimum_revision;
  uint16_t maximum_revision;
  char variant[kSiliconVariantMaxLen];
} __attribute__((packed));

constexpr size_t kSiliconRevisionDescriptorImageOffset =
    kDeviceDescriptorImageOffset + sizeof(DeviceDescriptor);
constexpr size_t kDeviceDescriptorImageBytes =
    kSiliconRevisionDescriptorImageOffset + sizeof(SiliconRevisionDescriptor);

const DeviceDescriptor& currentDeviceDescriptor();
const SiliconRevisionDescriptor& currentSiliconRevisionDescriptor();
const char* currentProjectKey();
const char* currentDeviceKey();
const char* currentDisplayName();
const char* currentSiliconVariant();
bool matchesCurrentDeviceKey(const char* incoming_device_key);
bool matchesCurrentSiliconVariant(const char* incoming_variant);
bool matchesCurrentSiliconRevisionRange(
    uint16_t incoming_minimum_revision,
    uint16_t incoming_maximum_revision);
const char* expectedDeviceDisplayName();
bool parseDeviceDescriptorFromImage(const uint8_t* image_data, size_t image_len, DeviceDescriptor& out);
bool parseSiliconRevisionDescriptorFromImage(
    const uint8_t* image_data,
    size_t image_len,
    SiliconRevisionDescriptor& out);
bool imageMatchesCurrentSiliconVariant(
    const uint8_t* image_data,
    size_t image_len,
    bool* accepted_legacy_descriptor = nullptr);

}  // namespace firmware_meta
