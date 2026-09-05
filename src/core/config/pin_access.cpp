#include "src/core/config/pin_access.h"

#include <esp_random.h>
#include <mbedtls/sha256.h>

#include <string.h>

namespace pin_access {

const char kRecoveryPin[] = "466384537";  // HOMETILES on a phone keypad.

namespace {

constexpr char kHashDomain[] = "HomeTiles-PIN-v1";

bool allDigits(const char* value, size_t length) {
  if (!value) return false;
  for (size_t i = 0; i < length; ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
  }
  return true;
}

bool constantTimeEqual(const uint8_t* a, const uint8_t* b, size_t size) {
  if (!a || !b) return false;
  uint8_t difference = 0;
  for (size_t i = 0; i < size; ++i) difference |= a[i] ^ b[i];
  return difference == 0;
}

bool calculateHash(const char* pin, const uint8_t salt[kSaltSize],
                   uint8_t hash[kHashSize]) {
  if (!pin || !salt || !hash) return false;
  const size_t pin_length = strlen(pin);
  if (pin_length == 0 || pin_length > kInputMaxDigits ||
      !allDigits(pin, pin_length)) {
    return false;
  }

  uint8_t input[sizeof(kHashDomain) - 1 + kSaltSize + kInputMaxDigits] = {};
  size_t offset = 0;
  memcpy(input + offset, kHashDomain, sizeof(kHashDomain) - 1);
  offset += sizeof(kHashDomain) - 1;
  memcpy(input + offset, salt, kSaltSize);
  offset += kSaltSize;
  memcpy(input + offset, pin, pin_length);
  offset += pin_length;

  const int result = mbedtls_sha256(input, offset, hash, 0);
  secureClear(input, sizeof(input));
  return result == 0;
}

}  // namespace

void secureClear(void* data, size_t size) {
  if (!data) return;
  volatile uint8_t* bytes = static_cast<volatile uint8_t*>(data);
  while (size--) *bytes++ = 0;
}

bool isValidUserPin(const String& pin) {
  const size_t length = pin.length();
  return length >= kUserPinMinDigits && length <= kUserPinMaxDigits &&
         allDigits(pin.c_str(), length);
}

bool makeCredential(const String& pin, uint8_t salt[kSaltSize],
                    uint8_t hash[kHashSize]) {
  if (!isValidUserPin(pin) || !salt || !hash) return false;
  esp_fill_random(salt, kSaltSize);
  if (!calculateHash(pin.c_str(), salt, hash)) {
    clearCredential(salt, hash);
    return false;
  }
  return true;
}

bool verifyCredential(const char* pin, const uint8_t salt[kSaltSize],
                      const uint8_t hash[kHashSize]) {
  if (!credentialIsSet(salt, hash)) return false;
  uint8_t candidate[kHashSize] = {};
  if (!calculateHash(pin, salt, candidate)) return false;
  const bool matches = constantTimeEqual(candidate, hash, sizeof(candidate));
  secureClear(candidate, sizeof(candidate));
  return matches;
}

bool isRecoveryPin(const char* pin) {
  if (!pin) return false;
  const size_t expected_length = sizeof(kRecoveryPin) - 1;
  const size_t actual_length = strlen(pin);
  uint8_t difference = static_cast<uint8_t>(actual_length ^ expected_length);
  for (size_t i = 0; i < expected_length; ++i) {
    const char actual = i < actual_length ? pin[i] : '\0';
    difference |= static_cast<uint8_t>(actual ^ kRecoveryPin[i]);
  }
  return difference == 0;
}

bool credentialIsSet(const uint8_t salt[kSaltSize],
                     const uint8_t hash[kHashSize]) {
  if (!salt || !hash) return false;
  uint8_t salt_combined = 0;
  uint8_t hash_combined = 0;
  for (size_t i = 0; i < kSaltSize; ++i) salt_combined |= salt[i];
  for (size_t i = 0; i < kHashSize; ++i) hash_combined |= hash[i];
  return salt_combined != 0 && hash_combined != 0;
}

void clearCredential(uint8_t salt[kSaltSize], uint8_t hash[kHashSize]) {
  secureClear(salt, kSaltSize);
  secureClear(hash, kHashSize);
}

}  // namespace pin_access
