#include "src/network/transport/native_ethernet_backend.h"

#include "src/core/config/config_manager.h"
#include "src/devices/device.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4) && CONFIG_ETH_ENABLED
#include <ETH.h>
#include <atomic>

namespace {

// ESP32-P4-86-Panel-ETH-2RO:
// IP101GRI, PHY address 1, external 50 MHz RMII clock.
constexpr int kPhyAddress = 1;
constexpr int kMdcPin = 31;
constexpr int kMdioPin = 52;
constexpr int kPhyResetPin = 51;
constexpr int kRoutePriority = 160;

std::atomic<bool> g_started{false};
std::atomic<bool> g_attempted{false};

bool parseConfiguredIp(const char* value, IPAddress& out) {
  if (!value || !value[0]) return false;
  String text = value;
  text.trim();
  return text.length() && out.fromString(text);
}

void applyAddressing() {
  const DeviceConfig& cfg = configManager.getConfig();
  if (!cfg.wifi_static_enabled) return;

  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;

  const bool has_ip = parseConfiguredIp(cfg.wifi_static_ip, ip);
  const bool has_gateway = parseConfiguredIp(cfg.wifi_gateway, gateway);
  const bool has_subnet = parseConfiguredIp(cfg.wifi_subnet, subnet);
  const bool has_dns = parseConfiguredIp(cfg.wifi_dns, dns);
  if (!has_ip && !has_gateway && !has_subnet && !has_dns) return;

  if (!has_ip || !has_gateway || !has_subnet) {
    Serial.println(
        "[ETH] Incomplete static IP configuration; using DHCP");
    ETH.config(IPAddress(), IPAddress(), IPAddress());
    return;
  }
  if (!has_dns) dns = gateway;
  if (ETH.config(ip, gateway, subnet, dns)) {
    Serial.printf("[ETH] Static IP %s / GW %s / MASK %s / DNS %s\n",
                  ip.toString().c_str(), gateway.toString().c_str(),
                  subnet.toString().c_str(), dns.toString().c_str());
  } else {
    Serial.println("[ETH] Static IP configuration failed; using DHCP");
    ETH.config(IPAddress(), IPAddress(), IPAddress());
  }
}

}  // namespace
#endif

NativeEthernetBackend nativeEthernetBackend;

bool NativeEthernetBackend::begin() {
#if defined(CONFIG_IDF_TARGET_ESP32P4) && CONFIG_ETH_ENABLED
  if (!Device::kCapabilities.supports_native_ethernet) return false;
  if (g_attempted.exchange(true)) return g_started.load();

  Serial.println(
      "[ETH] Probing ESP32-P4-86 IP101GRI on the shared B4 firmware");
  const bool ok = ETH.begin(ETH_PHY_IP101, kPhyAddress, kMdcPin, kMdioPin,
                            kPhyResetPin, EMAC_CLK_EXT_IN);
  if (!ok) {
    Serial.println(
        "[ETH] Native PHY not present; continuing with USB Ethernet/WiFi");
    return false;
  }

  applyAddressing();
  ETH.setRoutePrio(kRoutePriority);
  g_started.store(true);
  Serial.println("[ETH] Native IP101GRI backend started");
  return true;
#else
  return false;
#endif
}

bool NativeEthernetBackend::isStarted() const {
#if defined(CONFIG_IDF_TARGET_ESP32P4) && CONFIG_ETH_ENABLED
  return g_started.load();
#else
  return false;
#endif
}

NativeEthernetSnapshot NativeEthernetBackend::snapshot() const {
  NativeEthernetSnapshot result;
#if defined(CONFIG_IDF_TARGET_ESP32P4) && CONFIG_ETH_ENABLED
  if (!g_started.load()) return result;
  result.link_up = ETH.connected();
  result.has_ip = result.link_up && ETH.hasIP();
  if (result.has_ip) {
    result.local_ip = ETH.localIP();
    result.gateway = ETH.gatewayIP();
    result.dns = ETH.dnsIP();
  }
#endif
  return result;
}
