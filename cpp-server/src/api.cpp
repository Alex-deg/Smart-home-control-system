#include "../include/api.hpp"

API::API(DataBase &_db, std::shared_ptr<liblog::Logger> _logger, const std::string &token, 
         const std::string &baseURL, const std::string &endpoint, bool _debugFlag) : db(_db),
         logger(_logger), serverToken(token), baseRemoteApiUrl(baseURL), autodetectEndpoint(endpoint), debugFlag(_debugFlag) {}

std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    
    return ss.str();
}

void API::run(int _port, bool multithreaded) {
    setupRoutes();
    if (multithreaded) {
        app.port(_port).multithreaded().run();
        logger->info("API::API(): API has been launched in multithread mode on " + std::to_string(_port) + " port");
    } else {
        app.port(_port).run();
        if (debugFlag)
            logger->info("API::API(): API has been launched in basic mode on " + std::to_string(_port) + " port");
    }
}

void API::setMQTTClient(std::shared_ptr<MQTTClient> client) {
    mqtt_client_ = client;
    initMQTT(); 
}

void API::initMQTT() {
    mqtt_client_->setOnCommandResponse([this](const std::string& request_id, const nlohmann::json& payload) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        
        auto it = pending_requests_.find(request_id);
        if (it != pending_requests_.end()) {
            it->second.promise.set_value(payload);
            pending_requests_.erase(it);
        } else {
            logger->warning("API::onCommandResponse(): Unknown request_id: " + request_id);
        }
    });
}

crow::response API::getTelemetry(long long module_id, const std::string &param_name, int time_interval)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        auto telemetry = db.getTelemtry(module_id, param_name, time_interval);
        res.write(json(telemetry).dump(2));
        logger->info("API::getTelemetry(): Receiving telemetry was successful");
        return res;
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Получение телеметрии прошло с ошибкой";
        logger->error("API::getTelemetry(): Receiving telemetry occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));
    return res;
}

