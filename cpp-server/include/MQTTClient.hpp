#pragma once

#include "ScenarioHandler.hpp"
#include "DataBase.hpp"
#include "httplib.h"
#include <condition_variable>
#include <mosquitto.h>
#include <functional>
#include <iostream>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <map>

class MQTTClient {
public:

    enum Topics{
        DB_SAVE_TELEMETRY = 0,
        DB_SAVE_PARAMS, 
        REMOTE_SEND,
        TOPICS_N
    };

    using MessageCallback = std::function<void(const std::string& topic, 
                                               const std::string& payload)>;
    
    using SendCallback = std::function<void(const std::string& message)>;

    explicit MQTTClient(DataBase& db, ScenarioHandler &sh, const std::string& _base_api_url, 
                        const std::string& _get_act_info_endpoint, bool _debugFlag);
    ~MQTTClient();

    bool connect(const std::string& host = "localhost", 
                 int port = 1883, 
                 int keepalive = 60);
    void disconnect();
    bool isConnected() const;
    
    void publish(const std::string& topic, 
                 const std::string& message, 
                 int qos = 0, 
                 bool retain = false);
    
    void subscribe(const std::string& topic, int qos = 1);
    void unsubscribe(const std::string& topic);
    
    void setSendCallback(SendCallback callback);

    void stopLoop();
    void startLoop();
    
    void reEvaluationScenarios(const std::string& param_name, double param_value);
    void processIncomingMessage(const std::string& topic, 
                                const std::string& payload);
    
private:

    static void on_connect(struct mosquitto* mosq, void* obj, int rc);
    static void on_disconnect(struct mosquitto* mosq, void* obj, int rc);
    static void on_publish(struct mosquitto* mosq, void* obj, int mid);
    static void on_message(struct mosquitto* mosq, void* obj, 
                           const struct mosquitto_message* msg);
    static void on_subscribe(struct mosquitto* mosq, void* obj, 
                             int mid, int qos_count, const int* granted_qos);
    static void on_unsubscribe(struct mosquitto* mosq, void* obj, int mid);
    
    void reconnect();
    void handleMessage(const struct mosquitto_message* msg);
    
    Topics convertStringTopicToEnum(const std::string& topic);

    void loopThread();
    
private:

    bool debugFlag;

    DataBase& db_;
    ScenarioHandler& scenarioHandler;

    int port_;
    std::string host_;
    std::string client_id_;
    struct mosquitto* mosq_;   
    
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    
    std::mutex publish_mutex_;
    std::queue<std::tuple<std::string, std::string, int, bool>> publish_queue_;
    
    std::mutex loop_mutex_;
    std::thread loop_thread_;
    std::condition_variable loop_cv_;   
    
    SendCallback websocket_send;

    std::unique_ptr<httplib::Client> HTTPClient = nullptr;
    std::string base_api_url;
    std::string get_act_info_endpoint;
};