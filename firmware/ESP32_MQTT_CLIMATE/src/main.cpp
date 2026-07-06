#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <iostream>
#include <HTTPClient.h>
#include <esp_adc_cal.h>
#include <esp_system.h>
#include "../include/settings.hpp"

int module_id  = -1; // Module ID 
bool debugFlag = true;  

// Global objects
WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(Settings::Sensors::DHTPIN, Settings::Sensors::DHTTYPE);

// JSON документы для ArduinoJSON
StaticJsonDocument<128>  tempDoc;        // JSON for temperature
StaticJsonDocument<128>  humidityDoc;    // JSON for humidity
StaticJsonDocument<4096> telemetryResp;  // JSON for API response
StaticJsonDocument<512>  commandDoc;     // JSON for remote commands

char jsonBuffer[256];

void reconnectMQTT() {
    while (!client.connected()) {

        Serial.print("Connecting to MQTT...");
        
        String clientId = "ESP32_DHT11_" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str())) {
            client.subscribe(Settings::MQTT::mqtt_topic_sub);
            Serial.println("Connect has been established successfully!");
        } else {
            Serial.print("ERROR, code = ");
            Serial.print(client.state());
            Serial.println(" reconnect after 5 seconds...");
            delay(5000);
        }
    }
}

void autoDetect(){

    StaticJsonDocument<512> data;
    
    data["name"]        = Settings::Module::name;
    data["alias"]       = Settings::Module::alias;
    data["mqtt_topic"]  = Settings::MQTT::mqtt_topic_sub;
    data["description"] = Settings::Module::description;
    
    HTTPClient http;
    
    String url = Settings::API::BASE_API_URL + Settings::API::auto_detect_endpoint;
    if (debugFlag)
        printf("URL = %s", url);

    if(debugFlag){
        Serial.print("HTTP Request URL: ");
        Serial.println(url);
    }

    http.begin(url);

    String requestBody;
    serializeJson(data, requestBody);

    // Session limit
    http.setTimeout(10000);
    
    int httpCode = http.POST(requestBody);

    if(debugFlag){
        Serial.print("HTTP Response Code: ");
        Serial.println(httpCode);
    }

    if (httpCode == 200) {

        String payload = http.getString();
        if(debugFlag){
            Serial.print("Raw payload length: ");
            Serial.println(payload.length());
        }

        if (payload.length() == 0) {
            Serial.println("Empty payload received!");
            http.end();
        }

        data.clear();
        DeserializationError error = deserializeJson(data, payload);
        
        if (error) {
            Serial.print("JSON parse error: ");
            Serial.println(error.c_str());
            http.end();
        }
        module_id = data["module_id"];
    }
    else{
        Serial.printf("HTTP request failed with code: %d\n", httpCode);
        String errorBody = http.getString();
        if (errorBody.length() > 0) {
            Serial.print("Error body: ");
            Serial.println(errorBody);
        }
    }
    http.end();
}

JsonArray getTelemetryFromServer(long long module_id, const String &param_name, long long time_interval) {
    
    HTTPClient http;
    
    String url = Settings::API::BASE_API_URL + Settings::API::get_telemetry_endpoint;
    url += "?module_id=" + String(module_id);
    url += "&param_name=" + param_name;
    url += "&time_interval=" + String(time_interval);
    
    if(debugFlag){
        Serial.print("HTTP Request URL: ");
        Serial.println(url);
    }

    http.begin(url);

    // Session limit
    http.setTimeout(10000);
    
    int httpCode = http.GET();
    if(debugFlag){
        Serial.print("HTTP Response Code: ");
        Serial.println(httpCode);
    }
    
    if (httpCode == 200) {

        String payload = http.getString();
        
        if(debugFlag){
            Serial.print("Raw payload length: ");
            Serial.println(payload.length());
        }

        if (payload.length() == 0) {
            Serial.println("Empty payload received!");
            http.end();
            return JsonArray();
        }

        if(debugFlag){
            String debugPayload = payload.substring(0, min(500, (int)payload.length()));
            Serial.print("Raw payload (first 500 chars): ");
            Serial.println(debugPayload);
        }

        // Clearing JSON object before parsing
        telemetryResp.clear();
        
        DeserializationError error = deserializeJson(telemetryResp, payload);
        
        if (error) {
            Serial.print("JSON parse error: ");
            Serial.println(error.c_str());
            http.end();
            return JsonArray();
        }
        
        if (!telemetryResp.is<JsonArray>()) {
            Serial.println("Response is not a JSON array");
            if(debugFlag){
                Serial.print("Response type: ");
                if (telemetryResp.is<JsonObject>()) 
                    Serial.println("Object");
                else 
                if (telemetryResp.is<JsonArray>()) 
                    Serial.println("Array");
                else Serial.println("Other");
            }
            http.end();
            return JsonArray();
        }
        
        JsonArray array = telemetryResp.as<JsonArray>();
        if(debugFlag){
            Serial.print("Array size: ");
            Serial.println(array.size());
        }
        
        http.end();
        return array;
    }
    
    Serial.printf("HTTP request failed with code: %d\n", httpCode);
    
    String errorBody = http.getString();
    if (errorBody.length() > 0) {
        Serial.print("Error body: ");
        Serial.println(errorBody);
    }
    
    http.end();
    return JsonArray(); 
}

