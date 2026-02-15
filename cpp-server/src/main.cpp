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

    // 3. Настройка callback'ов (необязательно)
    mqtt.setOnConnectCallback([]() {
        std::cout << "Бизнес-логика: Система готова!" << std::endl;
    });
    mqtt.setOnMessageCallback([](const std::string& topic, const std::string& payload) {
        std::cout << "Бизнес-логика: Получено " << topic << std::endl;
    });
    // 4. Подключение (неблокирующее)
    if (!mqtt.connect("127.0.0.1", 1883)) {
        // Ошибка инициализации подключения
        return 1;
    }
    
    mqtt.startLoop();

    API api(db, mqtt);
    std::thread api_thread([&api]() {
        std::cout << "Starting HTTP API server..." << std::endl;
        api.run(); 
    });

    // 5. Основной цикл приложения
    while (true) {
        // 6. Проверка подключения
        if (mqtt.isConnected()) {
            
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    api_thread.join();
    mqtt.stopLoop();

    return 0;
}