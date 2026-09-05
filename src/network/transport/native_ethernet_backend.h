#pragma once

#include <Arduino.h>
#include <IPAddress.h>

struct NativeEthernetSnapshot {
  bool link_up = false;
  bool has_ip = false;
  IPAddress local_ip;
  IPAddress gateway;
  IPAddress dns;
};

class NativeEthernetBackend {
public:
  bool begin();
  bool isStarted() const;
  NativeEthernetSnapshot snapshot() const;
};

extern NativeEthernetBackend nativeEthernetBackend;