crow::response API::anomalyTagging(std::vector<long long> record_ids)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.anomalyTagging(record_ids);
        resp["status"] = true;
        resp["message"] = "Отметка аномальных данных прошла успешно";
        logger->info("API::anomalyTagging(): Anomaly tagging was successful");
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Отметка аномальных данных прошла с ошибкой";
        logger->error("API::anomalyTagging(): Anomaly tagging occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::getModuleParams(long long module_id, int time_interval, bool with_anomaly)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        auto params = db.getModuleParams(module_id, time_interval, with_anomaly);
        res.write(json(params).dump(2));
        logger->info("API::getModuleParams(): Receiving diagnostic data was successful");
        return res;
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Получение данных прошло с ошибкой";
        logger->error("API::getModuleParams(): Receiving diagnostic data occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::getUniqueModuleIDs()
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        auto params = db.getUniqueModuleIDs();
        res.write(json(params).dump(2));
        logger->info("API::getUniqueModuleIDs(): Receiving unique module ids was successful");
        return res;
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Получение данных прошло с ошибкой";
        logger->error("API::getUniqueModuleIDs(): Receiving unique module ids occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::autodetection(crow::json::rvalue input)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        json data;
        data["token"] = serverToken;
        data["name"] = input["name"];
        data["mqtt_topic"] = input["mqtt_topic"];
        data["alias"] = input["alias"];
        data["description"] = input["description"];

        httplib::Client client(baseRemoteApiUrl);
        httplib::Headers headers = {
            {"Content-Type", "application/json"}
        };
        if (auto result = client.Post(autodetectEndpoint, headers, data.dump(), "application/json")) {
            if (result->status == 200) {
                auto json = crow::json::load(result->body);
                data.clear();
                data["module_id"] = json["module_id"];
                res.write(data.dump());
                return res;
            } else {
                logger->error("API::autodetection(): HTTP error: " + std::to_string(result->status));
            }
        } else {
            logger->error("API::autodetection(): Connection to server hasn't been established");
        }        
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Получение данных прошло с ошибкой";
        logger->error("API::autodetection(): Autodetect occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::getModules()
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        auto params = db.getModules();
        res.write(json(params).dump(2));
        logger->info("API::getModules(): Receiving modules was successful");
        return res;
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Получение данных прошло с ошибкой";
        logger->error("API::getModules(): Receiving modules occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::addModule(const std::string &name, const std::string &alias, 
                              const std::string &mqtt_topic, const std::string &description)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        auto params = db.addModule(name, alias, mqtt_topic, description);
        resp["status"] = true;
        resp["message"] = "Сохранение данных прошло успешно";
        logger->info("API::addModule(): Adding module was successful");
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Сохранение данных прошло с ошибкой";
        logger->error("API::addModule(): Adding module occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::deleteModule(long long module_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.deleteModuleFromTables(module_id);
        resp["status"] = true;
        resp["message"] = "Удаление модуля прошло успешно";
        logger->info("API::deleteModule(): Deleting module was successful");
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Удаление модуля прошло с ошибкой";
        logger->error("API::deleteModule(): Deleting module occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::getCapabilities(long long module_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        auto params = db.getCapabilities(module_id);
        res.write(json(params).dump(2));
        logger->info("API::getCapabilities(): Receiving capabilities was successful");
        return res;
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Получение функций прошло с ошибкой";
        logger->error("API::getCapabilities(): Receiving capabilities occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::addCapability(long long module_id, const std::string &name)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.addCapability(module_id, name);
        resp["status"] = true;
        resp["message"] = "Сохранение данных прошло успешно";
        logger->info("API::addCapability(): Adding capability was successful");
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Сохранение данных прошло с ошибкой";
        logger->error("API::addCapability(): Adding capability occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::deleteCapability(long long capability_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.deleteCapabilityFromTables(capability_id);
        resp["status"] = true;
        resp["message"] = "Удаление функции прошло успешно";
        logger->info("API::deleteCapability(): Deleting capability was successful");
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Удаление функции прошло с ошибкой";
        logger->error("API::deleteCapability(): Deleting capability occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::unbindCapability(long long module_id, long long capability_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.unbindModuleCapability(module_id, capability_id);
        resp["status"] = true;
        resp["message"] = "Отвязка функции прошло успешно";
        logger->info("API::unbindCapability(): Capability unbind was successful");
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Отвязка функции прошло с ошибкой";
        logger->error("API::unbindCapability(): Capability unbinding occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::sendCommand(long long module_id, long long capability_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try {

        auto module_info = db.getModuleInfo(module_id);
        auto capability_info = db.getCapabilityInfo(capability_id);
        
        std::string request_id = generateUUID();
        
        json message;
        message["internet"] = false;
        message["request_id"] = request_id;
        message["params"]["mqtt_topic"] = module_info["mqtt_topic"];
        message["params"]["payload"] = capability_info["name"];
        
        std::promise<json> promise;
        auto future = promise.get_future();
        
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_[request_id] = {std::move(promise), std::chrono::steady_clock::now()};
        }
        
        mqtt_client_->publish(module_info["mqtt_topic"], json{{"command", capability_info["name"]}, 
                                                              {"request_id", request_id}}.dump(), 1);
        
        auto status = future.wait_for(std::chrono::seconds(10));
        
        if (status == std::future_status::timeout) {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_.erase(request_id);
            json resp;
            resp["status"] = false;
            resp["message"] = "Command timeout (no response from module)";
            res.code = 408;
            res.write(resp.dump(2));
            return res;
        }
        
        auto response_data = future.get();
        
        resp["status"] = "ok";
        resp["message"] = "Отправка команды прошла успешно";
        resp["result"] = response_data;
        res.write(resp.dump(2));
        res.code = 200;
        logger->info("API::sendCommand(): Command sent successfully, response received");        
    } catch (const std::exception& e) {
        resp["status"] = "fault";
        resp["message"] = "Отправка команды прошла с ошибкой";
        res.write(resp.dump(2));
    }
    return res;
}

crow::response API::addScenario(const std::string &name, const std::string &condition)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.addScenario(name, condition);
        resp["status"] = true;
        resp["message"] = "Сохранение сценария прошло успешно";
        logger->info("API::addScenario(): Adding scenario was successful");
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Сохранение сценария прошло с ошибкой";
        logger->error("API::addScenario(): Adding scenario occured with error: " + std::string(e.what()));
    }            
    res.write(json(resp).dump(2));           
    return res;
}

void API::set_on_command(CommandCallback cb){
    mqttPublish = cb;
}

void API::setupRoutes()
{

    CROW_ROUTE(app, "/")
    ([this](){
        std::ifstream file("../web/index.html");  
        if (!file.is_open()) {
            return crow::response(404, "File not found");
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        crow::response res;
        res.set_header("Content-Type", "text/html; charset=utf-8");
        res.write(buffer.str());
        return res;
    });

    CROW_ROUTE(app, "/api/database/telemetry")
    .methods(crow::HTTPMethod::Get)
    ([this](const crow::request& req){

        auto params = req.url_params;
        
        if (!params.get("module_id") || !params.get("param_name") || !params.get("time_interval")) {
            crow::response res(400);
            res.write(R"({"error": "Missing required parameters: module_id, param_name, time_interval"})");
            res.set_header("Content-Type", "application/json");
            return res;
        }
        
        long long module_id = std::stoll(params.get("module_id"));
        std::string param_name = params.get("param_name");
        long long time_interval = std::stoll(params.get("time_interval"));
        
        return getTelemetry(module_id, param_name, time_interval);
    });

    CROW_ROUTE(app, "/api/database/params")
    .methods(crow::HTTPMethod::Get)
    ([this](const crow::request& req){

        auto params = req.url_params;
        
        if (!params.get("module_id") || !params.get("time_interval") || !params.get("anomaly")) {
            crow::response res(400);
            res.write(R"({"error": "Missing required parameters: module_id, time_interval"})");
            res.set_header("Content-Type", "application/json");
            return res;
        }
        
        long long module_id = std::stoll(params.get("module_id"));
        long long time_interval = std::stoll(params.get("time_interval"));
        bool anomaly = static_cast<bool>(params.get("anomaly"));
        
        return getModuleParams(module_id, time_interval, anomaly);
    });

    CROW_ROUTE(app, "/api/database/params/unique_modules")
    .methods(crow::HTTPMethod::Get)
    ([this](const crow::request& req){
        return getUniqueModuleIDs();
    });

    CROW_ROUTE(app, "/api/database/params/anomaly_tagging")
    .methods(crow::HTTPMethod::Post)
    ([this](const crow::request& req){

        auto json = crow::json::load(req.body);

        if (!json || !json.has("record_ids")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        std::vector<long long> record_ids;
        
        for (const auto& item : json["record_ids"]) {
            record_ids.push_back(item.i());
        }

        return anomalyTagging(record_ids);
    });

    CROW_ROUTE(app, "/api/auto-detect")
    .methods(crow::HTTPMethod::Post)
    ([this](const crow::request& req){
        
        auto json = crow::json::load(req.body);

        if (!json || !json.has("name") || !json.has("mqtt_topic") || !json.has("alias") || !json.has("description")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        return autodetection(json);

    });

    CROW_ROUTE(app, "/api/modules")
    .methods(crow::HTTPMethod::Get)
    ([this]() {
        return getModules();
    });
    
    CROW_ROUTE(app, "/api/modules/add")
    .methods(crow::HTTPMethod::Post)
    ([this](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) {
            json resp;
            resp["status"] = false;
            resp["message"] = "Invalid JSON body";
            return crow::response(400, resp.dump(2));
        }
        std::string name = body["name"].s();
        std::string alias = body["alias"].s();
        std::string mqtt_topic = body["mqtt_topic"].s();
        std::string description = body["description"].s();
        return addModule(name, alias, mqtt_topic, description);
    });
    
    CROW_ROUTE(app, "/api/modules/<int>/delete")
    .methods(crow::HTTPMethod::Delete)
    ([this](int module_id) {
        return deleteModule(module_id);
    });
    
    CROW_ROUTE(app, "/api/modules/<int>/capabilities")
    .methods(crow::HTTPMethod::Get)
    ([this](int module_id) {
        return getCapabilities(module_id);
    });
    
    CROW_ROUTE(app, "/api/modules/capabilities/add")
    .methods(crow::HTTPMethod::Post)
    ([this](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) {
            json resp;
            resp["status"] = false;
            resp["message"] = "Invalid JSON body";
            return crow::response(400, resp.dump(2));
        }
        long long module_id = body["module_id"].i();
        std::string name = body["name"].s();
        return addCapability(module_id, name);
    });
    
    CROW_ROUTE(app, "/api/modules/<int>/capabilities/<int>/delete")
    .methods(crow::HTTPMethod::Delete)
    ([this](int module_id, int capability_id) {
        return deleteCapability(capability_id);
    });
    
    CROW_ROUTE(app, "/api/modules/<int>/capabilities/<int>/unbind")
    .methods(crow::HTTPMethod::Post)
    ([this](int module_id, int capability_id) {
        return unbindCapability(module_id, capability_id);
    });
    
    CROW_ROUTE(app, "/api/modules/<int>/capability/<int>/send_command")
    .methods(crow::HTTPMethod::Post)
    ([this](int module_id, int capability_id) {
        return sendCommand(module_id, capability_id);
    });
    
    CROW_ROUTE(app, "/api/add_scenario")
    .methods(crow::HTTPMethod::Post)
    ([this](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) {
            json resp;
            resp["status"] = false;
            resp["message"] = "Invalid JSON body";
            return crow::response(400, resp.dump(2));
        }
        std::string name = body["name"].s();
        std::string condition = body["condition"].s();
        return addScenario(name, condition);
    });

}