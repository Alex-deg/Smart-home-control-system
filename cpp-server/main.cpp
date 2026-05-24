#include "../SIMPLE_LOGGER/liblogger/Logger.h"
#include "include/ScenarioHandler.hpp"
#include "include/JSONHandler.hpp"
#include "include/DataBase.hpp"
#include "include/MQTTClient.hpp"
#include "include/api.hpp"
#include "include/ws.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <memory>

using json = nlohmann::json;
using namespace liblog;

namespace Settings{

#ifdef NDEBUG
    bool DEBUG = false;
#else
    bool DEBUG = true;
#endif

    std::string LOG_PATH;

    std::string PATH_TO_DATABASE;
    int API_PORT;

    std::string REMOTE_SERVER_IP;
    int REMOTE_SERVER_API_PORT;
    std::string REMOTE_SERVER_BASE_API_URL;
    std::string WS_CONNECTION_BIND_ENDPOINT;
    std::string GET_ACT_INFO_ENDPOINT;

    std::string MQTT_BROKER_IP;
    int MQTT_BROKER_PORT;
}

void ws_server_thr(std::shared_ptr<MQTTClient> mqtt_client, DataBase &db, ScenarioHandler &sh){
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
    auto client = std::make_shared<WebSocketClient>(server_token, Settings::REMOTE_SERVER_BASE_API_URL + Settings::WS_CONNECTION_BIND_ENDPOINT, 
                                                    db, sh, Settings::DEBUG);
    mqtt_client->setSendCallback([client](const std::string& message){
        client->send_message(message);
    });
    client->set_on_command([mqtt_client](const std::string &topic, const std::string &payload, int qos){
        mqtt_client->publish(topic, payload, qos);
    });
    client->connect();
}

void getSettingsValuesFromJson(const std::string& path){
    JSONHandler jh;
    if(jh.open(path)){
        Settings::PATH_TO_DATABASE = jh.getValueByKey<std::string>("CPP_CORE_SETTINGS.PATH_TO_DATABASE");
        Settings::API_PORT = jh.getValueByKey<int>("CPP_CORE_SETTINGS.API_PORT");
        Settings::REMOTE_SERVER_IP = jh.getValueByKey<std::string>("REMOTE_SERVER_SETTINGS.REMOTE_SERVER_IP");
        Settings::REMOTE_SERVER_API_PORT = jh.getValueByKey<int>("REMOTE_SERVER_SETTINGS.REMOTE_SERVER_API_PORT");
        Settings::REMOTE_SERVER_BASE_API_URL = Settings::REMOTE_SERVER_IP + ":" + std::to_string(Settings::REMOTE_SERVER_API_PORT);
        Settings::WS_CONNECTION_BIND_ENDPOINT = jh.getValueByKey<std::string>("REMOTE_SERVER_SETTINGS.ENDPOINTS.WS_CONNECTION_BIND_ENDPOINT");
        Settings::GET_ACT_INFO_ENDPOINT = jh.getValueByKey<std::string>("REMOTE_SERVER_SETTINGS.ENDPOINTS.GET_ACT_INFO_ENDPOINT");
        Settings::MQTT_BROKER_IP = jh.getValueByKey<std::string>("MQTT_SETTINGS.MQTT_BROKER_IP");
        Settings::MQTT_BROKER_PORT = jh.getValueByKey<int>("MQTT_SETTINGS.MQTT_BROKER_PORT");
        std::cout << Settings::PATH_TO_DATABASE << std::endl;
    }
    else{
        std::cout << "Config file don't open" << std::endl;
    }
}

int main(){

    getSettingsValuesFromJson("../configs/config.json");

    DataBase db;
    db.open(Settings::PATH_TO_DATABASE);

    // logger for console
    std::shared_ptr<Logger> logger = std::make_shared<Logger>(liblog::INFO);

    ScenarioHandler sh;

    std::shared_ptr<MQTTClient> mqtt = std::make_shared<MQTTClient>(db, sh, Settings::REMOTE_SERVER_BASE_API_URL, 
                                                                            Settings::GET_ACT_INFO_ENDPOINT,
                                                                            logger,
                                                                            Settings::DEBUG);

    std::cout << Settings::MQTT_BROKER_IP << " " << Settings::MQTT_BROKER_PORT << std::endl;
    if (!mqtt->connect(Settings::MQTT_BROKER_IP, Settings::MQTT_BROKER_PORT)) {
        return 1;
    }
    
    mqtt->startLoop();

    int wait_count = 0;
    while (!mqtt->isConnected() && wait_count < 30) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        wait_count++;
        std::cout << "Waiting for MQTT connection... " << wait_count << "s" << std::endl;
    }
    
    if (!mqtt->isConnected()) {
        std::cerr << "MQTT connection timeout!" << std::endl;
        return 1;
    }

    std::thread wsThread(ws_server_thr, mqtt, std::ref(db), std::ref(sh));

    API api(db, Settings::DEBUG);
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