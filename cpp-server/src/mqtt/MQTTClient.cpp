#include "MQTTClient.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>

// Генерация уникального ID клиента
std::string generateClientId() {
    std::stringstream ss;
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    ss << "smart_home_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    return ss.str();
}

MQTTClient::MQTTClient(DataBase& db) 
    : db_(db), mosq_(nullptr), connected_(false), running_(false) {
    
    client_id_ = generateClientId();
    
    // Инициализация библиотеки (один раз на всю программу)
    static bool lib_initialized = false;
    if (!lib_initialized) {
        mosquitto_lib_init();
        lib_initialized = true;
    }
    
    // Создание экземпляра клиента
    mosq_ = mosquitto_new(client_id_.c_str(), true, this);
    if (!mosq_) {
        throw std::runtime_error("Failed to create Mosquitto instance");
    }
    
    // Настройка callback'ов
    mosquitto_connect_callback_set(mosq_, on_connect);
    mosquitto_disconnect_callback_set(mosq_, on_disconnect);
    mosquitto_publish_callback_set(mosq_, on_publish);
    mosquitto_message_callback_set(mosq_, on_message);
    mosquitto_subscribe_callback_set(mosq_, on_subscribe);
    mosquitto_unsubscribe_callback_set(mosq_, on_unsubscribe);
    
    // Настройка логирования
    mosquitto_log_callback_set(mosq_, [](mosquitto* mosq, void* obj, int level, const char* str) {
        std::cout << "[MQTT LOG] Level " << level << ": " << str << std::endl;
    });
}

MQTTClient::~MQTTClient() {
    stopLoop();
    disconnect();
    
    if (mosq_) {
        mosquitto_destroy(mosq_);
    }
}

bool MQTTClient::connect(const std::string& host, int port, int keepalive) {
    
    if (connected_) {
        return true;
    }
    
    host_ = host;
    port_ = port;
    
    std::cout << "Connecting to MQTT broker at " << host << ":" << port << std::endl;
    
    // Установка Last Will & Testament (опционально)
    std::string lwt_topic = "system/status";
    std::string lwt_message = R"({"client_id":")" + client_id_ + R"(","status":"offline"})";
    mosquitto_will_set(mosq_, lwt_topic.c_str(), 
                       lwt_message.size(), lwt_message.c_str(), 1, true);
    
    // Подключение
    int rc = mosquitto_connect(mosq_, host_.c_str(), port_, keepalive);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to initiate connection: " << mosquitto_strerror(rc) << std::endl;
        return false;
    }
    
    // Запуск потока обработки
    // startLoop();
    
    return true;
}

void MQTTClient::disconnect() {
    if (mosq_ && connected_) {
        // Отправляем сообщение о отключении
        std::string topic = "system/status";
        std::string message = R"({"client_id":")" + client_id_ + R"(","status":"disconnecting"})";
        publish(topic, message, 1, true);
        
        mosquitto_disconnect(mosq_);
        connected_ = false;
    }
}

bool MQTTClient::isConnected() const {
    return connected_;
}

void MQTTClient::publish(const std::string& topic, 
                        const std::string& message, 
                        int qos, 
                        bool retain) {
    if (!connected_) {
        std::cerr << "Not connected to MQTT broker. Queueing message." << std::endl;
        std::lock_guard<std::mutex> lock(publish_mutex_);
        publish_queue_.push({topic, message, qos, retain});
        return;
    }
    
    int rc = mosquitto_publish(mosq_, nullptr, topic.c_str(), 
                               message.size(), message.c_str(), qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to publish to " << topic << ": " << mosquitto_strerror(rc) << std::endl;
        
        // Сохраняем в очередь для повторной отправки
        std::lock_guard<std::mutex> lock(publish_mutex_);
        publish_queue_.push({topic, message, qos, retain});
    } else {
        std::cout << "Message has been published" << std::endl;
        saveMessageToDB(topic, message, false); // false = исходящее
    }
}

void MQTTClient::subscribe(const std::string& topic, int qos) {
    if (!connected_) {
        std::cerr << "Cannot subscribe: not connected" << std::endl;
        return;
    }
    
    int rc = mosquitto_subscribe(mosq_, nullptr, topic.c_str(), qos);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to subscribe to " << topic << ": " << mosquitto_strerror(rc) << std::endl;
    } else {
        std::cout << "Subscribed to topic: " << topic << std::endl;
    }
}

