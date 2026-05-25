#include "../include/MQTTClient.hpp"

// Генерация уникального ID клиента
std::string generateClientId() {
    std::stringstream ss;
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    ss << "smart_home_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    return ss.str();
}

MQTTClient::MQTTClient(DataBase& db, ScenarioHandler &sh, const std::string& _base_api_url, 
                       const std::string& _get_act_info_endpoint, std::shared_ptr<Logger> _logger,
                       bool _debugFlag) 
    : db_(db), scenarioHandler(sh), mosq_(nullptr), connected_(false), running_(false),
      base_api_url(_base_api_url), get_act_info_endpoint(_get_act_info_endpoint), logger(_logger), 
      debugFlag(_debugFlag) {
    
    client_id_ = generateClientId();
    logger->debug("MQTTCLient::MQTTClient(): ClientID has been generated");
    HTTPClient = std::make_unique<httplib::Client>(base_api_url);
    logger->debug("MQTTCLient::MQTTClient(): HTTPClient for API requests has been created");

    static bool lib_initialized = false;
    if (!lib_initialized) {
        mosquitto_lib_init();
        lib_initialized = true;
    }
    logger->debug("MQTTCLient::MQTTClient(): Mosquitto lib has been initialized");
    
    // Создание экземпляра клиента
    mosq_ = mosquitto_new(client_id_.c_str(), true, this);
    if (!mosq_) {
        throw std::runtime_error("MQTTCLient::MQTTClient(): Failed to create Mosquitto instance");
    }
    logger->debug("MQTTCLient::MQTTClient(): Mosquitto client has been created");
    
    // Настройка callback'ов
    mosquitto_connect_callback_set(mosq_, on_connect);
    mosquitto_disconnect_callback_set(mosq_, on_disconnect);
    mosquitto_publish_callback_set(mosq_, on_publish);
    mosquitto_message_callback_set(mosq_, on_message);
    mosquitto_subscribe_callback_set(mosq_, on_subscribe);
    mosquitto_unsubscribe_callback_set(mosq_, on_unsubscribe);
    logger->debug("MQTTCLient::MQTTClient(): Callback setup has been completed successful");

    // Настройка логирования
    mosquitto_log_callback_set(mosq_, [](mosquitto* mosq, void* obj, int level, const char* str) {
        std::cout << "[MQTT LOG] Level " << level << ": " << str << std::endl;
    });
}

MQTTClient::~MQTTClient() {
    stopLoop();
    logger->debug("MQTTCLient::~MQTTClient(): MQTT main loop has been stopped");
    disconnect();
    logger->debug("MQTTCLient::~MQTTClient(): Mosquitto client has been disconnected");
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
    
    logger->info("MQTTCLient::connect(): Connecting to MQTT broker at " + host + ":" + std::to_string(port));
    
    int rc = mosquitto_connect(mosq_, host_.c_str(), port_, keepalive);
    if (rc != MOSQ_ERR_SUCCESS) {
        logger->error("MQTTCLient::connect(): Failed to initiate connection: " + std::string(mosquitto_strerror(rc)));
        return false;
    }
        
    return true;
}

void MQTTClient::disconnect() {
    if (mosq_ && connected_) {      
        mosquitto_disconnect(mosq_);
        connected_ = false;
        logger->debug("MQTTCLient::disconnect(): Mosquitto client has been disconnected");
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
        logger->warning("MQTTCLient::publish(): Not connected to MQTT broker. Queueing message.");
        std::lock_guard<std::mutex> lock(publish_mutex_);
        publish_queue_.push({topic, message, qos, retain});
        return;
    }
    
    int rc = mosquitto_publish(mosq_, nullptr, topic.c_str(), 
                               message.size(), message.c_str(), qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        logger->warning("MQTTCLient::publish(): Failed to publish to " + topic + ": " + std::string(mosquitto_strerror(rc)));
        // Сохраняем в очередь для повторной отправки
        std::lock_guard<std::mutex> lock(publish_mutex_);
        publish_queue_.push({topic, message, qos, retain});
        logger->debug("MQTTCLient::publish(): Message has been stored in queue");
    } else {
        logger->info("MQTTCLient::publish(): Message " + message + "has been published to the " + topic + "!");
    }
}

