#pragma once

#include <random>
#include <algorithm>

#include "../../include/crow_all.h"
#include "../database/DataBase.h"

class API{

private:

    crow::SimpleApp app;
    DataBase &db;
    
    crow::response getTelemetry(long long module_id, const std::string &param_name, int time_interval);
    // crow::response getParams();

    void setupRoutes();

public:
    explicit API(DataBase &db_);
    void run(int _port = 8080, bool multithreaded = true);
};

