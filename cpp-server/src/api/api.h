#pragma once

#include "../../include/crow_all.h"
#include "../database/DataBase.h"

class API{

private:
    crow::SimpleApp app;
    std::string handleRoot();
    crow::response getDevices();
    bool checkUser();
    std::string handleStatus();
    void setupRoutes();
    DataBase db;
public:
    API(DataBase &db);
    void run(int _port = 8080, bool multithreaded = true);
};