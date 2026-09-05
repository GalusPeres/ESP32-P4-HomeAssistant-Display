#pragma once

#include <Arduino.h>
#include <IPAddress.h>

struct UsbEthernetSnapshot {
  bool host_ready = false;
  int enumerated_devices = 0;
  bool adapter_attached = false;
  bool link_up = false;
  bool has_ip = false;
  IPAddress local_ip;
  IPAddress gateway;
  IPAddress dns;
};

// Optional USB-host network backend. The shared transport facade owns the
// policy; this class only exposes the current link/IP state.
class UsbEthernetBackend {
public:
  bool begin();
  bool isStarted() const;
  UsbEthernetSnapshot snapshot() const;
};

extern UsbEthernetBackend usbEthernetBackend;
