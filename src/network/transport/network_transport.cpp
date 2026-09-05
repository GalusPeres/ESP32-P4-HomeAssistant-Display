#include "src/network/transport/network_transport.h"

#include <Network.h>
#include <WiFi.h>

#include "src/core/config/config_manager.h"
#include "src/devices/device.h"
#include "src/network/transport/native_ethernet_backend.h"
#include "src/network/transport/usb_ethernet_backend.h"

NetworkTransportManager networkTransport;

bool NetworkTransportManager::deviceSupportsEthernet() {
  if (Device::kCapabilities.supports_native_ethernet) return true;
#if defined(HOMETILES_USB_ETHERNET_DEV)
  return Device::kCapabilities.supports_usb_host_network;
#else
  return false;
#endif
}

void NetworkTransportManager::begin() {
  if (begun_) return;
  begun_ = true;
  // Use one fixed network mode per boot: Wi-Fi or Ethernet, never both.
  // Concurrent USB-host and ESP-Hosted stacks exhausted scarce internal
  // DMA RAM in the 2026-07-17 8-inch field test. Start Ethernet backends
  // only when the user explicitly selects that mode.
  ethernet_mode_ =
      deviceSupportsEthernet() && configManager.getConfig().ethernet_enabled;
  Network.begin();
  if (ethernet_mode_) {
#if defined(HOMETILES_USB_ETHERNET_DEV)
    if (Device::kCapabilities.supports_usb_host_network) {
      usbEthernetBackend.begin();
    }
#endif
    if (Device::kCapabilities.supports_native_ethernet) {
      nativeEthernetBackend.begin();
    }
    Serial.println("[Network/Transport] Network mode: Ethernet");
  }
  refreshActiveTransport();
}

void NetworkTransportManager::update() {
  if (!begun_) begin();
  const uint32_t now = millis();
  if (wifi_driver_active_.load() &&
      static_cast<int32_t>(now - wifi_poll_at_) >= 0) {
    wifi_poll_at_ = now + 250;
    const bool connected = WiFi.status() == WL_CONNECTED;
    wifi_connected_.store(connected);
    wifi_local_ip_.store(
        connected ? static_cast<uint32_t>(WiFi.localIP()) : 0);
    wifi_gateway_.store(
        connected ? static_cast<uint32_t>(WiFi.gatewayIP()) : 0);
    wifi_dns_.store(
        connected ? static_cast<uint32_t>(WiFi.dnsIP(0)) : 0);
  }
  if (usbEthernetBackend.isStarted() &&
      static_cast<int32_t>(now - usb_poll_at_) >= 0) {
    usb_poll_at_ = now + 250;
    const UsbEthernetSnapshot usb = usbEthernetBackend.snapshot();
    setUsbEthernetState(usb.link_up, usb.has_ip, usb.local_ip, usb.gateway,
                        usb.dns);
  }
  if (nativeEthernetBackend.isStarted() &&
      static_cast<int32_t>(now - native_poll_at_) >= 0) {
    native_poll_at_ = now + 250;
    const NativeEthernetSnapshot native = nativeEthernetBackend.snapshot();
    setNativeEthernetState(native.link_up, native.has_ip, native.local_ip,
                           native.gateway, native.dns);
  }
  refreshActiveTransport();
}

bool NetworkTransportManager::isConnected() const {
  switch (active_kind_.load()) {
    case NetworkTransportKind::Wifi:
      return wifi_connected_.load();
    case NetworkTransportKind::UsbEthernet:
      return usb_.link_up.load() && usb_.has_ip.load();
    case NetworkTransportKind::NativeEthernet:
      return native_.link_up.load() && native_.has_ip.load();
    case NetworkTransportKind::None:
    default:
      return false;
  }
}

bool NetworkTransportManager::isWifiConnected() const {
  return wifi_driver_active_.load() && wifi_connected_.load();
}

bool NetworkTransportManager::isUsbEthernetLinkUp() const {
  return usb_.link_up.load();
}

bool NetworkTransportManager::isNativeEthernetLinkUp() const {
  return native_.link_up.load();
}

bool NetworkTransportManager::isUsbEthernetConnected() const {
  return usb_.link_up.load() && usb_.has_ip.load();
}

bool NetworkTransportManager::isNativeEthernetConnected() const {
  return native_.link_up.load() && native_.has_ip.load();
}

