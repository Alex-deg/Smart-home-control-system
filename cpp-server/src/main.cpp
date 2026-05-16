#include <iostream>
#include "database/DataBase.h"
#include "api/api.h"
#include <nlohmann/json.hpp>
#include "mqtt/MQTTClient.h"
#include "ws/ws.cpp"

using json = nlohmann::json;

void ws_server_thr(std::shared_ptr<MQTTClient> mqtt_client){
    std::string server_token;
    std::cout << "Введите токен сервера: ";
    std::getline(std::cin, server_token);
    if (server_token.empty()) {
        std::cout << "Токен пуст!" << std::endl;
        return;
    }
    else{
        std::cout << "TOKEN = " << server_token << std::endl;
    }
    std::string base_server_url = "ws://127.0.0.1:8000/ws/bind_server/";
    auto client = std::make_shared<RPiWebSocketClient>(server_token, base_server_url);
    mqtt_client->setSendCallback([client](const std::string& message){
        client->send_message(message);
    });
    client->set_on_command([mqtt_client](const std::string &topic, const std::string &payload, int qos){
        mqtt_client->publish(topic, payload, qos);
    });
    client->connect();
}

int main(){

    DataBase db;
    db.open("../Data/smart_home.db");

    std::shared_ptr<MQTTClient> mqtt = std::make_shared<MQTTClient>(db);

    if (!mqtt->connect("127.0.0.1", 1883)) {
        return 1;
    }
    
    mqtt->startLoop();

    std::thread wsThread(ws_server_thr, mqtt);

    API api(db);
    std::thread api_thread([&api]() {
        std::cout << "Starting HTTP API server..." << std::endl;
        api.run(); 
    });

    while (true) {
        if (!mqtt->isConnected()) {
            std::cout << "Mqtt connection is lost..." << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    if(wsThread.joinable())
        wsThread.join();
    if (api_thread.joinable())
        api_thread.join();
    
    mqtt->stopLoop();

    return 0;
}