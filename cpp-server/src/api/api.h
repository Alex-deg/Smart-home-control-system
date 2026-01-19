#pragma once

#include "../../include/crow_all.h"
#include "../database/DataBase.h"

class API{

private:
    crow::SimpleApp app;
    std::string handleRoot();
    crow::response getDevices();
    crow::response auth(const std::string &username, const std::string &password);
    crow::response registration(const std::string &username, const std::string &password, long int tg_chat_id);
    std::string handleStatus();
    void setupRoutes();
    DataBase db;
public:
    explicit API(DataBase &db);
    void run(int _port = 8080, bool multithreaded = true);
};