void MQTTClient::subscribe(const std::string& topic, int qos) {

    if (!connected_) {
        logger->error("MQTTCLient::subscribe(): Cannot subscribe: not connected");
        return;
    }
    
    int rc = mosquitto_subscribe(mosq_, nullptr, topic.c_str(), qos);
    if (rc != MOSQ_ERR_SUCCESS) {
        logger->error("MQTTCLient::subscribe(): Failed to subscribe to " + topic + ": " + std::string(mosquitto_strerror(rc)));
    } else {
        logger->info("MQTTCLient::subscribe(): Subscribed to topic: " + topic + ". QoS = " + std::to_string(qos));
    }
}

void MQTTClient::unsubscribe(const std::string& topic) {
    if (!connected_) return;
    
    int rc = mosquitto_unsubscribe(mosq_, nullptr, topic.c_str());
    if (rc != MOSQ_ERR_SUCCESS) {
        logger->error("MQTTCLient::unsubscribe(): Failed to unsubscribe from " + topic + ": " + std::string(mosquitto_strerror(rc)));
    }
    else logger->info("MQTTCLient::unsubscribe(): Unsubscribed from topic: " + topic);
}

void MQTTClient::on_connect(struct mosquitto* mosq, void* obj, int rc) {
    
    MQTTClient* client = static_cast<MQTTClient*>(obj);
    
    if (rc == 0) {
        client->connected_ = true;
        // Подписываемся на системные топики
        client->subscribe("rpi/database/save/telemetry", 1);   // Для сохранения данных с датчиков в БД
        client->subscribe("rpi/database/save/params", 1);      // Для сохранения диагностических в БД
        client->subscribe("rpi/database/get/telemetry", 1);    // Для получения данных с датчиков из БД
        client->subscribe("rpi/send_message/remote", 1);       // Для отправки ответа серверу на команду
        
        // Отправляем сообщения из очереди
        std::lock_guard<std::mutex> lock(client->publish_mutex_);
        while (!client->publish_queue_.empty()) {
            auto [topic, message, qos, retain] = client->publish_queue_.front();
            client->publish_queue_.pop();
            
            int result = mosquitto_publish(mosq, nullptr, topic.c_str(), 
                                          message.size(), message.c_str(), qos, retain);
            if (result == MOSQ_ERR_SUCCESS) {
                std::cout << "All messages from queue have been successfully published!";
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
}

void MQTTClient::on_unsubscribe(struct mosquitto* mosq, void* obj, int mid) {
}



void MQTTClient::handleMessage(const struct mosquitto_message* msg) {

    if (!msg || !msg->payload) return;
    
    std::string topic(msg->topic);
    std::string payload(static_cast<const char*>(msg->payload), msg->payloadlen);
    
    logger->info("MQTTCLient::handleMessage(): Received message on topic: " + topic + "\n" + 
                 "Payload: " + payload);
        
    // Обрабатываем сообщение
    processIncomingMessage(topic, payload);
}

void MQTTClient::reconnect() {

    logger->info("MQTTCLient::reconnect(): Attempting to reconnect...");
    
    disconnect();
    logger->debug("MQTTCLient::reconnect(): Mosquitto client has been disconnected");
    
    // Ждем перед повторной попыткой
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    int rc = mosquitto_reconnect(mosq_);
    if (rc != MOSQ_ERR_SUCCESS) {
        logger->info("MQTTCLient::reconnect(): Reconnection failed: " + std::string(mosquitto_strerror(rc)));
        // Можно реализовать экспоненциальную backoff стратегию
    }
    else logger->info("MQTTCLient::reconnect(): Reconnect has been completed successfully");
}

MQTTClient::Topics MQTTClient::convertStringTopicToEnum(const std::string &topic) {
    if (topic == "rpi/database/save/telemetry")
        return Topics::DB_SAVE_TELEMETRY;
    if (topic == "rpi/database/save/params")
        return Topics::DB_SAVE_PARAMS;    
    if (topic == "rpi/send_message/remote")
        return Topics::REMOTE_SEND;
}

void MQTTClient::startLoop() {
    if (running_) return;
    
    running_ = true;
    loop_thread_ = std::thread(&MQTTClient::loopThread, this);
    logger->debug("MQTTCLient::startLoop(): Main loop has been started");
}

void MQTTClient::stopLoop() {
    running_ = false;
    loop_cv_.notify_all();
    
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
    logger->debug("MQTTCLient::stopLoop(): Main loop has been stopped");
}

void MQTTClient::loopThread() {
    while (running_) {
        // Обработка сетевых событий
        int rc = mosquitto_loop(mosq_, 100, 1); // timeout 100ms
        if (rc != MOSQ_ERR_SUCCESS && rc != MOSQ_ERR_NO_CONN) {
            logger->error("MQTTCLient::loopThread(): Mosquitto loop error: " + std::string(mosquitto_strerror(rc)));
            if (rc == MOSQ_ERR_CONN_LOST && connected_) {
                connected_ = false;
                logger->warning("MQTTCLient::loopThread(): Connection lost, attempting to reconnect...");
                reconnect();
            }
        }
        // Небольшая пауза чтобы не грузить CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void MQTTClient::setSendCallback(SendCallback callback) {
    websocket_send = callback;
}



void MQTTClient::reEvaluationScenarios(const std::string& param_name, double param_value){

    std::vector<int> triggeredScenarios = scenarioHandler.updateParameter(param_name, param_value);
    logger->debug("MQTTCLient::reEvaluationScenarios(): List of triggered scenarios after param update has been received");
    for (auto &&tsID : triggeredScenarios){
        std::vector<long long> actIDs = db_.getScenariosActs(tsID);
        logger->debug("MQTTCLient::reEvaluationScenarios(): List of acts to be performed has been received");
        for (auto &&actID : actIDs){ 
            if (auto res = HTTPClient->Get(get_act_info_endpoint + "/" + std::to_string(actID))) {
                if (res->status == 200) {
                    std::string response = res->body;
                    json actInfo = json::parse(response);
                    json message;
                    message["request_id"] = -1; // stub, not good solution in general
                    message["payload"] = actInfo["command"];
                    publish(actInfo["mqtt_topic"], message.dump(), 1);
                } else {
                    logger->error("MQTTCLient::reEvaluationScenarios(): HTTP error: " + std::to_string(res->status));
                }
            } else {
                logger->error("MQTTCLient::reEvaluationScenarios(): Connection to server hasn't been established");
            }
        }
    }
    if (triggeredScenarios.size() > 0)
        logger->info("MQTTCLient::reEvaluationScenarios(): All triggered scenarios have been re evaluated");
    else logger->info("MQTTCLient::reEvaluationScenarios(): No one scenario has been triggered");
}

void MQTTClient::processIncomingMessage(const std::string& topic, 
                                        const std::string& payload) {

    json data = json::parse(payload);
    Topics t = convertStringTopicToEnum(topic);
    switch (t)
    {
    case Topics::DB_SAVE_TELEMETRY:
        try{
            db_.addTelemetry(data["module_id"], data["param_name"], data["param_value"], time(NULL), data["meas_unit"]);
            logger->info("MQTTCLient::processIncomingMessage(): Telemetry has been saved into database");
            reEvaluationScenarios(data["param_name"], data["param_value"]);
        }
        catch(std::runtime_error &err){
            logger->error("MQTTCLient::processIncomingMessage(): An error occurred while processing telemetry: " + std::string(err.what()));
        }
        break;
    case Topics::DB_SAVE_PARAMS:
        try{
            db_.addModuleParams(data["module_id"], data["module_temp"], data["free_bytes"], time(NULL));
            logger->info("MQTTCLient::processIncomingMessage(): Diagnostics data has been saved into database");
        }
        catch(std::runtime_error &err){
            logger->error("MQTTCLient::processIncomingMessage(): An error occurred while processing diagnostic data: " + std::string(err.what()));
        }
        break;
    case Topics::REMOTE_SEND:
        try{
            websocket_send(data.dump());
            logger->info("MQTTCLient::processIncomingMessage(): Message has been sent to the remote server");
        }
        catch(std::runtime_error &err){
            logger->error("MQTTCLient::processIncomingMessage(): An error occurred while sending message to the remote server: " + std::string(err.what()));
        }
        break;
    default:
        logger->warning("MQTTCLient::processIncomingMessage(): There is no such service topic yet.");
        break;
    }
 
}