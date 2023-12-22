#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "main.h"

template <typename Logger>
class WifiSniffer {
public:
    WifiSniffer(Logger& logger) : logger_(logger), tag_("wifi-sniffer") {}

    void start() {
        ESP_LOGI(tag_, "Starting WiFi Sniffer");
        init();
        while (1) {
            for (int channel = 1; channel <= WIFI_CHANNEL_MAX; channel++) {
                hopChannel(channel);
                vTaskDelay(5000 / portTICK_PERIOD_MS);  // Delay for 5 seconds between channel hops
            }
        }
    }

private:
    static WifiSniffer<Logger>* instance_;  // Static member variable to store the instance

    static void packetHandler(void* buff, wifi_promiscuous_pkt_type_t type) {
        instance_->handlePacket(buff, type);
    }

    void handlePacket(void* buff, wifi_promiscuous_pkt_type_t type) {
        if (type == WIFI_PKT_MGMT) {
            wifi_promiscuous_pkt_t *pkt = reinterpret_cast<wifi_promiscuous_pkt_t *>(buff);
            wifi_ieee80211_packet_t *ipkt = reinterpret_cast<wifi_ieee80211_packet_t *> (pkt->payload);
            wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

            // Extract source MAC address
            // Adjust the offset based on the frame structure
            logger_.log("Receiver MAC Address", hdr->addr1,6);
            logger_.log("Sender MAC Address", hdr->addr2,6);
            logger_.log("Filtering MAC Address", hdr->addr3,6);

            // Extract relevant information from the packet
            auto rssi = pkt->rx_ctrl.rssi;
            auto chan = pkt->rx_ctrl.channel;
            auto timestamp = pkt->rx_ctrl.timestamp;
            auto cwb = "";
            if(pkt->rx_ctrl.cwb==0)
            cwb = "20MHz";
            else if(pkt->rx_ctrl.cwb==1)
            cwb = "40MHz";
            logger_.log("RSSI", rssi);
            logger_.log("CHANNEL",chan);
            logger_.log("CHANNEL BW",cwb);
            logger_.log("TIMESTAMP",timestamp);

            size_t packet_len = pkt->rx_ctrl.sig_len;
            // Logging the first 16 bytes of payload
            logger_.log("Packet Payload (first 16 bytes)", pkt->payload, std::min((std::size_t)16, packet_len));
        }
    }

    void init() {
        // Initialize NVS
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            // Handle error or version mismatch, erase and retry
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        instance_ = this;  // Set the instance to the current object
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        esp_wifi_set_mode(WIFI_MODE_NULL);
        esp_wifi_start();

        esp_wifi_set_promiscuous(true);
        esp_wifi_set_promiscuous_rx_cb(&packetHandler);  // Set the callback function
    }

    void hopChannel(int channel) {
        wifi_promiscuous_filter_t filter = {
            .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL,
        };
        esp_wifi_set_promiscuous_filter(&filter);
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        ESP_LOGI(tag_, "Sniffing on Channel: %d", channel);
    }

private:
    Logger& logger_;
    const char *tag_;
};

template <typename Logger>
WifiSniffer<Logger>* WifiSniffer<Logger>::instance_ = nullptr;  // Initialize the static member variable

class ConsoleLogger {
public:
    template <typename T>
    void log(const char* label, T value) const {
        std::cout << label << ": " << value << std::endl;
    }

    void log(const char* label, const uint8_t* data, size_t length) const {
        std::cout << label << ": ";
        for (size_t i = 0; i < length; i++) {
            printf("%02x ", data[i]);
        }
        printf("\n");
    }
};

extern "C" void app_main() {
    ConsoleLogger consoleLogger;
    WifiSniffer<ConsoleLogger> wifiSniffer(consoleLogger);
    wifiSniffer.start();
}
