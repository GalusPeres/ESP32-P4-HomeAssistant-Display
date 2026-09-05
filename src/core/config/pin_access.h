#pragma once

#include <Arduino.h>

#include <stddef.h>
#include <stdint.h>

namespace pin_access {

constexpr size_t kSaltSize = 16;
constexpr size_t kHashSize = 32;
constexpr size_t kUserPinMinDigits = 4;
constexpr size_t kUserPinMaxDigits = 8;
constexpr size_t kInputMaxDigits = 9;

// Public recovery code for parental-control lockouts. This is deliberately
// documented and is not intended to provide security against an administrator.
extern const char kRecoveryPin[];

bool isValidUserPin(const String& pin);
bool makeCredential(const String& pin, uint8_t salt[kSaltSize],
                    uint8_t hash[kHashSize]);
bool verifyCredential(const char* pin, const uint8_t salt[kSaltSize],
                      const uint8_t hash[kHashSize]);
bool isRecoveryPin(const char* pin);
bool credentialIsSet(const uint8_t salt[kSaltSize],
                     const uint8_t hash[kHashSize]);
void clearCredential(uint8_t salt[kSaltSize], uint8_t hash[kHashSize]);
void secureClear(void* data, size_t size);

}  // namespace pin_access
