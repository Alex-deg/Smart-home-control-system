#pragma once

#include "../../include/crow_all.h"
#include "../database/DataBase.h"
#include "../mqtt/MQTTClient.h"

class API{

private:

    crow::SimpleApp app;
    DataBase &db;
    MQTTClient &mqtt;
    //std::string handleRoot();

    crow::response getDevices();
    crow::response getActuatorsDevices();
    crow::response auth(const std::string &username, const std::string &password);
    crow::response registration(const std::string &username, const std::string &password, long int tg_chat_id);
    crow::response singleAction(long long int device_id, const std::string &action);

    void setupRoutes();

public:
    explicit API(DataBase &db_, MQTTClient &mqtt_);
    void run(int _port = 8080, bool multithreaded = true);
};