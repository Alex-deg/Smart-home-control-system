#pragma once

#include <WiFi.h>

namespace Settings{

    namespace Network{
        const char* ssid          = "";                   // Network SSID 
        const char* password      = "";                   // Password
        const char* server_ip     = "x.x.x.x";            // MQTT broker IP
        const char* mqtt_port     = "1883";               // MQTT broker port
        const char* api_port      = "8080";               // HTTP API port
        const bool hidden_network = true;                 // true if network is hidden
    }
    
    namespace API{
        const String BASE_API_URL          = "http://" + String(Network::server_ip) + ":" + String(Network::api_port);  // base URL
        const char* auto_detect_endpoint   = "/api/auto-detect";    
        const char* get_telemetry_endpoint = "/api/database/telemetry";
    }
    
    namespace MQTT{
        const char* mqtt_topic_sub       = "smart_home/R2D2C3";            // For remote commands
        const char* mqtt_topic_telemetry = "rpi/database/save/telemetry";  // For sending telemetry
        const char* mqtt_topic_params    = "rpi/database/save/params";     // For sending diagnostic data
        const char* mqtt_topic_remote    = "rpi/send_message/remote";      // For sending response to the remote server
        const char* mqtt_topic_local     = "rpi/send_message/local/api";   // For sending response to the local server
    }
    
    namespace Module{
        const char* name        = "climate-control";    // Module name
        const char* alias       = "hall";               // Module alias
        const char* description = "";                   // Module description
    }
    
    namespace Streaming{
        int telemetry_interval = 60 * 1000;       // Telemetry sending interval (ms)
        int params_interval    = 60 * 60 * 1000;  // Dignostic data sending interval (ms)           
    }

    namespace Sensors{
        const int ledPin  = 2;         // Built-in LED
        const int DHTPIN  = 4;         // DHT sensor pin
        const int DHTTYPE = DHT11;     // DHT type 
    }
}
