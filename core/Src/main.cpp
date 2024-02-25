#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "main.h"
#include <thread>
#include <queue>
#include <semaphore>
#include <atomic>

#include "esp_vfs_dev.h"
#include "driver/uart.h"


template <typename Logger>
class WifiSniffer {
public:
    WifiSniffer(Logger& logger) : logger_(logger), tag_("wifi-sniffer") {}

    void userInput()
    {
        char ch;
        do
        {
        
        if(init_uart()!=ESP_OK)
                return;
            char userTargetMac[18];
            std::cout<<"Please enter target MAC:\n";   
            std::cin>> userTargetMac;
            std::cout<<"UserTargetMac is:"<<userTargetMac<<"\n";

        if (sscanf(userTargetMac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &targetMac[0], &targetMac[1], &targetMac[2],
               &targetMac[3], &targetMac[4], &targetMac[5]) != 6)
        {
            std::cout<<"Invalid MAC address format.\n";
            return;
        }
        
        flag = true;
        std::thread thread2(&WifiSniffer<Logger>::start,this);
        std::cin>>ch;
        flag = false;
        thread2.join();
        }
        while(ch!='n');

        return;           
    }
    
    
    void start() {
        ESP_LOGI(tag_, "Starting WiFi Sniffer");  

        init();

        while (1) {

            for (int channel = 10; channel <= 14; channel++) {
                if(!flag) 
                {
                    esp_wifi_stop();
                    break;
                }    
                if(flag)
                {
                hopChannel(channel);
                vTaskDelay(3000 / portTICK_PERIOD_MS);
                }  // Delay for 5 seconds between channel hops
            }

            if(!flag)
            break;
        }
        return;
    }

private:

    uint8_t targetMac[6];// = {0x04, 0x95, 0xe6, 0xf4, 0xfa, 0x11};  10-5B-AD-52-AD-81 ; 90-65-84-58-F3-8B
    std::atomic<bool> flag = true;
    int packetno=0;
    //static std::queue<std::pair<void *,void *>> q;
    //std::binary_semaphore prepareSignal(0);
    static WifiSniffer<Logger>* instance_;  // Static member variable to store the instance

    static void packetHandler(void* buff, wifi_promiscuous_pkt_type_t type) {
        //q.push({buff,(void *)type});
        //prepareSignal.release();
        instance_->handlePacket(buff, type);
    }

    //void 

    void handlePacket(void* buff, wifi_promiscuous_pkt_type_t type) {

        //prepareSignal.acquire();
        //void *buff = q.front().first;
        //void * type = q.front().second;
         //type = reinterpret_cast<wifi_promiscuous_pkt_type_t>(type);
        //q.pop();
        //if (type == WIFI_PKT_MGMT) {
            wifi_promiscuous_pkt_t *pkt = reinterpret_cast<wifi_promiscuous_pkt_t *>(buff);
            wifi_ieee80211_packet_t *ipkt = reinterpret_cast<wifi_ieee80211_packet_t *> (pkt->payload);
            wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

            // Extract source MAC address
            // Adjust the offset based on the frame structure
            // 

            if(std::equal(hdr->addr1, hdr->addr1 + 6, std::begin(targetMac))||std::equal(hdr->addr2, hdr->addr2 + 6, std::begin(targetMac))
            ||std::equal(hdr->addr3, hdr->addr3 + 6, std::begin(targetMac))||std::equal(hdr->addr4, hdr->addr4 + 6, std::begin(targetMac)))
            {
            
            //std::cout.setf(std::ios_base::binary);
            //Serial.write(*hdr);
            logger_.log("Packet No: ",++packetno);
            logger_.log("Receiver MAC Address", hdr->addr1,6);
            logger_.log("Sender MAC Address", hdr->addr2,6);
            logger_.log("Filtering MAC Address", hdr->addr3,6);

            // Extract relevant information from the packet
            auto rssi = pkt->rx_ctrl.rssi;
            auto chan = pkt->rx_ctrl.channel;
            // uint16_t framecontrol = hdr->frame_ctrl;
            // uint8_t bit_2 = BitVal(framecontrol,2); 
            // uint8_t bit_3 = BitVal(framecontrol,3);

            std::cout<<"Packet Type: ";
            if(type == WIFI_PKT_MGMT)
            std::cout<<"MANAGEMENT\n";
            else if(type == WIFI_PKT_CTRL)
            std::cout<<"CONTROL\n";
            else if(type == WIFI_PKT_DATA)
            std::cout<<"DATA\n";
            else
            std::cout<<"EXTENSION\n";

            //std::cout<<"Confirmation :"<<type<<"\n"; 
            //auto timestamp = pkt->rx_ctrl.timestamp;
            //auto cwb = "";
            // if(pkt->rx_ctrl.cwb==0)
            // cwb = "20MHz";
            // else if(pkt->rx_ctrl.cwb==1)
            // cwb = "40MHz";
            logger_.log("RSSI", rssi);
            logger_.log("CHANNEL",chan);
            std::cout<<"\n";
            //logger_.log("CHANNEL BW",cwb);
            //logger_.log("TIMESTAMP",timestamp);

            //size_t packet_len = pkt->rx_ctrl.sig_len;
            // Logging the first 16 bytes of payload
            //logger_.log("Packet Payload (first 16 bytes)", pkt->payload, std::min((std::size_t)16, packet_len));
        }
         //vTaskDelay(1000/ portTICK_PERIOD_MS);
    //} 
    }

