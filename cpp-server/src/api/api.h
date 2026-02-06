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
                             const std::string& server_id);
    crow::response addModule(long long server_id, long long module_id);

    crow::response getServers(long long user_id);
    crow::response getModules(long long server_id);
    crow::response getAllModules();
    crow::response getDevices();
    crow::response getActuatorsDevices();
    crow::response getModuleCapabilities(long long record_id);
    crow::response getModuleNecessaryDevices(long long module_id);

    crow::response updateServerName(long long server_id, const std::string& new_server_name);

    crow::response deleteServer(long long server_id);
    crow::response deleteModule(long long record_id);

    crow::response auth(const std::string &username, const std::string &password);
    crow::response registration(const std::string &username, const std::string &password, long int tg_chat_id);
    crow::response singleAction(long long int device_id, const std::string &action);
    
    // crow::response securityModule();

    void setupRoutes();

public:
    explicit API(DataBase &db_, MQTTClient &mqtt_);
    void run(int _port = 8080, bool multithreaded = true);
};