void MQTTClient::unsubscribe(const std::string& topic) {
    if (!connected_) return;
    
    int rc = mosquitto_unsubscribe(mosq_, nullptr, topic.c_str());
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to unsubscribe from " << topic << ": " << mosquitto_strerror(rc) << std::endl;
    }
}

// ==================== CALLBACK ОБРАБОТЧИКИ ====================

void MQTTClient::on_connect(struct mosquitto* mosq, void* obj, int rc) {
    
    MQTTClient* client = static_cast<MQTTClient*>(obj);
    
    if (rc == 0) {
        std::cout << "Successfully connected to MQTT broker" << std::endl;
        client->connected_ = true;
                
        // Подписываемся на системные топики
        client->subscribe("rpi/database/save/telemetry", 1);     // Для сохранения данных с датчиков в БД (esp отдает данные серверу)
        client->subscribe("rpi/database/save/params", 1);     // Для сохранения данных с датчиков в БД (esp отдает данные серверу)
        client->subscribe("rpi/database/get/telemetry", 1);      // Для получения данных с БД (esp зпрашивает данные у сервера)
        client->subscribe("rpi/send_message/remote", 1);  // Для отправки ответа пользователю на его команду (esp отдает данные серверу)
                
        // Вызываем пользовательский callback
        if (client->on_connect_callback_) {
            client->on_connect_callback_();
        }
        
        // Отправляем сообщения из очереди
        std::lock_guard<std::mutex> lock(client->publish_mutex_);
        while (!client->publish_queue_.empty()) {
            auto [topic, message, qos, retain] = client->publish_queue_.front();
            client->publish_queue_.pop();
            
            int result = mosquitto_publish(mosq, nullptr, topic.c_str(), 
                                          message.size(), message.c_str(), qos, retain);
            if (result == MOSQ_ERR_SUCCESS) {
                client->saveMessageToDB(topic, message, false);
            }
        }
        
    } else {
        std::cerr << "Failed to connect to MQTT broker: " << mosquitto_connack_string(rc) << std::endl;
        client->connected_ = false;
        
        // Пытаемся переподключиться через 5 секунд
        std::this_thread::sleep_for(std::chrono::seconds(5));
        client->reconnect();
    }
}

void MQTTClient::on_disconnect(struct mosquitto* mosq, void* obj, int rc) {
    MQTTClient* client = static_cast<MQTTClient*>(obj);
    client->connected_ = false;
    
    std::cout << "Disconnected from MQTT broker" << std::endl;
    
    if (rc != 0) {
        // Неожиданное отключение, пытаемся переподключиться
        std::cout << "Unexpected disconnect, reconnecting..." << std::endl;
        client->reconnect();
    }
}

void MQTTClient::on_publish(struct mosquitto* mosq, void* obj, int mid) {
    // Сообщение успешно опубликовано
    // Можно добавить логирование или обновление статуса
}

void MQTTClient::on_message(struct mosquitto* mosq, void* obj, 
                           const struct mosquitto_message* msg) {
    MQTTClient* client = static_cast<MQTTClient*>(obj);
    client->handleMessage(msg);
}

void MQTTClient::on_subscribe(struct mosquitto* mosq, void* obj, 
                             int mid, int qos_count, const int* granted_qos) {
    std::cout << "Successfully subscribed to topic (QoS: ";
    for (int i = 0; i < qos_count; ++i) {
        std::cout << granted_qos[i];
        if (i < qos_count - 1) std::cout << ", ";
    }
    std::cout << ")" << std::endl;
}

void MQTTClient::on_unsubscribe(struct mosquitto* mosq, void* obj, int mid) {
    std::cout << "Successfully unsubscribed from topic" << std::endl;
}

// ==================== ВНУТРЕННИЕ МЕТОДЫ ====================

void MQTTClient::handleMessage(const struct mosquitto_message* msg) {
    if (!msg || !msg->payload) return;
    
    std::string topic(msg->topic);
    std::string payload(static_cast<const char*>(msg->payload), msg->payloadlen);
    
    std::cout << "Received message on topic: " << topic << std::endl;
    std::cout << "Payload: " << payload << std::endl;
    
    // Сохраняем в БД
    saveMessageToDB(topic, payload, true); // true = входящее
    
    // Обрабатываем сообщение
    processIncomingMessage(topic, payload);
    
    // Вызываем пользовательский callback
    if (on_message_callback_) {
        on_message_callback_(topic, payload);
    }
}

