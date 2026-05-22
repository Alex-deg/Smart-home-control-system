#pragma once
#include <mosquitto.h>
#include <string>
#include <functional>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <queue>
#include <thread>
#include "../database/DataBase.h"
#include <condition_variable>
#include "../scenario-handler/ScenarioEngine.hpp"
#include "../../include/httplib.h"

const std::string REMOTE_SERVER_IP = "127.0.0.1";
const int REMOTE_SERVER_API_PORT = 8080;
const std::string BASE_API_URL = REMOTE_SERVER_IP + ":" + std::to_string(REMOTE_SERVER_API_PORT);

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

    explicit MQTTClient(DataBase& db, ScenarioEngine &se);
    ~MQTTClient();

    // Основные методы
    bool connect(const std::string& host = "localhost", 
                 int port = 1883, 
                 int keepalive = 60);
    void disconnect();
    bool isConnected() const;
    
    // Публикация
    void publish(const std::string& topic, 
                 const std::string& message, 
                 int qos = 0, 
                 bool retain = false);
    
    // Подписка
    void subscribe(const std::string& topic, int qos = 1);
    void unsubscribe(const std::string& topic);
    
    // Callback'и
    void setOnConnectCallback(std::function<void()> callback);
    void setOnMessageCallback(MessageCallback callback);
    
    // Обработка сообщений в отдельном потоке
    void startLoop();
    void stopLoop();
    
    // Для интеграции с системой
    void processIncomingMessage(const std::string& topic, 
                                const std::string& payload);
    void sendDeviceCommand(int device_id, const std::string& command);

    void setSendCallback(SendCallback cb) {
        websocket_send = cb;
    }
    
private:
    // Callback'и Mosquitto
    static void on_connect(struct mosquitto* mosq, void* obj, int rc);
    static void on_disconnect(struct mosquitto* mosq, void* obj, int rc);
    static void on_publish(struct mosquitto* mosq, void* obj, int mid);
    static void on_message(struct mosquitto* mosq, void* obj, 
                           const struct mosquitto_message* msg);
    static void on_subscribe(struct mosquitto* mosq, void* obj, 
                             int mid, int qos_count, const int* granted_qos);
    static void on_unsubscribe(struct mosquitto* mosq, void* obj, int mid);
    
    // Внутренние методы
    void handleMessage(const struct mosquitto_message* msg);
    void reconnect();
    void saveMessageToDB(const std::string& topic, 
                         const std::string& payload, 
                         bool incoming);
    
    Topics convertStringTopicToEnum(const std::string& topic);

    // Поток для обработки loop
    void loopThread();
    
private:
    struct mosquitto* mosq_;
    DataBase& db_;
    ScenarioEngine& scenarioHandler;

    std::string client_id_;
    std::string host_;
    int port_;
    
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    
    std::function<void()> on_connect_callback_;
    MessageCallback on_message_callback_;
    
    std::mutex publish_mutex_;
    std::queue<std::tuple<std::string, std::string, int, bool>> publish_queue_;
    
    std::thread loop_thread_;
    std::mutex loop_mutex_;
    std::condition_variable loop_cv_;   
    SendCallback websocket_send;
};