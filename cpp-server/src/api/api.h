#pragma once

#include "../../include/crow_all.h"
#include "../database/DataBase.h"

class API{

private:

    crow::SimpleApp app;
    DataBase db;

    //std::string handleRoot();

    crow::response getDevices();
    crow::response getActuatorsDevices();
    crow::response auth(const std::string &username, const std::string &password);
    crow::response registration(const std::string &username, const std::string &password, long int tg_chat_id);
    crow::response singleAction(const std::string &device_name, const std::string &action);

    void setupRoutes();

    
public:
    explicit API(DataBase &db);
    void run(int _port = 8080, bool multithreaded = true);
};