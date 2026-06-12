#pragma once

#include "../../SIMPLE_LOGGER/liblogger/Logger.h"
#include "httplib.h"
#include "DataBase.hpp"
#include "MQTTClient.hpp"
#include "crow_all.h"
#include <algorithm>
#include <random>

class API{

public:
    using CommandCallback = std::function<void(const std::string& mqtt_topic,
                                               const std::string& message,
                                               int qos)>;

    explicit API(DataBase &_db, std::shared_ptr<liblog::Logger> _logger, const std::string &token, 
                 const std::string &baseURL, const std::string &endpoint, bool _debugFlag);
    void run(int _port = 8080, bool multithreaded = true);
    void setMQTTClient(std::shared_ptr<MQTTClient> client);
private:

    crow::SimpleApp app;
    DataBase &db;
    std::shared_ptr<liblog::Logger> logger;
    std::string serverToken;
    std::string baseRemoteApiUrl;
    std::string autodetectEndpoint;
    CommandCallback mqttPublish;
    bool debugFlag;
    struct PendingRequest {
        std::shared_ptr<std::promise<nlohmann::json>> promise;
        std::chrono::steady_clock::time_point timestamp;
        
        PendingRequest(std::shared_ptr<std::promise<nlohmann::json>> p,
                    std::chrono::steady_clock::time_point t)
            : promise(p), timestamp(t) {}
        
        PendingRequest(PendingRequest&&) = default;
        PendingRequest& operator=(PendingRequest&&) = default;
        
        PendingRequest(const PendingRequest&) = delete;
        PendingRequest& operator=(const PendingRequest&) = delete;
    };
    std::shared_ptr<MQTTClient> mqtt_client_;
    std::unordered_map<std::string, PendingRequest> pending_requests_;
    std::mutex pending_mutex_;
    
    void initMQTT();

    crow::response getTelemetry(long long module_id, const std::string &param_name, int time_interval);
    crow::response anomalyTagging(std::vector<long long> record_ids);
    crow::response getModuleParams(long long module_id, int time_interval, bool with_anomaly);
    crow::response getUniqueModuleIDs();
    crow::response autodetection(crow::json::rvalue input);
    
    crow::response getModules();
    crow::response addModule(const std::string &name, const std::string &alias, 
                             const std::string &mqtt_topic, const std::string &description);
    crow::response deleteModule(long long module_id);
    crow::response getCapabilities(long long module_id);
    crow::response addCapability(long long module_id, const std::string &name);
    crow::response deleteCapability(long long capability_id);
    crow::response unbindCapability(long long module_id, long long capability_id);
    crow::response sendCommand(long long module_id, long long capability_id);
    crow::response addScenario(const std::string &name, const std::string &condition);
    
    void set_on_command(CommandCallback cb);

    void setupRoutes();
};

