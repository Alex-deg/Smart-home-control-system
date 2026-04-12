#include <iostream>
#include "database/DataBase.h"
#include "api/api.h"
#include <nlohmann/json.hpp>
#include "mqtt/MQTTClient.h"

using json = nlohmann::json;

int main(){

    DataBase db;
    db.open("../Data/smart_home.db");

    MQTTClient mqtt(db);  // db передается по ссылке

    if (!mqtt.connect("127.0.0.1", 1883)) {
        return 1;
    }
    
    mqtt.startLoop();

    API api(db, mqtt);
    std::thread api_thread([&api]() {
        std::cout << "Starting HTTP API server..." << std::endl;
        api.run(); 
    });

    while (true) {
        if (!mqtt.isConnected()) {
            std::cout << "Mqtt connection is lost..." << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    api_thread.join();
    mqtt.stopLoop();

    return 0;
}