bool NetworkTransportManager::isSdioWifiActive() const {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  // The SDIO RX task consumes DMA memory whenever the hosted WiFi driver is
  // running, even if Ethernet currently owns the default route. Keep the
  // existing MQTT pressure guards active until WiFi has actually stopped.
  return wifi_driver_active_.load();
#else
  return false;
#endif
}

void NetworkTransportManager::setWifiDriverActive(bool active) {
  wifi_driver_active_.store(active);
  if (!active) {
    wifi_connected_.store(false);
    wifi_local_ip_.store(0);
    wifi_gateway_.store(0);
    wifi_dns_.store(0);
  }
}

const char* NetworkTransportManager::activeName() const {
  switch (active_kind_.load()) {
    case NetworkTransportKind::Wifi:
      return "WiFi";
    case NetworkTransportKind::UsbEthernet:
      return "USB Ethernet";
    case NetworkTransportKind::NativeEthernet:
      return "Ethernet";
    case NetworkTransportKind::None:
    default:
      return "Offline";
  }
}

const NetworkTransportManager::EthernetState*
NetworkTransportManager::activeEthernetState() const {
  const NetworkTransportKind active = active_kind_.load();
  if (active == NetworkTransportKind::UsbEthernet) return &usb_;
  if (active == NetworkTransportKind::NativeEthernet) return &native_;
  return nullptr;
}

IPAddress NetworkTransportManager::localIP() const {
  if (const EthernetState* state = activeEthernetState()) {
    return IPAddress(state->local_ip.load());
  }
  return isWifiConnected() ? IPAddress(wifi_local_ip_.load()) : IPAddress();
}

IPAddress NetworkTransportManager::gatewayIP() const {
  if (const EthernetState* state = activeEthernetState()) {
    return IPAddress(state->gateway.load());
  }
  return isWifiConnected() ? IPAddress(wifi_gateway_.load()) : IPAddress();
}

IPAddress NetworkTransportManager::dnsIP(uint8_t index) const {
  if (const EthernetState* state = activeEthernetState()) {
    return index == 0 ? IPAddress(state->dns.load()) : IPAddress();
  }
  return isWifiConnected() && index == 0 ? IPAddress(wifi_dns_.load())
                                         : IPAddress();
}

void NetworkTransportManager::setUsbEthernetState(
    bool link_up, bool has_ip, const IPAddress& local_ip,
    const IPAddress& gateway, const IPAddress& dns) {
  usb_.link_up.store(link_up);
  usb_.has_ip.store(has_ip);
  usb_.local_ip.store(static_cast<uint32_t>(local_ip));
  usb_.gateway.store(static_cast<uint32_t>(gateway));
  usb_.dns.store(static_cast<uint32_t>(dns));
  refreshActiveTransport();
}

void NetworkTransportManager::setNativeEthernetState(
    bool link_up, bool has_ip, const IPAddress& local_ip,
    const IPAddress& gateway, const IPAddress& dns) {
  native_.link_up.store(link_up);
  native_.has_ip.store(has_ip);
  native_.local_ip.store(static_cast<uint32_t>(local_ip));
  native_.gateway.store(static_cast<uint32_t>(gateway));
  native_.dns.store(static_cast<uint32_t>(dns));
  refreshActiveTransport();
}

void NetworkTransportManager::refreshActiveTransport() {
  NetworkTransportKind next = NetworkTransportKind::None;

  // Wired transports win over WiFi. Native Ethernet is preferred when both
  // wired backends are available because it avoids the USB-host overhead.
  if (native_.link_up.load() && native_.has_ip.load()) {
    next = NetworkTransportKind::NativeEthernet;
  } else if (usb_.link_up.load() && usb_.has_ip.load()) {
    next = NetworkTransportKind::UsbEthernet;
  } else if (wifi_connected_.load()) {
    next = NetworkTransportKind::Wifi;
  }

  const NetworkTransportKind previous = active_kind_.load();
  if (next == previous) return;
  active_kind_.store(next);
  const uint32_t generation = generation_.fetch_add(1) + 1;
  Serial.printf("[Network/Transport] %u -> %u (%s), generation=%u\n",
                static_cast<unsigned>(previous),
                static_cast<unsigned>(next),
                activeName(),
                static_cast<unsigned>(generation));
}