    esp_err_t init_uart()
    {
        
    static bool configured = false;
    if (configured) {
      return ESP_OK;
    }
    // Initialize VFS & UART so we can use std::cout/cin
    setvbuf(stdin, NULL, _IONBF, 0);
    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install( (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM,
            256, 0, 0, NULL, 0) );
   /* Tell VFS to use UART driver */
        esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
        esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
        esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
    configured = true;
    return ESP_OK;

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

    std::thread thread1(&WifiSniffer<ConsoleLogger>::userInput,&wifiSniffer);
    thread1.join();

    return;
}




// #include <iostream> 34 53 d2 b9 e8 a5
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/event_groups.h"
// #include "esp_wifi.h"
// #include "esp_log.h"
// #include "nvs_flash.h"
// #include "main.h"

// template <typename Logger>
// class WifiSniffer {
// public:
//     WifiSniffer(Logger& logger) : logger_(logger), tag_("wifi-sniffer") {}

//     void start() {
//         // Start the WiFi Sniffing task
//         xTaskCreate(&WifiSniffer::sniffTask, "sniff_task", 4096, this, 5, NULL);

//         // Start the Console Printing task
//         //xTaskCreate(&WifiSniffer::printTask, "print_task", 1024, this, 5, NULL);

//         // Start the scheduler
//         //vTaskStartScheduler();
//     }

// private:
//     static WifiSniffer<Logger>* instance_;  // Static member variable to store the instance
//     static TaskHandle_t printTaskHandle;     // Task handle for the print task

//     static void sniffTask(void* pvParameters) {
//         instance_->init();
//         while (1) {
//             for (int channel = 1; channel <= WIFI_CHANNEL_MAX; channel++) {
//                 instance_->hopChannel(channel);
//                 vTaskDelay(5000 / portTICK_PERIOD_MS);  // Delay for 5 seconds between channel hops
//             }
//         }
//     }

//     static void printTask(void* pvParameters) {
//         while (1) {
//             instance_->printToSerial();
//             vTaskDelay(3000 / portTICK_PERIOD_MS);  // Delay for 1 second between prints
//         }
//     }

//     void printToSerial() {
//         // Your printing logic here
//         logger_.log("Printing from printTask", "Hello from printTask");
//     }

//         static void packetHandler(void* buff, wifi_promiscuous_pkt_type_t type) {
//          instance_->handlePacket(buff, type);
//         }

//     void handlePacket(void* buff, wifi_promiscuous_pkt_type_t type) {
//         if (type == WIFI_PKT_MGMT) {
//             wifi_promiscuous_pkt_t *pkt = reinterpret_cast<wifi_promiscuous_pkt_t *>(buff);
//             wifi_ieee80211_packet_t *ipkt = reinterpret_cast<wifi_ieee80211_packet_t *> (pkt->payload);
//             wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

//             // Extract source MAC address
//             // Adjust the offset based on the frame structure

//             uint8_t targetMac[] = {0x04, 0x95, 0xe6, 0xf4, 0xfa, 0x11};

//             if(std::equal(hdr->addr2, hdr->addr2 + 6, std::begin(targetMac)))
//             {
//             logger_.log("Receiver MAC Address", hdr->addr1,6);
//             logger_.log("Sender MAC Address", hdr->addr2,6);
//             logger_.log("Filtering MAC Address", hdr->addr3,6);

//             // Extract relevant information from the packet
//             auto rssi = pkt->rx_ctrl.rssi;
//             auto chan = pkt->rx_ctrl.channel;
//             auto timestamp = pkt->rx_ctrl.timestamp;
//             auto cwb = "";
//             if(pkt->rx_ctrl.cwb==0)
//             cwb = "20MHz";
//             else if(pkt->rx_ctrl.cwb==1)
//             cwb = "40MHz";
//             logger_.log("RSSI", rssi);
//             logger_.log("CHANNEL",chan);
//             logger_.log("CHANNEL BW",cwb);
//             logger_.log("TIMESTAMP",timestamp);

//             size_t packet_len = pkt->rx_ctrl.sig_len;
//             // Logging the first 16 bytes of payload
//             logger_.log("Packet Payload (first 16 bytes)", pkt->payload, std::min((std::size_t)16, packet_len));
//         }
//     }
//          vTaskDelay(1000/ portTICK_PERIOD_MS);
//     }

//     void init() {
//         // Initialize NVS
//         esp_err_t ret = nvs_flash_init();
//         if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//             // Handle error or version mismatch, erase and retry
//             ESP_ERROR_CHECK(nvs_flash_erase());
//             ret = nvs_flash_init();
//         }
//         ESP_ERROR_CHECK(ret);

//         instance_ = this;  // Set the instance to the current object
//         wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//         esp_wifi_init(&cfg);
//         esp_wifi_set_storage(WIFI_STORAGE_RAM);
//         esp_wifi_set_mode(WIFI_MODE_NULL);
//         esp_wifi_start();

//         esp_wifi_set_promiscuous(true);
//         esp_wifi_set_promiscuous_rx_cb(&packetHandler);
//     }

//     void hopChannel(int channel) {
//         wifi_promiscuous_filter_t filter = {
//             .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL,
//         };
//         esp_wifi_set_promiscuous_filter(&filter);
//         esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
//         ESP_LOGI(tag_, "Sniffing on Channel: %d", channel);
//     }

// private:
//     Logger& logger_;
//     const char *tag_;
// };

// template <typename Logger>
// WifiSniffer<Logger>* WifiSniffer<Logger>::instance_ = nullptr;  // Initialize the static member variable

// template <typename Logger>
// TaskHandle_t WifiSniffer<Logger>::printTaskHandle = NULL;  // Initialize the static member variable

// class ConsoleLogger {
// public:
//     template <typename T>
//     void log(const char* label, T value) const {
//         std::cout << label << ": " << value << std::endl;
//     }

//     void log(const char* label, const uint8_t* data, size_t length) const {
//         std::cout << label << ": ";
//         for (size_t i = 0; i < length; i++) {
//             printf("%02x ", data[i]);
//         }
//         printf("\n");
//     }
// };

// extern "C" void app_main() {
//     ConsoleLogger consoleLogger;
//     WifiSniffer<ConsoleLogger> wifiSniffer(consoleLogger);
//     wifiSniffer.start();
// }

// #include <iostream>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// extern "C" void app_main() {
//     xTaskCreate([](void* param) {
//         int taskId = reinterpret_cast<int>(param);
//         while (1) {
//             std::cout << "Thread " << taskId << ": Hello from thread!" << std::endl;
//             vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for 1 second
//         }
//     }, "Thread 1", 4096, reinterpret_cast<void*>(1), 1, nullptr);

//     xTaskCreate([](void* param) {
//         int taskId = reinterpret_cast<int>(param);
//         while (1) {
//             std::cout << "Thread " << taskId << ": Hi there, I'm another thread!" << std::endl;
//             vTaskDelay(1500 / portTICK_PERIOD_MS);  // Delay for 1.5 seconds
//         }
//     }, "Thread 2", 4096, reinterpret_cast<void*>(2), 1, nullptr);

//     xTaskCreate([] (void* param){
//         int taskId = reinterpret_cast<int>(param);
//         while(1)
//         {
//             std::cout<<"Thread"<<taskId <<"Says hello!"<<std::endl;
//             vTaskDelay(1500/portTICK_PERIOD_MS);
//         }
//     }, "Thread3", 4096, reinterpret_cast<void*>(3),1,nullptr);
// }

// #include <iostream>
// #include <functional>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include <thread>
// //#include "esp32-hal.h"

// void threadFunction(int threadId) {
//     while (1) {
//         std::cout << "Thread " << threadId << ": Hello from thread!" << std::endl;
//         vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for 1 second
//     }
// }

// void anotherThreadFunction(int threadId) {
//     while (1) {
//         std::cout << "Thread " << threadId << ": Hi there, I'm another thread!" << std::endl;
//         vTaskDelay(1500 / portTICK_PERIOD_MS);  // Delay for 1.5 seconds
//     }
// }

// extern "C" void app_main() {
//     std::thread thread1(threadFunction, 1);
//     std::thread thread2(anotherThreadFunction, 2);

//     while (1) {
//         // Your main application logic here
//         vTaskDelay(1000 / portTICK_PERIOD_MS);  // Example: Main loop with 1-second delay
//     }
// }



