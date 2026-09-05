/*
 * Portions of the RTL8156 register setup are derived from FreeBSD's ure(4)
 * driver:
 *
 * Copyright (c) 2015-2016 Kevin Lo
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "src/network/transport/usb_ethernet_backend.h"

#include "src/core/config/config_manager.h"
#include "src/devices/device.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)

#include <Network.h>
#include <atomic>
#include <cstring>

#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_netif_defaults.h>
#include <esp_netif_net_stack.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <usb/usb_helpers.h>
#include <usb/usb_host.h>

namespace {

constexpr uint16_t kRealtekVid = 0x0bda;
constexpr uint16_t kRtl8156Pid = 0x8156;
constexpr uint8_t kRealtekRequest = USB_B_REQUEST_SET_ADDRESS;

constexpr size_t kRxTransferCount = 2;
constexpr size_t kRxBufferSize = 16 * 1024;
constexpr size_t kTxBufferSize = 2048;
constexpr size_t kControlBufferSize = 16;
constexpr size_t kMaxEthernetFrame = 1536;
constexpr uint32_t kLinkPollMs = 500;
constexpr uint32_t kLinkDhcpDelayMs = 750;
constexpr uint32_t kUsbDiagnosticsMs = 3000;
constexpr uint32_t kRxUpgradeRetryMs = 10000;

constexpr uint16_t kByteEnableDword = 0x00ff;
constexpr uint16_t kByteEnableWord = 0x0033;
constexpr uint16_t kByteEnableByte = 0x0011;
constexpr uint16_t kByteEnableSixBytes = 0x003f;
constexpr uint16_t kMcuPla = 0x0100;
constexpr uint16_t kMcuUsb = 0x0000;

constexpr uint16_t kPlaIdr = 0xc000;
constexpr uint16_t kPlaRcr = 0xc010;
constexpr uint16_t kPlaRms = 0xc016;
constexpr uint16_t kPlaFmc = 0xc0b4;
constexpr uint16_t kPlaTeredoCfg = 0xc0bc;
constexpr uint16_t kPlaMar0 = 0xcd00;
constexpr uint16_t kPlaMar4 = 0xcd04;
constexpr uint16_t kPlaBackup = 0xd000;
constexpr uint16_t kPlaTeredoTimer = 0xd2cc;
constexpr uint16_t kPlaRealwowTimer = 0xd2e8;
constexpr uint16_t kPlaSuspendFlag = 0xd38a;
constexpr uint16_t kPlaIndicateFlag = 0xd38c;
constexpr uint16_t kPlaExtraStatus = 0xd398;
constexpr uint16_t kPlaBootCtrl = 0xe004;

bool parseConfiguredIp(const char* value, IPAddress& out) {
  if (!value || !value[0]) return false;
  String text = value;
  text.trim();
  return text.length() && out.fromString(text);
}

bool applyConfiguredAddressing(esp_netif_t* netif) {
  if (!netif) return false;

  const DeviceConfig& cfg = configManager.getConfig();
  if (!cfg.wifi_static_enabled) return false;

  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;
  const bool has_ip = parseConfiguredIp(cfg.wifi_static_ip, ip);
  const bool has_gateway = parseConfiguredIp(cfg.wifi_gateway, gateway);
  const bool has_subnet = parseConfiguredIp(cfg.wifi_subnet, subnet);
  const bool has_dns = parseConfiguredIp(cfg.wifi_dns, dns);
  if (!has_ip && !has_gateway && !has_subnet && !has_dns) return false;

  if (!has_ip || !has_gateway || !has_subnet) {
    Serial.println(
        "[USB-ETH] Incomplete static IP configuration; using DHCP");
    esp_netif_dhcpc_start(netif);
    return false;
  }
  if (!has_dns) dns = gateway;

  const esp_err_t stop_err = esp_netif_dhcpc_stop(netif);
  if (stop_err != ESP_OK &&
      stop_err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    Serial.printf("[USB-ETH] Cannot stop DHCP: %s\n",
                  esp_err_to_name(stop_err));
    return false;
  }

  esp_netif_ip_info_t ip_info = {};
  ip_info.ip.addr = static_cast<uint32_t>(ip);
  ip_info.gw.addr = static_cast<uint32_t>(gateway);
  ip_info.netmask.addr = static_cast<uint32_t>(subnet);
  const esp_err_t ip_err = esp_netif_set_ip_info(netif, &ip_info);
  if (ip_err != ESP_OK) {
    Serial.printf("[USB-ETH] Cannot configure static IP: %s\n",
                  esp_err_to_name(ip_err));
    esp_netif_dhcpc_start(netif);
    return false;
  }

  esp_netif_dns_info_t dns_info = {};
  dns_info.ip.type = ESP_IPADDR_TYPE_V4;
  dns_info.ip.u_addr.ip4.addr = static_cast<uint32_t>(dns);
  const esp_err_t dns_err =
      esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
  if (dns_err != ESP_OK) {
    Serial.printf("[USB-ETH] Cannot configure DNS: %s\n",
                  esp_err_to_name(dns_err));
  }
  Serial.printf("[USB-ETH] Static IP %s / GW %s / MASK %s / DNS %s\n",
                ip.toString().c_str(), gateway.toString().c_str(),
                subnet.toString().c_str(), dns.toString().c_str());
  return true;
}
constexpr uint16_t kPlaMacPwrCtrl = 0xe0c0;
constexpr uint16_t kPlaMacPwrCtrl2 = 0xe0ca;
constexpr uint16_t kPlaMacPwrCtrl3 = 0xe0cc;
constexpr uint16_t kPlaMacPwrCtrl4 = 0xe0ce;
constexpr uint16_t kPlaWdt6Ctrl = 0xe428;
constexpr uint16_t kPlaTcr1 = 0xe612;
constexpr uint16_t kPlaMtps = 0xe615;
constexpr uint16_t kPlaTxfifoCtrl = 0xe618;
constexpr uint16_t kPlaRstTally = 0xe800;
constexpr uint16_t kPlaCr = 0xe813;
constexpr uint16_t kPlaCrwecr = 0xe81c;
constexpr uint16_t kPlaConfig34 = 0xe820;
constexpr uint16_t kPlaOobCtrl = 0xe84f;
constexpr uint16_t kPlaCpcr = 0xe854;
constexpr uint16_t kPlaMisc1 = 0xe85a;
constexpr uint16_t kPlaOcpGphyBase = 0xe86c;
constexpr uint16_t kPlaSffStatus7 = 0xe8de;
constexpr uint16_t kPlaPhystatus = 0xe908;

constexpr uint16_t kUsbU2p3Ctrl = 0xb460;
constexpr uint16_t kUsbMscTimer = 0xcbfc;
constexpr uint16_t kUsbLpmConfig = 0xcfd8;
constexpr uint16_t kUsbFwCtrl = 0xd334;
constexpr uint16_t kUsbFcTimer = 0xd340;
constexpr uint16_t kUsbUsbCtrl = 0xd406;
constexpr uint16_t kUsbRxBufThreshold = 0xd40c;
constexpr uint16_t kUsbRxEarlyAgg = 0xd42c;
constexpr uint16_t kUsbRxEarlySize = 0xd42e;
constexpr uint16_t kUsbRxExtraAggTimer = 0xd432;
constexpr uint16_t kUsbUptRxdmaOwn = 0xd437;
constexpr uint16_t kUsbBmuReset = 0xd4b0;
constexpr uint16_t kUsbU1u2Timer = 0xd4da;
constexpr uint16_t kUsbFwTask = 0xd4e8;
constexpr uint16_t kUsbPowerCut = 0xd80a;
constexpr uint16_t kUsbMisc0 = 0xd81a;

constexpr uint16_t kOcpAldpsConfig = 0x2010;
constexpr uint16_t kOcpBaseMii = 0xa400;
constexpr uint16_t kOcpPhyStatus = 0xa420;
constexpr uint16_t kOcpPowerCfg = 0xa430;

constexpr uint16_t kVersionMask = 0x7cf0;
constexpr uint16_t kVersion8156A1 = 0x7020;
constexpr uint16_t kVersion8156A2 = 0x7030;
constexpr uint16_t kVersion8156B1 = 0x7400;
constexpr uint16_t kVersion8156B2 = 0x7410;

constexpr uint32_t kRcrAcceptPhysical = 0x00000002;
constexpr uint32_t kRcrAcceptMulticast = 0x00000004;
constexpr uint32_t kRcrAcceptBroadcast = 0x00000008;
constexpr uint32_t kRcrAcceptAll =
    0x00000001 | kRcrAcceptPhysical | kRcrAcceptMulticast |
    kRcrAcceptBroadcast;
constexpr uint16_t kFmcMcuEnable = 0x0001;
constexpr uint8_t kCrReset = 0x10;
constexpr uint8_t kCrRxEnable = 0x08;
constexpr uint8_t kCrTxEnable = 0x04;
constexpr uint8_t kCrwecrNormal = 0x00;
constexpr uint8_t kCrwecrConfig = 0xc0;
constexpr uint16_t kRxdyGatedEnable = 0x0008;
constexpr uint16_t kCpcrRxVlan = 0x0040;
constexpr uint16_t kMcuBorrowEnable = 0x4000;
constexpr uint16_t kWdt6SetMode = 0x0010;
constexpr uint16_t kAutoloadDone = 0x0002;
constexpr uint8_t kUpcomingRuntimeD3 = 0x01;
constexpr uint8_t kLinkChangeEvent = 0x01;
constexpr uint16_t kLinkChangeFlag = 0x0100;
constexpr uint16_t kCurrentLinkOk = 0x8000;
constexpr uint16_t kPollLinkChange = 0x0001;
constexpr uint16_t kPhystatusLink = 0x0002;
constexpr uint16_t kPhystatus100 = 0x0008;
constexpr uint16_t kPhystatus1000 = 0x0010;
constexpr uint16_t kPhystatus2500 = 0x0400;
constexpr uint16_t kMacClockSpeedDownEnable = 0x8000;
constexpr uint16_t kPlaMcuSpeedDownEnable = 0x4000;
constexpr uint16_t kLpmU1u2Enable = 0x0001;
constexpr uint8_t kBmuResetIn = 0x01;
constexpr uint8_t kBmuResetOut = 0x02;
constexpr uint8_t kOwnUpdate = 0x01;
constexpr uint8_t kOwnClear = 0x02;
constexpr uint16_t kFcPatchTask = 0x0001;
constexpr uint16_t kFlowControlPatchOption = 0x0001;
constexpr uint16_t kControlTimerEnable = 0x8000;
constexpr uint16_t kRxAggregationDisable = 0x0010;
constexpr uint16_t kRxZeroEnable = 0x0080;
constexpr uint16_t kU2p3Enable = 0x0001;
constexpr uint16_t kPowerEnable = 0x0001;
constexpr uint8_t kUpsEnable = 0x10;
constexpr uint8_t kUpsPrewake = 0x20;
constexpr uint16_t kPowerCutStatus = 0x0001;
constexpr uint16_t kEnablePowerDown = 0x0200;
constexpr uint16_t kLinkEnable = 0x0100;
constexpr uint16_t kDisableSdsave = 0x0010;
constexpr uint16_t kPhyStatusMask = 0x0007;
constexpr uint16_t kPhyStatusExternalInit = 2;
constexpr uint16_t kPhyStatusLanOn = 3;
constexpr uint16_t kPhyStatusPowerDown = 5;
constexpr uint16_t kEnableAldps = 0x0004;
constexpr uint16_t kLinkOffWakeEnable = 0x0008;
constexpr uint16_t kRxFifoThreshold = 0x0008;
constexpr uint32_t kUsbRxThreshold = 0x00600400;
constexpr uint16_t kTxFifoThreshold = 0x0008;
constexpr uint8_t kMtpsJumbo = 192;
constexpr uint16_t kTallyReset = 0x0001;

constexpr uint16_t kMiiBmcr = 0;
constexpr uint16_t kMiiAnar = 4;
constexpr uint16_t kMii1000Control = 9;
constexpr uint16_t kBmcrPowerDown = 0x0800;
constexpr uint16_t kBmcrAutoEnable = 0x1000;
constexpr uint16_t kBmcrRestartAuto = 0x0200;
constexpr uint16_t kAnar10Half = 0x0020;
constexpr uint16_t kAnar10Full = 0x0040;
constexpr uint16_t kAnar100Half = 0x0080;
constexpr uint16_t kAnar100Full = 0x0100;
constexpr uint16_t kAnarPause = 0x0400;
constexpr uint16_t kAnarAsymPause = 0x0800;
constexpr uint16_t kAdvertise1000Half = 0x0100;
constexpr uint16_t kAdvertise1000Full = 0x0200;
constexpr uint16_t kAdvertise2500Full = 0x0080;

constexpr uint32_t kTxFirstSegment = (1UL << 31);
constexpr uint32_t kTxLastSegment = (1UL << 30);
constexpr uint32_t kRxLengthMask = 0x7fff;

struct __attribute__((packed)) RtlRxHeader {
  uint32_t packet_length;
  uint32_t checksum;
  uint32_t misc;
  uint32_t reserved2;
  uint32_t reserved3;
  uint32_t reserved4;
};

struct __attribute__((packed)) RtlTxHeader {
  uint32_t packet_length;
  uint32_t checksum;
};

static_assert(sizeof(RtlRxHeader) == 24, "Unexpected RTL8156 RX header");
static_assert(sizeof(RtlTxHeader) == 8, "Unexpected RTL8156 TX header");

struct TxFrame {
  uint16_t length;
  uint8_t data[];
};

enum class DriverEventType : uint8_t {
  NewDevice,
  DeviceGone,
};

struct DriverEvent {
  DriverEventType type;
  uint8_t address;
  usb_device_handle_t device;
};

class Rtl8156UsbDriver;

struct RxContext {
  Rtl8156UsbDriver* owner = nullptr;
  uint8_t index = 0;
};

struct NetifGlue {
  esp_netif_driver_base_t base = {};
  Rtl8156UsbDriver* owner = nullptr;
};

class Rtl8156UsbDriver {
public:
  bool begin() {
    if (begun_.exchange(true)) return started_.load();

    if (!Network.begin()) {
      Serial.println("[USB-ETH] Network stack initialization failed");
      return false;
    }

    event_queue_ = xQueueCreate(8, sizeof(DriverEvent));
    tx_queue_ = xQueueCreate(12, sizeof(TxFrame*));
    host_ready_ = xSemaphoreCreateBinary();
    client_ready_ = xSemaphoreCreateBinary();
    control_done_ = xSemaphoreCreateBinary();
    if (!event_queue_ || !tx_queue_ || !host_ready_ || !client_ready_ ||
        !control_done_) {
      Serial.println("[USB-ETH] FreeRTOS resource allocation failed");
      return false;
    }

    static esp_netif_inherent_config_t usb_eth_base =
        ESP_NETIF_INHERENT_DEFAULT_ETH();
    usb_eth_base.if_key = "USB_ETH_DEF";
    usb_eth_base.if_desc = "usb_eth";
    usb_eth_base.route_prio = 150;

    const esp_netif_config_t netif_config = {
        &usb_eth_base,
        nullptr,
        ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };
    netif_ = esp_netif_new(&netif_config);
    if (!netif_) {
      Serial.println("[USB-ETH] esp_netif_new failed");
      return false;
    }

    glue_.owner = this;
    glue_.base.post_attach = &Rtl8156UsbDriver::netifPostAttach;
    const esp_err_t attach_err = esp_netif_attach(netif_, &glue_.base);
    if (attach_err != ESP_OK) {
      Serial.printf("[USB-ETH] esp_netif_attach failed: %s\n",
                    esp_err_to_name(attach_err));
      return false;
    }

    if (xTaskCreate(&Rtl8156UsbDriver::hostTaskEntry, "usb_eth_host", 4096,
                    this, 2, &host_task_) != pdPASS ||
        xTaskCreate(&Rtl8156UsbDriver::clientTaskEntry, "usb_eth_client", 4096,
                    this, 3, &client_task_) != pdPASS ||
        xTaskCreate(&Rtl8156UsbDriver::workerTaskEntry, "usb_eth_worker", 7168,
                    this, 2, &worker_task_) != pdPASS) {
      Serial.println("[USB-ETH] Task creation failed");
      return false;
    }

    started_.store(true);
    Serial.println(
        "[USB-ETH] USB host enabled; waiting for Realtek 0BDA:8156");
    return true;
  }

  bool isStarted() const {
    return started_.load();
  }

  UsbEthernetSnapshot snapshot() const {
    UsbEthernetSnapshot result;
    result.host_ready = host_installed_.load();
    result.enumerated_devices = enumerated_devices_.load();
    result.adapter_attached = attached_.load();
    result.link_up = link_up_.load();
    if (!result.link_up || !netif_) return result;

    esp_netif_ip_info_t ip_info = {};
    if (esp_netif_get_ip_info(netif_, &ip_info) != ESP_OK ||
        ip_info.ip.addr == 0) {
      return result;
    }

    result.has_ip = true;
    result.local_ip = IPAddress(ip_info.ip.addr);
    result.gateway = IPAddress(ip_info.gw.addr);

    esp_netif_dns_info_t dns_info = {};
    if (esp_netif_get_dns_info(netif_, ESP_NETIF_DNS_MAIN, &dns_info) ==
            ESP_OK &&
        dns_info.ip.type == ESP_IPADDR_TYPE_V4) {
      result.dns = IPAddress(dns_info.ip.u_addr.ip4.addr);
    }
    return result;
  }

private:
  static esp_err_t netifPostAttach(esp_netif_t* netif,
                                   esp_netif_iodriver_handle handle) {
    auto* glue = static_cast<NetifGlue*>(handle);
    if (!glue || !glue->owner) return ESP_ERR_INVALID_ARG;

    glue->base.netif = netif;
    esp_netif_driver_ifconfig_t driver_config = {};
    driver_config.handle = glue->owner;
    driver_config.transmit = &Rtl8156UsbDriver::netifTransmit;
    driver_config.transmit_wrap = nullptr;
    driver_config.driver_free_rx_buffer =
        &Rtl8156UsbDriver::netifFreeRxBuffer;
    driver_config.driver_set_mac_filter = nullptr;
    return esp_netif_set_driver_config(netif, &driver_config);
  }

  static void netifFreeRxBuffer(void*, void* buffer) {
    heap_caps_free(buffer);
  }

  static esp_err_t netifTransmit(void* handle, void* buffer, size_t length) {
    auto* self = static_cast<Rtl8156UsbDriver*>(handle);
    if (!self || !buffer || length == 0 || length > kMaxEthernetFrame) {
      return ESP_ERR_INVALID_ARG;
    }
    if (!self->attached_.load() || !self->link_up_.load() ||
        !self->data_path_active_.load() || !self->tx_queue_) {
      return ESP_ERR_INVALID_STATE;
    }

    auto* frame = static_cast<TxFrame*>(
        heap_caps_malloc(sizeof(TxFrame) + length, MALLOC_CAP_8BIT));
    if (!frame) return ESP_ERR_NO_MEM;
    frame->length = static_cast<uint16_t>(length);
    memcpy(frame->data, buffer, length);

    if (xQueueSend(self->tx_queue_, &frame, 0) != pdTRUE) {
      heap_caps_free(frame);
      return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
  }

  static void hostTaskEntry(void* arg) {
    static_cast<Rtl8156UsbDriver*>(arg)->hostTask();
  }

  static void clientTaskEntry(void* arg) {
    static_cast<Rtl8156UsbDriver*>(arg)->clientTask();
  }

  static void workerTaskEntry(void* arg) {
    static_cast<Rtl8156UsbDriver*>(arg)->workerTask();
  }

  void hostTask() {
    usb_host_config_t host_config = {};
    host_config.skip_phy_setup = false;
    host_config.root_port_unpowered = false;
    host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;
    host_config.enum_filter_cb = nullptr;
    // Match Waveshare's BSP and let ESP-IDF select the default peripheral.
    // On ESP32-P4 the documented default is the high-speed controller.
    host_config.peripheral_map = 0;

    const esp_err_t err = usb_host_install(&host_config);
    host_installed_.store(err == ESP_OK);
    if (err != ESP_OK) {
      Serial.printf("[USB-ETH] usb_host_install failed: %s\n",
                    esp_err_to_name(err));
      xSemaphoreGive(host_ready_);
      vTaskDelete(nullptr);
      return;
    }

    Serial.println("[USB-ETH] ESP32-P4 high-speed USB host ready");
    xSemaphoreGive(host_ready_);
    int previous_device_count = -1;
    for (;;) {
      uint32_t flags = 0;
      const esp_err_t event_err =
          usb_host_lib_handle_events(pdMS_TO_TICKS(kUsbDiagnosticsMs), &flags);
      if (event_err != ESP_OK && event_err != ESP_ERR_TIMEOUT) {
        Serial.printf("[USB-ETH] Host event error: %s\n",
                      esp_err_to_name(event_err));
        vTaskDelay(pdMS_TO_TICKS(20));
      }

      usb_host_lib_info_t info = {};
      if (usb_host_lib_info(&info) == ESP_OK &&
          info.num_devices != previous_device_count) {
        previous_device_count = info.num_devices;
        enumerated_devices_.store(info.num_devices);
        Serial.printf("[USB-ETH] Enumerated USB devices: %d\n",
                      info.num_devices);
      }
    }
  }

  static void clientEventCallback(
      const usb_host_client_event_msg_t* event_message, void* arg) {
    auto* self = static_cast<Rtl8156UsbDriver*>(arg);
    if (!self || !event_message || !self->event_queue_) return;

    DriverEvent event = {};
    if (event_message->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
      event.type = DriverEventType::NewDevice;
      event.address = event_message->new_dev.address;
    } else if (event_message->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
      event.type = DriverEventType::DeviceGone;
      event.device = event_message->dev_gone.dev_hdl;
    } else {
      return;
    }
    xQueueSend(self->event_queue_, &event, 0);
  }

  void clientTask() {
    xSemaphoreTake(host_ready_, portMAX_DELAY);
    if (!host_installed_.load()) {
      xSemaphoreGive(client_ready_);
      vTaskDelete(nullptr);
      return;
    }

    usb_host_client_config_t client_config = {};
    client_config.is_synchronous = false;
    client_config.max_num_event_msg = 8;
    client_config.async.client_event_callback =
        &Rtl8156UsbDriver::clientEventCallback;
    client_config.async.callback_arg = this;

    const esp_err_t err =
        usb_host_client_register(&client_config, &client_);
    client_registered_.store(err == ESP_OK);
    if (err != ESP_OK) {
      Serial.printf("[USB-ETH] USB client registration failed: %s\n",
                    esp_err_to_name(err));
      xSemaphoreGive(client_ready_);
      vTaskDelete(nullptr);
      return;
    }

    xSemaphoreGive(client_ready_);
    for (;;) {
      const esp_err_t event_err =
          usb_host_client_handle_events(client_, portMAX_DELAY);
      if (event_err != ESP_OK) {
        Serial.printf("[USB-ETH] Client event error: %s\n",
                      esp_err_to_name(event_err));
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    }
  }

  void workerTask() {
    xSemaphoreTake(client_ready_, portMAX_DELAY);
    if (!client_registered_.load()) {
      vTaskDelete(nullptr);
      return;
    }

    for (;;) {
      DriverEvent event = {};
      if (xQueueReceive(event_queue_, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (event.type == DriverEventType::NewDevice) {
          Serial.printf("[USB-ETH] New USB device at address %u\n",
                        static_cast<unsigned>(event.address));
          if (!dev_) attachDevice(event.address);
        } else if (event.type == DriverEventType::DeviceGone &&
                   event.device == dev_) {
          Serial.println("[USB-ETH] Adapter removed");
          cleanupDevice(true);
        }
      }

      serviceEndpointRecovery();
      serviceTx();
      serviceDataPath();
      serviceRxUpgrade();

      const uint32_t now = millis();
      if (attached_.load() &&
          static_cast<int32_t>(now - next_link_poll_ms_) >= 0) {
        next_link_poll_ms_ = now + kLinkPollMs;
        pollLink();
      }
    }
  }

  bool attachDevice(uint8_t address) {
    usb_device_handle_t candidate = nullptr;
    esp_err_t err = usb_host_device_open(client_, address, &candidate);
    if (err != ESP_OK) {
      Serial.printf("[USB-ETH] Cannot open USB device %u: %s\n",
                    static_cast<unsigned>(address), esp_err_to_name(err));
      return false;
    }

    const usb_device_desc_t* device_desc = nullptr;
    err = usb_host_get_device_descriptor(candidate, &device_desc);
    if (err != ESP_OK || !device_desc) {
      Serial.printf("[USB-ETH] Cannot read descriptor for USB device %u: %s\n",
                    static_cast<unsigned>(address), esp_err_to_name(err));
      usb_host_device_close(client_, candidate);
      return false;
    }
    Serial.printf("[USB-ETH] USB device %04X:%04X detected\n",
                  device_desc->idVendor, device_desc->idProduct);
    if (device_desc->idVendor != kRealtekVid ||
        device_desc->idProduct != kRtl8156Pid) {
      Serial.println("[USB-ETH] Device is not the configured RTL8156 adapter");
      usb_host_device_close(client_, candidate);
      return false;
    }

    const usb_config_desc_t* config_desc = nullptr;
    err = usb_host_get_active_config_descriptor(candidate, &config_desc);
    if (err != ESP_OK || !config_desc) {
      usb_host_device_close(client_, candidate);
      return false;
    }

    uint8_t interface_number = 0xff;
    uint8_t bulk_in = 0;
    uint8_t bulk_out = 0;
    uint16_t bulk_in_mps = 0;
    for (uint8_t number = 0; number < config_desc->bNumInterfaces; ++number) {
      int interface_offset = 0;
      const usb_intf_desc_t* interface_desc =
          usb_parse_interface_descriptor(config_desc, number, 0,
                                         &interface_offset);
      if (!interface_desc || interface_desc->bInterfaceClass != 0xff ||
          interface_desc->bInterfaceSubClass != 0xff) {
        continue;
      }

      for (uint8_t endpoint_index = 0;
           endpoint_index < interface_desc->bNumEndpoints; ++endpoint_index) {
        int endpoint_offset = interface_offset;
        const usb_ep_desc_t* endpoint_desc =
            usb_parse_endpoint_descriptor_by_index(
                interface_desc, endpoint_index, config_desc->wTotalLength,
                &endpoint_offset);
        if (!endpoint_desc ||
            USB_EP_DESC_GET_XFERTYPE(endpoint_desc) !=
                USB_TRANSFER_TYPE_BULK) {
          continue;
        }
        if (USB_EP_DESC_GET_EP_DIR(endpoint_desc)) {
          bulk_in = endpoint_desc->bEndpointAddress;
          bulk_in_mps = USB_EP_DESC_GET_MPS(endpoint_desc);
        } else {
          bulk_out = endpoint_desc->bEndpointAddress;
        }
      }
      if (bulk_in && bulk_out) {
        interface_number = interface_desc->bInterfaceNumber;
        break;
      }
    }

    if (interface_number == 0xff || !bulk_in || !bulk_out ||
        bulk_in_mps == 0) {
      Serial.println("[USB-ETH] RTL8156 bulk endpoints not found");
      usb_host_device_close(client_, candidate);
      return false;
    }

    err = usb_host_interface_claim(client_, candidate, interface_number, 0);
    if (err != ESP_OK) {
      Serial.printf("[USB-ETH] Interface claim failed: %s\n",
                    esp_err_to_name(err));
      usb_host_device_close(client_, candidate);
      return false;
    }

    dev_ = candidate;
    interface_number_ = interface_number;
    interface_claimed_ = true;
    bulk_in_endpoint_ = bulk_in;
    bulk_out_endpoint_ = bulk_out;
    bulk_in_mps_ = bulk_in_mps;

    usb_device_info_t device_info = {};
    usb_host_device_info(dev_, &device_info);
    Serial.printf(
        "[USB-ETH] Realtek 0BDA:8156 connected, USB speed=%s, IN=0x%02x, "
        "OUT=0x%02x\n",
        device_info.speed == USB_SPEED_HIGH
            ? "high"
            : (device_info.speed == USB_SPEED_FULL ? "full" : "low"),
        bulk_in_endpoint_, bulk_out_endpoint_);

    if (!allocateControlTransfer() || !initializeAdapter()) {
      Serial.println("[USB-ETH] RTL8156 initialization failed");
      cleanupDevice(false);
      return false;
    }

    const esp_err_t mac_err = esp_netif_set_mac(netif_, mac_);
    if (mac_err != ESP_OK) {
      Serial.printf("[USB-ETH] Cannot set netif MAC: %s\n",
                    esp_err_to_name(mac_err));
      cleanupDevice(false);
      return false;
    }

    esp_netif_action_start(netif_, nullptr, 0, nullptr);
    static_ip_active_ = applyConfiguredAddressing(netif_);
    netif_started_ = true;
    attached_.store(true);
    next_link_poll_ms_ = 0;

    pollLink();
    Serial.printf("[USB-ETH] Adapter ready, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5]);
    return true;
  }

  bool allocateControlTransfer() {
    if (control_transfer_) return true;
    const esp_err_t err =
        usb_host_transfer_alloc(kControlBufferSize, 0, &control_transfer_);
    if (err != ESP_OK || !control_transfer_) return false;
    control_transfer_->device_handle = dev_;
    control_transfer_->bEndpointAddress = 0;
    control_transfer_->timeout_ms = 1000;
    control_transfer_->callback = &Rtl8156UsbDriver::controlTransferCallback;
    control_transfer_->context = this;
    return true;
  }

  bool allocateDataTransfers() {
    if (!tx_transfer_) {
      const esp_err_t tx_err =
          usb_host_transfer_alloc(kTxBufferSize, 0, &tx_transfer_);
      if (tx_err != ESP_OK || !tx_transfer_) {
        releaseDataTransfers();
        return false;
      }
      tx_transfer_->device_handle = dev_;
      tx_transfer_->bEndpointAddress = bulk_out_endpoint_;
      tx_transfer_->timeout_ms = 1000;
      tx_transfer_->flags = USB_TRANSFER_FLAG_ZERO_PACK;
      tx_transfer_->callback = &Rtl8156UsbDriver::txTransferCallback;
      tx_transfer_->context = this;
    }

    size_t rx_count = 0;
    for (size_t i = 0; i < kRxTransferCount; ++i) {
      if (rx_transfers_[i]) {
        ++rx_count;
        continue;
      }
      if (!allocateRxSlot(i)) {
        if (rx_count == 0) {
          releaseDataTransfers();
          return false;
        }
        Serial.printf(
            "[USB-ETH] RX fallback: %u x %u KB (second transfer "
            "unavailable)\n",
            static_cast<unsigned>(rx_count),
            static_cast<unsigned>(kRxBufferSize / 1024));
        break;
      }
      ++rx_count;
    }

    return rx_count > 0;
  }

  bool allocateRxSlot(size_t i) {
    const size_t transfer_size = static_cast<size_t>(
        usb_round_up_to_mps(static_cast<int>(kRxBufferSize), bulk_in_mps_));
    const esp_err_t err =
        usb_host_transfer_alloc(transfer_size, 0, &rx_transfers_[i]);
    if (err != ESP_OK || !rx_transfers_[i]) {
      rx_transfers_[i] = nullptr;
      return false;
    }
    rx_contexts_[i].owner = this;
    rx_contexts_[i].index = static_cast<uint8_t>(i);
    rx_transfers_[i]->device_handle = dev_;
    rx_transfers_[i]->bEndpointAddress = bulk_in_endpoint_;
    rx_transfers_[i]->num_bytes = static_cast<int>(transfer_size);
    rx_transfers_[i]->timeout_ms = 0;
    rx_transfers_[i]->callback = &Rtl8156UsbDriver::rxTransferCallback;
    rx_transfers_[i]->context = &rx_contexts_[i];
    return true;
  }

  static void controlTransferCallback(usb_transfer_t* transfer) {
    auto* self = static_cast<Rtl8156UsbDriver*>(transfer->context);
    if (self && self->control_done_) {
      xSemaphoreGive(self->control_done_);
    }
  }

  bool controlTransfer(bool read, uint16_t address, uint16_t index,
                       void* data, size_t length) {
    if (!control_transfer_ || !dev_ || length + USB_SETUP_PACKET_SIZE >
                                               control_transfer_->data_buffer_size) {
      io_ok_ = false;
      return false;
    }

    while (xSemaphoreTake(control_done_, 0) == pdTRUE) {
    }

    auto* setup =
        reinterpret_cast<usb_setup_packet_t*>(control_transfer_->data_buffer);
    setup->bmRequestType =
        (read ? USB_BM_REQUEST_TYPE_DIR_IN : USB_BM_REQUEST_TYPE_DIR_OUT) |
        USB_BM_REQUEST_TYPE_TYPE_VENDOR |
        USB_BM_REQUEST_TYPE_RECIP_DEVICE;
    setup->bRequest = kRealtekRequest;
    setup->wValue = address;
    setup->wIndex = index;
    setup->wLength = static_cast<uint16_t>(length);

    uint8_t* payload =
        control_transfer_->data_buffer + USB_SETUP_PACKET_SIZE;
    if (!read && length) memcpy(payload, data, length);
    control_transfer_->num_bytes =
        static_cast<int>(USB_SETUP_PACKET_SIZE + length);
    control_transfer_->device_handle = dev_;

    const esp_err_t submit_err =
        usb_host_transfer_submit_control(client_, control_transfer_);
    if (submit_err != ESP_OK ||
        xSemaphoreTake(control_done_, pdMS_TO_TICKS(1200)) != pdTRUE ||
        control_transfer_->status != USB_TRANSFER_STATUS_COMPLETED) {
      io_ok_ = false;
      return false;
    }

    if (read && length) memcpy(data, payload, length);
    return true;
  }

  bool readMemory(uint16_t address, uint16_t index, void* data,
                  size_t length) {
    return controlTransfer(true, address, index, data, length);
  }

  bool writeMemory(uint16_t address, uint16_t index, const void* data,
                   size_t length) {
    return controlTransfer(false, address, index,
                           const_cast<void*>(data), length);
  }

  uint8_t read1(uint16_t reg, uint16_t index) {
    uint32_t value = 0;
    const uint8_t shift = static_cast<uint8_t>((reg & 3U) << 3U);
    reg &= ~3U;
    readMemory(reg, index, &value, sizeof(value));
    return static_cast<uint8_t>((value >> shift) & 0xffU);
  }

  uint16_t read2(uint16_t reg, uint16_t index) {
    uint32_t value = 0;
    const uint8_t shift = static_cast<uint8_t>((reg & 2U) << 3U);
    reg &= ~3U;
    readMemory(reg, index, &value, sizeof(value));
    return static_cast<uint16_t>((value >> shift) & 0xffffU);
  }

  uint32_t read4(uint16_t reg, uint16_t index) {
    uint32_t value = 0;
    readMemory(reg, index, &value, sizeof(value));
    return value;
  }

  void write1(uint16_t reg, uint16_t index, uint32_t value) {
    uint16_t byte_enable = kByteEnableByte;
    const uint8_t shift = static_cast<uint8_t>(reg & 3U);
    value &= 0xffU;
    if (shift) {
      byte_enable = static_cast<uint16_t>(byte_enable << shift);
      value <<= (shift << 3U);
      reg &= ~3U;
    }
    writeMemory(reg, index | byte_enable, &value, sizeof(value));
  }

  void write2(uint16_t reg, uint16_t index, uint32_t value) {
    uint16_t byte_enable = kByteEnableWord;
    const uint8_t shift = static_cast<uint8_t>(reg & 2U);
    value &= 0xffffU;
    if (shift) {
      byte_enable = static_cast<uint16_t>(byte_enable << shift);
      value <<= (shift << 3U);
      reg &= ~3U;
    }
    writeMemory(reg, index | byte_enable, &value, sizeof(value));
  }

  void write4(uint16_t reg, uint16_t index, uint32_t value) {
    writeMemory(reg, index | kByteEnableDword, &value, sizeof(value));
  }

  uint16_t ocpRead(uint16_t address) {
    write2(kPlaOcpGphyBase, kMcuPla, address & 0xf000U);
    const uint16_t reg = static_cast<uint16_t>((address & 0x0fffU) | 0xb000U);
    return read2(reg, kMcuPla);
  }

  void ocpWrite(uint16_t address, uint16_t value) {
    write2(kPlaOcpGphyBase, kMcuPla, address & 0xf000U);
    const uint16_t reg = static_cast<uint16_t>((address & 0x0fffU) | 0xb000U);
    write2(reg, kMcuPla, value);
  }

  void setBits1(uint16_t reg, uint16_t index, uint8_t bits) {
    write1(reg, index, read1(reg, index) | bits);
  }

  void clearBits1(uint16_t reg, uint16_t index, uint8_t bits) {
    write1(reg, index, read1(reg, index) & ~bits);
  }

  void setBits2(uint16_t reg, uint16_t index, uint16_t bits) {
    write2(reg, index, read2(reg, index) | bits);
  }

  void clearBits2(uint16_t reg, uint16_t index, uint16_t bits) {
    write2(reg, index, read2(reg, index) & ~bits);
  }

  void clearBits4(uint16_t reg, uint16_t index, uint32_t bits) {
    write4(reg, index, read4(reg, index) & ~bits);
  }

  void enableAldps(bool enable) {
    if (enable) {
      ocpWrite(kOcpPowerCfg, ocpRead(kOcpPowerCfg) | kEnableAldps);
      return;
    }

    ocpWrite(kOcpAldpsConfig,
             kEnablePowerDown | kLinkEnable | kDisableSdsave);
    for (int i = 0; i < 20; ++i) {
      vTaskDelay(pdMS_TO_TICKS(1));
      if (ocpRead(0xe000) & 0x0100) break;
    }
  }

  uint16_t waitPhyStatus(uint16_t desired) {
    uint16_t value = 0;
    for (int i = 0; i < 1000; ++i) {
      value = ocpRead(kOcpPhyStatus) & kPhyStatusMask;
      if (desired) {
        if (value == desired) break;
      } else if (value == kPhyStatusLanOn ||
                 value == kPhyStatusPowerDown ||
                 value == kPhyStatusExternalInit) {
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    return value;
  }

  void resetChip() {
    write1(kPlaCr, kMcuPla, kCrReset);
    for (int i = 0; i < 1000; ++i) {
      if (!(read1(kPlaCr, kMcuPla) & kCrReset)) return;
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    io_ok_ = false;
    Serial.println("[USB-ETH] RTL8156 reset timed out");
  }

  void disableTeredo() {
    write1(kPlaTeredoCfg, kMcuPla, 0xff);
    write2(kPlaWdt6Ctrl, kMcuPla, kWdt6SetMode);
    write2(kPlaRealwowTimer, kMcuPla, 0);
    write4(kPlaTeredoTimer, kMcuPla, 0);
  }

  void rtl8153bInit() {
    clearBits1(0xd26b, kMcuUsb, 0x01);
    write2(0xd32a, kMcuUsb, 0);
    setBits2(0xcfee, kMcuUsb, 0x0020);
    if (is_8156b_) setBits2(0xb460, kMcuUsb, 0x0008);

    enableAldps(false);
    clearBits2(kUsbLpmConfig, kMcuUsb, kLpmU1u2Enable);

    if (chip_version_ == kVersion8156B2 &&
        (read2(0xd3ae, kMcuPla) & 0x0002) &&
        !(read2(0xd284, kMcuUsb) & 0x0020)) {
      for (int i = 0; i < 100; ++i) {
        if (read2(0xd284, kMcuUsb) & 0x0004) break;
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }

    bool autoload_done = false;
    for (int i = 0; i < 1000; ++i) {
      if (read2(kPlaBootCtrl, kMcuPla) & kAutoloadDone) {
        autoload_done = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (!autoload_done) {
      io_ok_ = false;
      Serial.println("[USB-ETH] RTL8156 autoload timed out");
      return;
    }

    const uint16_t phy_status = waitPhyStatus(0);
    if (phy_status == kPhyStatusExternalInit) {
      ocpWrite(0xa468, ocpRead(0xa468) & ~0x000a);
      if (is_8156b_) ocpWrite(0xa466, ocpRead(0xa466) & ~0x0001);
    }

    uint16_t bmcr = ocpRead(kOcpBaseMii + kMiiBmcr);
    if (bmcr & kBmcrPowerDown) {
      ocpWrite(kOcpBaseMii + kMiiBmcr, bmcr & ~kBmcrPowerDown);
    }
    waitPhyStatus(kPhyStatusLanOn);

    clearBits2(kUsbU2p3Ctrl, kMcuUsb, kU2p3Enable);
    write2(kUsbMscTimer, kMcuUsb, 0x0fff);
    write2(kUsbU1u2Timer, kMcuUsb, 500);

    clearBits2(kUsbPowerCut, kMcuUsb, kPowerEnable);
    clearBits2(kUsbMisc0, kMcuUsb, kPowerCutStatus);
    clearBits1(kUsbPowerCut, kMcuUsb, kUpsEnable | kUpsPrewake);
    clearBits1(0xcfff, kMcuUsb, 0x01);

    clearBits1(kPlaIndicateFlag, kMcuUsb, kUpcomingRuntimeD3);
    clearBits1(kPlaSuspendFlag, kMcuUsb, kLinkChangeEvent);
    clearBits2(kPlaExtraStatus, kMcuUsb, kLinkChangeFlag);

    write1(kPlaCrwecr, kMcuPla, kCrwecrConfig);
    clearBits2(kPlaConfig34, kMcuUsb, kLinkOffWakeEnable);
    write1(kPlaCrwecr, kMcuPla, kCrwecrNormal);

    if (is_8156b_) {
      clearBits2(0xc010, kMcuPla, 0x0800);
      setBits2(0xe854, kMcuPla, 0x0001);
      write2(kUsbFcTimer, kMcuUsb, kControlTimerEnable | (600 / 8));
      if (!(read1(0xdc6b, kMcuPla) & 0x80)) {
        uint16_t value = read2(kUsbFwCtrl, kMcuUsb);
        value |= kFlowControlPatchOption | 0x0100;
        value &= ~0x0008;
        write2(kUsbFwCtrl, kMcuUsb, value);
      }
      setBits2(kUsbFwTask, kMcuUsb, kFcPatchTask);
    }

    uint16_t extra = read2(kPlaExtraStatus, kMcuPla);
    if (read2(kPlaPhystatus, kMcuPla) & kPhystatusLink) {
      extra |= kCurrentLinkOk;
    } else {
      extra &= ~kCurrentLinkOk;
    }
    write2(kPlaExtraStatus, kMcuPla, extra | kPollLinkChange);

    write2(kPlaMacPwrCtrl, kMcuPla, 0x0403);
    uint16_t power2 = read2(kPlaMacPwrCtrl2, kMcuPla);
    power2 = static_cast<uint16_t>((power2 & ~0x00ffU) |
                                   kMacClockSpeedDownEnable | 0x0003);
    write2(kPlaMacPwrCtrl2, kMcuPla, power2);
    clearBits2(kPlaMacPwrCtrl3, kMcuPla, kPlaMcuSpeedDownEnable);

    clearBits2(kUsbUsbCtrl, kMcuUsb,
               kRxAggregationDisable | kRxZeroEnable);
    if (!is_8156b_) setBits1(0xd4b4, kMcuUsb, 0x02);
    setBits2(kPlaRstTally, kMcuUsb, kTallyReset);
  }

  void rtl8153bNicReset() {
    clearBits2(kUsbLpmConfig, kMcuUsb, kLpmU1u2Enable);
    clearBits2(kUsbU2p3Ctrl, kMcuUsb, kU2p3Enable);
    enableAldps(false);
    setBits2(kPlaMisc1, kMcuPla, kRxdyGatedEnable);
    disableTeredo();
    clearBits4(kPlaRcr, kMcuPla, kRcrAcceptAll);
    resetChip();

    clearBits1(kUsbBmuReset, kMcuUsb, kBmuResetIn | kBmuResetOut);
    setBits1(kUsbBmuReset, kMcuUsb, kBmuResetIn | kBmuResetOut);
    clearBits1(kPlaOobCtrl, kMcuPla, 0x80);
    clearBits2(kPlaSffStatus7, kMcuPla, kMcuBorrowEnable);

    uint16_t rx_vlan = read2(0xc012, kMcuPla);
    write2(0xc012, kMcuPla, rx_vlan & ~0x00c0U);

    constexpr uint16_t frame_length = 1500 + 14 + 4 + 4;
    write2(kPlaRms, kMcuPla, frame_length);
    write1(kPlaMtps, kMcuPla, kMtpsJumbo);

    if (!is_8156b_) {
      write2(0xc0a6, kMcuPla, 0x0400);
      write2(0xc0aa, kMcuPla, 0x0800);
    } else {
      write2(0xc0a6, kMcuPla, 0x0200);
      write2(0xc0aa, kMcuPla, 0x0400);
    }

    uint16_t fifo = read2(0xc0a2, kMcuPla);
    write2(0xc0a2, kMcuPla, (fifo & ~0x0fffU) | kRxFifoThreshold);
    write4(kUsbRxBufThreshold, kMcuUsb, kUsbRxThreshold);

    write2(kPlaTxfifoCtrl, kMcuPla, kTxFifoThreshold);
    if (!is_8156b_) {
      setBits2(0xd4b4, kMcuUsb, 0x0002);
    } else {
      write2(0xe61a, kMcuPla, (frame_length + 0x100) / 16);
    }

    clearBits2(kPlaMacPwrCtrl3, kMcuPla, kPlaMcuSpeedDownEnable);
    clearBits2(0xd32a, kMcuUsb, 0x0300);
    enableAldps(true);
    setBits2(kUsbU2p3Ctrl, kMcuUsb, kU2p3Enable);
  }

  bool enableAdapterDataPathHardware() {
    io_ok_ = true;

    write1(kPlaCrwecr, kMcuPla, kCrwecrConfig);
    uint8_t mac_buffer[8] = {};
    memcpy(mac_buffer, mac_, sizeof(mac_));
    writeMemory(kPlaIdr, kMcuPla | kByteEnableSixBytes, mac_buffer,
                sizeof(mac_buffer));
    write1(kPlaCrwecr, kMcuPla, kCrwecrNormal);

    write2(kUsbRxEarlyAgg, kMcuUsb, 80);
    write2(kUsbRxExtraAggTimer, kMcuUsb, 1875);
    const uint32_t early_size =
        kRxBufferSize - (1500 + 14 + 4 + sizeof(RtlRxHeader) + 8);
    write2(kUsbRxEarlySize, kMcuUsb, early_size / 8);
    write1(kUsbUptRxdmaOwn, kMcuUsb, kOwnUpdate | kOwnClear);

    if (is_8156b_) {
      clearBits2(kUsbFwTask, kMcuUsb, kFcPatchTask);
      vTaskDelay(pdMS_TO_TICKS(2));
      setBits2(kUsbFwTask, kMcuUsb, kFcPatchTask);
    }

    clearBits2(kPlaFmc, kMcuPla, kFmcMcuEnable);
    setBits2(kPlaFmc, kMcuPla, kFmcMcuEnable);
    clearBits2(kPlaCpcr, kMcuPla, kCpcrRxVlan);
    configureReceiveFilter();
    setBits1(kPlaCr, kMcuPla, kCrRxEnable | kCrTxEnable);
    clearBits2(kPlaMisc1, kMcuPla, kRxdyGatedEnable);
    adapter_data_path_ready_ = io_ok_;
    return adapter_data_path_ready_;
  }

  void configureReceiveFilter() {
    uint32_t receive_mode = read4(kPlaRcr, kMcuPla);
    receive_mode &= ~(0x00000001U | kRcrAcceptMulticast);
    receive_mode |=
        kRcrAcceptPhysical | kRcrAcceptBroadcast | kRcrAcceptMulticast;
    // Home Assistant discovery, mDNS and IPv6 all depend on multicast.
    write4(kPlaMar0, kMcuPla, 0xffffffff);
    write4(kPlaMar4, kMcuPla, 0xffffffff);
    write4(kPlaRcr, kMcuPla, receive_mode);
  }

  void startAutoNegotiation() {
    uint16_t advertise_2500 = ocpRead(0xa5d4);
    advertise_2500 |= kAdvertise2500Full;
    ocpWrite(kOcpBaseMii + kMiiAnar * 2,
             kAnar10Half | kAnar10Full | kAnar100Half | kAnar100Full |
                 kAnarPause | kAnarAsymPause);
    ocpWrite(kOcpBaseMii + kMii1000Control * 2,
             kAdvertise1000Half | kAdvertise1000Full);
    ocpWrite(0xa5d4, advertise_2500);
    ocpWrite(kOcpBaseMii + kMiiBmcr,
             kBmcrAutoEnable | kBmcrRestartAuto);
  }

  bool initializeAdapter() {
    io_ok_ = true;
    chip_version_ = read2(kPlaTcr1, kMcuPla) & kVersionMask;
    switch (chip_version_) {
      case kVersion8156A1:
      case kVersion8156A2:
        is_8156b_ = false;
        break;
      case kVersion8156B1:
      case kVersion8156B2:
        is_8156b_ = true;
        break;
      default:
        Serial.printf("[USB-ETH] Unsupported RTL8156 version 0x%04x\n",
                      chip_version_);
        return false;
    }

    rtl8153bInit();
    if (!io_ok_) return false;

    uint8_t station_address[8] = {};
    readMemory(kPlaBackup, kMcuPla, station_address,
               sizeof(station_address));
    memcpy(mac_, station_address, sizeof(mac_));
    bool all_zero = true;
    for (uint8_t value : mac_) {
      if (value != 0) {
        all_zero = false;
        break;
      }
    }
    if (all_zero || (mac_[0] & 0x01)) {
      if (esp_read_mac(mac_, ESP_MAC_ETH) != ESP_OK) {
        esp_read_mac(mac_, ESP_MAC_BASE);
      }
      mac_[0] &= ~0x01;
      mac_[0] |= 0x02;
      Serial.println("[USB-ETH] Adapter MAC invalid; using local MAC");
    }

    rtl8153bNicReset();
    if (!enableAdapterDataPathHardware()) return false;
    startAutoNegotiation();

    Serial.printf("[USB-ETH] RTL8156 chip version 0x%04x initialized\n",
                  chip_version_);
    return io_ok_;
  }

  static void rxTransferCallback(usb_transfer_t* transfer) {
    auto* context = static_cast<RxContext*>(transfer->context);
    if (!context || !context->owner ||
        context->index >= kRxTransferCount) {
      return;
    }
    context->owner->handleRxTransfer(context->index, transfer);
  }

  void handleRxTransfer(size_t index, usb_transfer_t* transfer) {
    rx_callback_active_[index].store(true);
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
      const uint32_t completed = rx_completed_.fetch_add(1) + 1;
      if (completed <= 3) {
        Serial.printf("[USB-ETH] RX #%u: %u bytes\n",
                      static_cast<unsigned>(completed),
                      static_cast<unsigned>(transfer->actual_num_bytes));
      }
      parseRxBuffer(transfer->data_buffer,
                    static_cast<size_t>(transfer->actual_num_bytes));
      rx_in_flight_[index].store(false);
      if (attached_.load() && link_up_.load() &&
          data_path_active_.load()) {
        submitRx(index);
      }
      rx_callback_active_[index].store(false);
      return;
    }
    rx_in_flight_[index].store(false);
    if (transfer->status == USB_TRANSFER_STATUS_NO_DEVICE ||
        transfer->status == USB_TRANSFER_STATUS_CANCELED) {
      rx_callback_active_[index].store(false);
      return;
    }

    Serial.printf("[USB-ETH] RX transfer error: %u\n",
                  static_cast<unsigned>(transfer->status));
    rx_recovery_mask_.fetch_or(1U << index);
    rx_callback_active_[index].store(false);
  }

  void parseRxBuffer(const uint8_t* buffer, size_t length) {
    size_t offset = 0;
    while (length - offset >= sizeof(RtlRxHeader)) {
      RtlRxHeader header = {};
      memcpy(&header, buffer + offset, sizeof(header));
      offset += sizeof(header);

      const size_t packet_length =
          static_cast<size_t>(header.packet_length & kRxLengthMask);
      if (packet_length < 14 + 4 || packet_length > length - offset) {
        Serial.printf("[USB-ETH] Invalid RX packet length: %u\n",
                      static_cast<unsigned>(packet_length));
        return;
      }

      const size_t ethernet_length = packet_length - 4;  // strip CRC
      void* ethernet_frame =
          heap_caps_malloc(ethernet_length, MALLOC_CAP_8BIT);
      if (!ethernet_frame) {
        Serial.printf("[USB-ETH] RX frame allocation failed: %u bytes\n",
                      static_cast<unsigned>(ethernet_length));
        return;
      }
      memcpy(ethernet_frame, buffer + offset, ethernet_length);
      const esp_err_t receive_err =
          esp_netif_receive(netif_, ethernet_frame, ethernet_length, nullptr);
      if (receive_err != ESP_OK) {
        Serial.printf("[USB-ETH] esp_netif_receive failed: %s\n",
                      esp_err_to_name(receive_err));
      }

      const size_t aligned_length = (packet_length + 7U) & ~7U;
      if (aligned_length > length - offset) return;
      offset += aligned_length;
    }
  }

  bool submitRx(size_t index) {
    if (index >= kRxTransferCount || !rx_transfers_[index] ||
        !attached_.load() || !link_up_.load() ||
        !data_path_active_.load()) {
      return false;
    }
    bool expected = false;
    if (!rx_in_flight_[index].compare_exchange_strong(expected, true)) {
      return true;
    }

    rx_transfers_[index]->num_bytes =
        static_cast<int>(rx_transfers_[index]->data_buffer_size);
    const esp_err_t err = usb_host_transfer_submit(rx_transfers_[index]);
    if (err != ESP_OK) {
      rx_in_flight_[index].store(false);
      Serial.printf("[USB-ETH] RX submit failed: %s\n",
                    esp_err_to_name(err));
      return false;
    }
    return true;
  }

  static void txTransferCallback(usb_transfer_t* transfer) {
    auto* self = static_cast<Rtl8156UsbDriver*>(transfer->context);
    if (!self) return;
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED ||
        transfer->status == USB_TRANSFER_STATUS_NO_DEVICE ||
        transfer->status == USB_TRANSFER_STATUS_CANCELED) {
      self->tx_in_flight_.store(false);
      return;
    }
    Serial.printf("[USB-ETH] TX transfer error: %u\n",
                  static_cast<unsigned>(transfer->status));
    self->tx_recovery_pending_.store(true);
    self->tx_in_flight_.store(false);
  }

  void serviceTx() {
    if (!attached_.load() || !link_up_.load() || !tx_transfer_ ||
        tx_in_flight_.load()) {
      return;
    }

    TxFrame* frame = nullptr;
    if (xQueueReceive(tx_queue_, &frame, 0) != pdTRUE || !frame) return;

    const uint16_t frame_length = frame->length;
    const size_t aligned_length = (frame_length + 3U) & ~3U;
    const size_t transfer_length = sizeof(RtlTxHeader) + aligned_length;
    if (transfer_length > tx_transfer_->data_buffer_size) {
      heap_caps_free(frame);
      return;
    }

    RtlTxHeader header = {};
    header.packet_length =
        (frame_length & 0xffffU) | kTxFirstSegment | kTxLastSegment;
    memcpy(tx_transfer_->data_buffer, &header, sizeof(header));
    memcpy(tx_transfer_->data_buffer + sizeof(header), frame->data,
           frame_length);
    if (aligned_length > frame_length) {
      memset(tx_transfer_->data_buffer + sizeof(header) + frame_length, 0,
             aligned_length - frame_length);
    }
    heap_caps_free(frame);

    tx_transfer_->num_bytes = static_cast<int>(transfer_length);
    tx_in_flight_.store(true);
    const esp_err_t err = usb_host_transfer_submit(tx_transfer_);
    if (err != ESP_OK) {
      tx_in_flight_.store(false);
      Serial.printf("[USB-ETH] TX submit failed: %s\n",
                    esp_err_to_name(err));
    } else {
      const uint32_t submitted = tx_submitted_.fetch_add(1) + 1;
      if (submitted <= 3) {
        Serial.printf("[USB-ETH] TX #%u: %u bytes\n",
                      static_cast<unsigned>(submitted),
                      static_cast<unsigned>(frame_length));
      }
    }
  }

  // Restore the full RX buffer set after the one-buffer 16 KB fallback used
  // when switching back from Wi-Fi. Hosted teardown can leave DMA heap too
  // fragmented for the second block; periodic retries add the transfer
  // once the heap has recovered.
  void serviceRxUpgrade() {
    if (!data_path_active_.load() || !attached_.load() || !dev_) return;

    bool missing = false;
    for (size_t i = 0; i < kRxTransferCount; ++i) {
      if (!rx_transfers_[i]) {
        missing = true;
        break;
      }
    }
    if (!missing) return;

    const uint32_t now = millis();
    if (static_cast<int32_t>(now - next_rx_upgrade_ms_) < 0) return;
    next_rx_upgrade_ms_ = now + kRxUpgradeRetryMs;

    for (size_t i = 0; i < kRxTransferCount; ++i) {
      if (rx_transfers_[i]) continue;
      if (!allocateRxSlot(i)) continue;
      if (submitRx(i)) {
        Serial.printf(
            "[USB-ETH] RX buffer restored: %u x %u KB active again\n",
            static_cast<unsigned>(kRxTransferCount),
            static_cast<unsigned>(kRxBufferSize / 1024));
      } else {
        // Submission never succeeded, so no callback is pending. Free this slot
        // so the next attempt is not stuck behind an inactive buffer.
        usb_host_transfer_free(rx_transfers_[i]);
        rx_transfers_[i] = nullptr;
      }
    }
  }

  void serviceEndpointRecovery() {
    if (!attached_.load() || !dev_ || !data_path_active_.load()) return;

    const uint32_t rx_mask = rx_recovery_mask_.exchange(0);
    if (rx_mask) {
      usb_host_endpoint_halt(dev_, bulk_in_endpoint_);
      usb_host_endpoint_flush(dev_, bulk_in_endpoint_);
      usb_host_endpoint_clear(dev_, bulk_in_endpoint_);
      for (size_t i = 0; i < kRxTransferCount; ++i) {
        if (rx_mask & (1U << i)) submitRx(i);
      }
    }

    if (tx_recovery_pending_.exchange(false)) {
      usb_host_endpoint_halt(dev_, bulk_out_endpoint_);
      usb_host_endpoint_flush(dev_, bulk_out_endpoint_);
      usb_host_endpoint_clear(dev_, bulk_out_endpoint_);
    }
  }

  void pollLink() {
    io_ok_ = true;
    const uint16_t status = read2(kPlaPhystatus, kMcuPla);
    if (!io_ok_) {
      if (++link_read_failures_ >= 3) setLink(false, 0);
      return;
    }
    link_read_failures_ = 0;
    setLink((status & kPhystatusLink) != 0, status);
  }

  void setLink(bool up, uint16_t status) {
    const bool previous = link_up_.exchange(up);
    if (previous == up) return;

    if (up) {
      if (status & kPhystatus2500) {
        clearBits2(kPlaMacPwrCtrl4, kMcuPla, 0x0040);
      } else {
        setBits2(kPlaMacPwrCtrl4, kMcuPla, 0x0040);
      }
      // Expose the physical link immediately, but start DHCP shortly later.
      // This gives the shared transport manager time to stop hosted WiFi and
      // return its DMA/lwIP memory before Ethernet allocates its data path.
      netif_connect_at_ = millis() + kLinkDhcpDelayMs;
      netif_connect_pending_.store(true);
      const char* speed =
          (status & kPhystatus2500)
              ? "2.5 Gbit/s"
              : ((status & kPhystatus1000)
                     ? "1 Gbit/s"
                     : ((status & kPhystatus100) ? "100 Mbit/s"
                                                : "10 Mbit/s"));
      Serial.printf("[USB-ETH] Link up: %s\n", speed);
    } else {
      netif_connect_pending_.store(false);
      if (netif_started_) {
        esp_netif_action_disconnected(netif_, nullptr, 0, nullptr);
      }
      suspendDataPath();
      Serial.println("[USB-ETH] Link down");
    }
  }

  bool releaseDataTransfers() {
    if (tx_in_flight_.load() || anyRxBusy()) return false;

    if (tx_transfer_) {
      usb_host_transfer_free(tx_transfer_);
      tx_transfer_ = nullptr;
    }
    for (size_t i = 0; i < kRxTransferCount; ++i) {
      if (rx_transfers_[i]) {
        usb_host_transfer_free(rx_transfers_[i]);
        rx_transfers_[i] = nullptr;
      }
    }
    rx_recovery_mask_.store(0);
    tx_recovery_pending_.store(false);
    data_release_pending_.store(false);
    return true;
  }

  void suspendDataPath() {
    data_path_active_.store(false);

    TxFrame* frame = nullptr;
    while (tx_queue_ && xQueueReceive(tx_queue_, &frame, 0) == pdTRUE) {
      heap_caps_free(frame);
    }

    const bool has_rx_transfers =
        rx_transfers_[0] != nullptr || rx_transfers_[1] != nullptr;
    if (dev_ && has_rx_transfers) {
      usb_host_endpoint_halt(dev_, bulk_in_endpoint_);
      usb_host_endpoint_flush(dev_, bulk_in_endpoint_);
      usb_host_endpoint_clear(dev_, bulk_in_endpoint_);
    }
    if (dev_ && tx_transfer_) {
      usb_host_endpoint_halt(dev_, bulk_out_endpoint_);
      usb_host_endpoint_flush(dev_, bulk_out_endpoint_);
      usb_host_endpoint_clear(dev_, bulk_out_endpoint_);
    }

    const uint32_t wait_started = millis();
    while ((tx_in_flight_.load() || anyRxBusy()) &&
           millis() - wait_started < 1000) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (releaseDataTransfers()) {
      if (has_rx_transfers || tx_transfer_) {
        Serial.println("[USB-ETH] Data buffers released for Wi-Fi");
      }
    } else {
      data_release_pending_.store(true);
      Serial.println("[USB-ETH] Data buffer release deferred");
    }

    // Mirror the carrier-off sequence used by the reference RTL8156 driver:
    // stop/gate the NIC and reset its buffer-management unit after all host
    // transfers are gone. Otherwise the next RX URB can stay silent.
    if (attached_.load() && dev_) {
      rtl8153bNicReset();
      adapter_data_path_ready_ = false;
      Serial.println("[USB-ETH] RTL8156 data path stopped");
    }
  }

  void serviceDataPath() {
    if (data_release_pending_.load() && releaseDataTransfers()) {
      Serial.println("[USB-ETH] Data buffers released for Wi-Fi");
    }

    if (!netif_connect_pending_.load() || !netif_started_ ||
        !attached_.load() || !link_up_.load()) {
      return;
    }
    if (static_cast<int32_t>(millis() - netif_connect_at_) < 0) return;

    if (!adapter_data_path_ready_ && !enableAdapterDataPathHardware()) {
      netif_connect_at_ = millis() + 1000;
      Serial.println("[USB-ETH] RTL8156 data path not ready yet");
      return;
    }

    if (data_release_pending_.load() || !allocateDataTransfers()) {
      netif_connect_at_ = millis() + 1000;
      Serial.println("[USB-ETH] Data buffers not available yet");
      return;
    }

    data_path_active_.store(true);
    rx_completed_.store(0);
    tx_submitted_.store(0);
    size_t rx_ready = 0;
    for (size_t i = 0; i < kRxTransferCount; ++i) {
      if (rx_transfers_[i] && submitRx(i)) ++rx_ready;
    }
    if (rx_ready == 0) {
      suspendDataPath();
      netif_connect_at_ = millis() + 1000;
      return;
    }

    netif_connect_pending_.store(false);
    esp_netif_action_connected(netif_, nullptr, 0, nullptr);
    Serial.println(static_ip_active_
                       ? "[USB-ETH] Ethernet with static IP connected"
                       : "[USB-ETH] Ethernet DHCP started");
  }

  void cleanupDevice(bool device_gone) {
    attached_.store(false);
    netif_connect_pending_.store(false);
    setLink(false, 0);

    TxFrame* frame = nullptr;
    while (tx_queue_ && xQueueReceive(tx_queue_, &frame, 0) == pdTRUE) {
      heap_caps_free(frame);
    }

    const uint32_t wait_started = millis();
    while ((tx_in_flight_.load() || anyRxBusy()) &&
           millis() - wait_started < 1000) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (netif_started_) {
      esp_netif_action_stop(netif_, nullptr, 0, nullptr);
      netif_started_ = false;
    }

    releaseDataTransfers();
    if (control_transfer_) {
      usb_host_transfer_free(control_transfer_);
      control_transfer_ = nullptr;
    }

    if (dev_) {
      if (interface_claimed_) {
        const esp_err_t release_err = usb_host_interface_release(
            client_, dev_, interface_number_);
        if (release_err != ESP_OK && !device_gone) {
          Serial.printf("[USB-ETH] Interface release failed: %s\n",
                        esp_err_to_name(release_err));
        }
      }
      const esp_err_t close_err = usb_host_device_close(client_, dev_);
      if (close_err != ESP_OK && !device_gone) {
        Serial.printf("[USB-ETH] Device close failed: %s\n",
                      esp_err_to_name(close_err));
      }
    }

    dev_ = nullptr;
    interface_claimed_ = false;
    interface_number_ = 0xff;
    bulk_in_endpoint_ = 0;
    bulk_out_endpoint_ = 0;
    bulk_in_mps_ = 0;
    chip_version_ = 0;
    is_8156b_ = false;
    io_ok_ = true;
    link_read_failures_ = 0;
    rx_recovery_mask_.store(0);
    tx_recovery_pending_.store(false);
    tx_in_flight_.store(false);
    data_path_active_.store(false);
    data_release_pending_.store(false);
    adapter_data_path_ready_ = false;
    rx_completed_.store(0);
    tx_submitted_.store(0);
    for (auto& in_flight : rx_in_flight_) in_flight.store(false);
    for (auto& callback_active : rx_callback_active_) {
      callback_active.store(false);
    }
  }

  bool anyRxBusy() const {
    for (size_t i = 0; i < kRxTransferCount; ++i) {
      if (rx_in_flight_[i].load() || rx_callback_active_[i].load()) {
        return true;
      }
    }
    return false;
  }

  std::atomic<bool> begun_{false};
  std::atomic<bool> started_{false};
  std::atomic<bool> host_installed_{false};
  std::atomic<bool> client_registered_{false};
  std::atomic<bool> attached_{false};
  std::atomic<bool> link_up_{false};
  std::atomic<bool> netif_connect_pending_{false};
  std::atomic<bool> data_path_active_{false};
  std::atomic<bool> data_release_pending_{false};
  std::atomic<uint32_t> rx_completed_{0};
  std::atomic<uint32_t> tx_submitted_{0};
  std::atomic<int> enumerated_devices_{0};
  uint32_t netif_connect_at_ = 0;
  bool adapter_data_path_ready_ = false;

  esp_netif_t* netif_ = nullptr;
  bool static_ip_active_ = false;
  NetifGlue glue_;
  bool netif_started_ = false;

  QueueHandle_t event_queue_ = nullptr;
  QueueHandle_t tx_queue_ = nullptr;
  SemaphoreHandle_t host_ready_ = nullptr;
  SemaphoreHandle_t client_ready_ = nullptr;
  SemaphoreHandle_t control_done_ = nullptr;
  TaskHandle_t host_task_ = nullptr;
  TaskHandle_t client_task_ = nullptr;
  TaskHandle_t worker_task_ = nullptr;

  usb_host_client_handle_t client_ = nullptr;
  usb_device_handle_t dev_ = nullptr;
  bool interface_claimed_ = false;
  uint8_t interface_number_ = 0xff;
  uint8_t bulk_in_endpoint_ = 0;
  uint8_t bulk_out_endpoint_ = 0;
  uint16_t bulk_in_mps_ = 0;

  usb_transfer_t* control_transfer_ = nullptr;
  usb_transfer_t* rx_transfers_[kRxTransferCount] = {};
  usb_transfer_t* tx_transfer_ = nullptr;
  RxContext rx_contexts_[kRxTransferCount] = {};
  std::atomic<bool> rx_in_flight_[kRxTransferCount] = {};
  std::atomic<bool> rx_callback_active_[kRxTransferCount] = {};
  std::atomic<bool> tx_in_flight_{false};
  std::atomic<uint32_t> rx_recovery_mask_{0};
  std::atomic<bool> tx_recovery_pending_{false};

  uint16_t chip_version_ = 0;
  bool is_8156b_ = false;
  bool io_ok_ = true;
  uint8_t mac_[6] = {};
  uint8_t link_read_failures_ = 0;
  uint32_t next_link_poll_ms_ = 0;
  uint32_t next_rx_upgrade_ms_ = 0;
};

Rtl8156UsbDriver g_rtl8156_driver;

}  // namespace

#endif  // CONFIG_IDF_TARGET_ESP32P4

UsbEthernetBackend usbEthernetBackend;

bool UsbEthernetBackend::begin() {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  if (!Device::kCapabilities.supports_usb_host_network) return false;
  return g_rtl8156_driver.begin();
#else
  return false;
#endif
}

bool UsbEthernetBackend::isStarted() const {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  return g_rtl8156_driver.isStarted();
#else
  return false;
#endif
}

UsbEthernetSnapshot UsbEthernetBackend::snapshot() const {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  return g_rtl8156_driver.snapshot();
#else
  return UsbEthernetSnapshot{};
#endif
}
