#pragma once

#include "../../SIMPLE_LOGGER/liblogger/Logger.h"
#include "httplib.h"
#include "DataBase.hpp"
#include "crow_all.h"
#include <algorithm>
#include <random>

class API{

private:

    crow::SimpleApp app;
    DataBase &db;
    std::shared_ptr<liblog::Logger> logger;
    std::string serverToken;
    std::string baseRemoteApiUrl;
    std::string autodetectEndpoint;
    bool debugFlag;
    
    crow::response getTelemetry(long long module_id, const std::string &param_name, int time_interval);
    crow::response anomalyTagging(std::vector<long long> record_ids);
    crow::response getModuleParams(long long module_id, int time_interval, bool with_anomaly);
    crow::response getUniqueModuleIDs();
    crow::response autodetection(crow::json::rvalue input);
    
    void setupRoutes();

public:
    explicit API(DataBase &_db, std::shared_ptr<liblog::Logger> _logger, const std::string &token, 
                 const std::string &baseURL, const std::string &endpoint, bool _debugFlag);
    void run(int _port = 8080, bool multithreaded = true);
};

