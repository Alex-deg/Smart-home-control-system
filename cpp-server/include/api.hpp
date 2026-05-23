#pragma once

#include "DataBase.hpp"
#include "crow_all.h"
#include <random>
#include <algorithm>

class API{

private:

    crow::SimpleApp app;
    DataBase &db;
    
    crow::response getTelemetry(long long module_id, const std::string &param_name, int time_interval);
    crow::response anomalyTagging(std::vector<long long> record_ids);
    crow::response getModuleParams(long long module_id, int time_interval, bool with_anomaly);
    crow::response getUniqueModuleIDs();
    
    void setupRoutes();

public:
    explicit API(DataBase &db_);
    void run(int _port = 8080, bool multithreaded = true);
};