void processCommand(const String& message) {

    if(debugFlag)
        Serial.println("Processing command: " + message);
    
    StaticJsonDocument<512> cmdDoc;
    
    DeserializationError error = deserializeJson(cmdDoc, message);
    
    if (error) {
        Serial.print("Command JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }
    
    String payload = cmdDoc["payload"] | "";
    
    StaticJsonDocument<8192> responseDoc;
    responseDoc.clear();
    responseDoc["type"] = "response";
    responseDoc["request_id"] = cmdDoc["request_id"];

    if (payload == "get_data") {

        int timeInterval = 2; // Time interval (min)
        JsonArray telemetryArray = getTelemetryFromServer(module_id, "temperature", timeInterval);
                
        JsonArray responseArray = responseDoc.createNestedArray("payload");
        for (JsonVariant item : telemetryArray) {
            responseArray.add(item);
        }
    } 
    else
    if (payload == "turn_on") {
        digitalWrite(Settings::Sensors::ledPin, HIGH);
        Serial.println("💡 LED ON");
        responseDoc["payload"] = "LED IS ON";
    } 
    else 
    if (payload == "turn_off") {
        digitalWrite(Settings::Sensors::ledPin, LOW);
        Serial.println("💡 LED OFF");
        responseDoc["payload"] = "LED IS OFF";
    } 
    else {
        Serial.println("Unknown command: " + payload);
        return;
    }

    String responseStr, responseTopic;
    serializeJson(responseDoc, responseStr);
    if (cmdDoc["internet"])
        responseTopic = Settings::MQTT::mqtt_topic_remote;
    else responseTopic = Settings::MQTT::mqtt_topic_local;

    if (client.publish(responseTopic.c_str(), responseStr.c_str())) {
        Serial.println("Response sent to remote topic");
    } else {
        Serial.println("Failed to send response");
    }
}

void sendTelemetry() {  

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (isnan(t) || isnan(h)) {
        Serial.println("DHT reading error!");
        return;
    }
    
    if(debugFlag){
        Serial.print("Temperature: ");
        Serial.print(t);
        Serial.print(" °C, Humidity: ");
        Serial.print(h);
        Serial.println(" %");
    }
    
    tempDoc["param_value"] = t;
    serializeJson(tempDoc, jsonBuffer);
    if (client.publish(Settings::MQTT::mqtt_topic_telemetry, jsonBuffer)) {
        if(debugFlag){
            Serial.print("Temperature sent: ");
            Serial.println(jsonBuffer);
        }
    } else {
        Serial.println("Failed to send temperature");
    }
    
    delay(100); 
    
    humidityDoc["param_value"] = h;
    serializeJson(humidityDoc, jsonBuffer);
    if (client.publish(Settings::MQTT::mqtt_topic_telemetry, jsonBuffer)) {
        if(debugFlag){
            Serial.print("Humidity sent: ");
            Serial.println(jsonBuffer);
        }
    } else {
        Serial.println("Failed to send humidity");
    }
}

void sendDiagnostics() {
    
    StaticJsonDocument<256> doc;
    
    doc["module_id"]   = module_id;
    doc["module_temp"] = temperatureRead();    // ESP32 temperature
    doc["free_bytes"]  = ESP.getFreeHeap();    // Free bytes

    String output;
    serializeJson(doc, output);
    client.publish(Settings::MQTT::mqtt_topic_params, output.c_str());
}

void callback(char* topic, byte* payload, unsigned int length) {
    
    if(debugFlag){
        Serial.println("\n=== MQTT CALLBACK TRIGGERED ===");
        Serial.print("Topic: ");
        Serial.println(topic);
    }

    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    if(debugFlag){
        Serial.print("Message: ");
        Serial.println(message);
    }

    processCommand(message);
}

void setup() {

    Serial.begin(115200);
    
    delay(100);

    pinMode(Settings::Sensors::ledPin, OUTPUT);
    digitalWrite(Settings::Sensors::ledPin, LOW);   
    
    dht.begin();

    if(debugFlag)
        Serial.println("DHT sensor initialized");
    
    if(debugFlag){
        Serial.print("Connecting to WiFi: ");
        Serial.println(Settings::Network::ssid);
    }

    WiFi.begin(Settings::Network::ssid, Settings::Network::password, 0, NULL, Settings::Network::hidden_network);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        if(debugFlag){
            Serial.println("\nWiFi connected!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
        }
    } else {
        Serial.println("\nWiFi connection failed!");
    }
    
    client.setServer(Settings::Network::server_ip, atoi(Settings::Network::mqtt_port));
    client.setCallback(callback);
    
    autoDetect();

    tempDoc["module_id"] = module_id;
    tempDoc["param_name"] = "temperature";
    tempDoc["meas_unit"] = "°C";

    humidityDoc["module_id"] = module_id;
    humidityDoc["param_name"] = "humidity";
    humidityDoc["meas_unit"] = "%";

    if(debugFlag)
        Serial.println("\nSetup complete, starting main loop...\n");

}

void loop() {

    if (!client.connected()) {
        reconnectMQTT();
    }

    client.loop();
    
    static unsigned long lastTelemetryTime = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastTelemetryTime > Settings::Streaming::telemetry_interval) {
        lastTelemetryTime = currentTime;
        sendTelemetry();
    }

    static unsigned long lastDiagnostic = 0;

    if (millis() - lastDiagnostic > Settings::Streaming::params_interval) { 
        lastDiagnostic = millis();
        sendDiagnostics();
    }

}