void MQTTClient::reconnect() {
    std::cout << "Attempting to reconnect..." << std::endl;
    
    disconnect();
    
    // Ждем перед повторной попыткой
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    int rc = mosquitto_reconnect(mosq_);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Reconnection failed: " << mosquitto_strerror(rc) << std::endl;
        // Можно реализовать экспоненциальную backoff стратегию
    }
}

void MQTTClient::saveMessageToDB(const std::string& topic, 
                                const std::string& payload, 
                                bool incoming) {
    // try {
    //     db_.addMQTTMessage(topic, payload, incoming);        
    // } catch (const std::exception& e) {
    //     std::cerr << "Failed to save MQTT message to DB: " << e.what() << std::endl;
    // }

}

MQTTClient::Topics MQTTClient::convertStringTopicToEnum(const std::string &topic)
{
    if (topic == "rpi/database/save/telemetry")
        return Topics::DB_SAVE_TELEMETRY;
    if (topic == "rpi/database/save/params")
        return Topics::DB_SAVE_PARAMS;    
    if (topic == "rpi/database/get/telemetry")
        return Topics::DB_GET_TELEMETRY;
    if (topic == "rpi/send_message/remote")
        return Topics::REMOTE_SEND;
}

void MQTTClient::startLoop() {
    if (running_) return;
    
    running_ = true;
    loop_thread_ = std::thread(&MQTTClient::loopThread, this);
}

void MQTTClient::stopLoop() {
    running_ = false;
    loop_cv_.notify_all();
    
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

void MQTTClient::loopThread() {
    while (running_) {
        // Обработка сетевых событий
        int rc = mosquitto_loop(mosq_, 100, 1); // timeout 100ms
        if (rc != MOSQ_ERR_SUCCESS && rc != MOSQ_ERR_NO_CONN) {
            std::cerr << "Mosquitto loop error: " << mosquitto_strerror(rc) << std::endl;
            if (rc == MOSQ_ERR_CONN_LOST && connected_) {
                connected_ = false;
                std::cout << "Connection lost, attempting to reconnect..." << std::endl;
                reconnect();
            }
        }
        // Небольшая пауза чтобы не грузить CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void MQTTClient::setOnConnectCallback(std::function<void()> callback) {
    on_connect_callback_ = callback;
}

void MQTTClient::setOnMessageCallback(MessageCallback callback) {
    on_message_callback_ = callback;
}

// ==================== ИНТЕГРАЦИЯ С СИСТЕМОЙ ====================

void MQTTClient::processIncomingMessage(const std::string& topic, 
                                       const std::string& payload) {

    json data = json::parse(payload);
    Topics t = convertStringTopicToEnum(topic);
    switch (t)
    {
    case Topics::DB_SAVE_TELEMETRY:
        try{
            db_.addTelemetry(data["module_id"], data["param_name"], data["param_value"], time(NULL), data["meas_unit"]);
        }
        catch(std::runtime_error &err){
            std::cerr << err.what() << std::endl;
        }
        break;
    case Topics::DB_SAVE_PARAMS:
        try{
            db_.addModuleParams(data["module_id"], data["input_amperege"], data["input_voltage"], data["module_temp"], time(NULL));
        }
        catch(std::runtime_error &err){
            std::cerr << err.what() << std::endl;
        }
        break;
    case Topics::DB_GET_TELEMETRY:
        try{
            std::vector<double> values = db_.getTelemtry(data["module_id"], data["param_name"], data["time_interval"]);
            std::stringstream ss;
            for (auto &&val : values){ ss << val << " "; }
            // auto module_info = db_.getModuleInfo(data["module_id"]);
            // publish(module_info["mqtt_topic"] + "/data", ss.str(), 1);
        }
        catch(std::runtime_error &err){
            std::cerr << err.what() << std::endl;
        }
        break;
    case Topics::REMOTE_SEND:
        try{
            json response;
            response["request_id"] = data["request_id"];
            response["payload"] = data["payload"];
            websocket_send(response.dump());
        }
        catch(std::runtime_error &err){
            std::cerr << err.what() << std::endl;
        }
        break;
    default:
        std::cout << "There is no such service topic yet." << std::endl;
        break;
    }
 
}