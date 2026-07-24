#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <atomic>

// Die Waveshare Touch-LCD-X-Geraete haben nur USB-Host-Ethernet. Deshalb muss
// der RTL8156-Weg auch in ihren CI/Release-Builds enthalten sein. Lokale
// Dev-Builds behalten ihn fuer Hardwaretests auf allen USB-Host-faehigen
// Profilen.
#if !defined(HOMETILES_CI_TARGET) || defined(DEVICE_WAVESHARE_TOUCH_LCD_X)
#define HOMETILES_USB_ETHERNET_DEV 1
#endif

// Shared network view for every HomeTiles device.
//
// Application code must use this facade for connectivity, addressing and
// transport-sensitive workarounds. Backend-specific operations such as WiFi
// scans, SoftAP configuration or PHY setup remain inside their backend.
enum class NetworkTransportKind : uint8_t {
  None = 0,
  Wifi,
  UsbEthernet,
  NativeEthernet,
};

class NetworkTransportManager {
public:
  void begin();
  void update();

  // Kann dieser Build auf diesem Geraet ueberhaupt Ethernet? Natives Ethernet
  // zaehlt immer; beim 8-Zoll-Geraet zaehlt auch USB-Ethernet im Release.
  // Steuert auch, ob der Modus-Schalter in Settings/Web-Admin erscheint.
  static bool deviceSupportsEthernet();

  // Fester Netzwerkmodus dieser Boot-Session (in begin() aus der Config
  // uebernommen): true = Ethernet, WLAN/ESP-Hosted startet nie.
  bool isEthernetMode() const { return ethernet_mode_; }

  bool isConnected() const;
  bool isWifiConnected() const;
  bool isWifiDriverActive() const { return wifi_driver_active_.load(); }
  bool isUsbEthernetLinkUp() const;
  bool isNativeEthernetLinkUp() const;
  bool isUsbEthernetConnected() const;
  bool isNativeEthernetConnected() const;
  bool isSdioWifiActive() const;
  void setWifiDriverActive(bool active);

  NetworkTransportKind activeKind() const { return active_kind_.load(); }
  const char* activeName() const;
  uint32_t generation() const { return generation_.load(); }

  IPAddress localIP() const;
  IPAddress gatewayIP() const;
  IPAddress dnsIP(uint8_t index = 0) const;

  // Backend hooks. A backend reports link/IP state here; the facade selects
  // the highest-priority usable transport and exposes it to the application.
  void setUsbEthernetState(bool link_up, bool has_ip, const IPAddress& local_ip,
                           const IPAddress& gateway, const IPAddress& dns);
  void setNativeEthernetState(bool link_up, bool has_ip,
                              const IPAddress& local_ip,
                              const IPAddress& gateway,
                              const IPAddress& dns);

private:
  struct EthernetState {
    std::atomic<bool> link_up{false};
    std::atomic<bool> has_ip{false};
    std::atomic<uint32_t> local_ip{0};
    std::atomic<uint32_t> gateway{0};
    std::atomic<uint32_t> dns{0};
  };

  void refreshActiveTransport();
  const EthernetState* activeEthernetState() const;

  bool begun_ = false;
  bool ethernet_mode_ = false;
  uint32_t wifi_poll_at_ = 0;
  uint32_t usb_poll_at_ = 0;
  uint32_t native_poll_at_ = 0;
  std::atomic<NetworkTransportKind> active_kind_{NetworkTransportKind::None};
  std::atomic<uint32_t> generation_{0};
  std::atomic<bool> wifi_driver_active_{false};
  std::atomic<bool> wifi_connected_{false};
  std::atomic<uint32_t> wifi_local_ip_{0};
  std::atomic<uint32_t> wifi_gateway_{0};
  std::atomic<uint32_t> wifi_dns_{0};
  EthernetState usb_;
  EthernetState native_;
};

extern NetworkTransportManager networkTransport;
