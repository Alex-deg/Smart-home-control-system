#pragma once

#include <random>
#include <algorithm>

#include "../../include/crow_all.h"
#include "../database/DataBase.h"
#include "../mqtt/MQTTClient.h"

class API{

private:

    crow::SimpleApp app;
    DataBase &db;
    MQTTClient &mqtt;
    //std::string handleRoot();

    crow::response addServer(long long user_id, const std::string& server_name, 
                             const std::string& server_key);
    crow::response addModule(long long server_id, long long module_type_id, 
                             const std::string& alias);

    crow::response addDevice(long long module_id, long long device_type_id, 
                             const std::string& mqtt_topic, const std::string& alias);
    crow::response getServers(long long user_id);
    crow::response getModules(long long server_id);
    crow::response getModulesTypes(long long user_id);
    crow::response getDevices();
    crow::response getActuatorsDevices();
    
    crow::response getModuleCapabilities(long long module_id);

    crow::response getModuleNecessaryDevices(long long module_type_id);

    crow::response updateServerName(long long server_id, const std::string& new_server_name);

    crow::response deleteServer(long long server_id);
    crow::response deleteModule(long long module_id);

    crow::response auth(const std::string &username, const std::string &password);
    crow::response registration(const std::string &username, const std::string &password, long int tg_chat_id);

    crow::response capabilityHandler(long long module_id, long long capability_id);
    crow::response actionHandler(json action_info, json device_info);
    crow::response singleAction(const std::string& mqtt_topic , const std::string &action);
    
    // crow::response generateMQTTTopic(long long record_id);
    std::string generateMQTTTopic();
    void setupRoutes();

public:
    explicit API(DataBase &db_, MQTTClient &mqtt_);
    void run(int _port = 8080, bool multithreaded = true);
};

