#include "api.h"

// Перепроверить конструктор
API::API(DataBase &db_, MQTTClient &mqtt_) : db(db_), mqtt(mqtt_){}

void API::run(int _port, bool multithreaded) {
    setupRoutes();
    if (multithreaded) {
        app.port(_port).multithreaded().run();
    } else {
        app.port(_port).run();
    }
}

// std::string API::handleRoot() {
//     return R"(
//         <html>
//         <head><title>SmartHome</title></head>
//         <body>
//             <h1>Smart home system control</h1>
//             <p>API server is working</p>
//             <ul>
//                 <li><a href="/api/status">System status</a></li>
//                 <li><a href="/api/devices">Devices</a></li>
//             </ul>
//         </body>
//         </html>
//         )";
// }

crow::response API::addServer(long long user_id, const std::string &server_name, const std::string &server_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.addServer(user_id, server_name, server_id);
        resp["status"] = true;
        resp["message"] = "Добавление сервера прошло успешно!";
    }
    catch(DataBaseException &e){
        std::cerr << "Error: " << e.what() << std::endl;
        resp["status"] = false;
        resp["message"] = "Добавление сервера прошло с ошибкой";
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::addModule(long long server_id, long long module_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.addModule(server_id, module_id);
        resp["status"] = true;
        resp["message"] = "Добавление модуля прошло успешно!";
    }
    catch(DataBaseException &e){
        std::cerr << "Error: " << e.what() << std::endl;
        resp["status"] = false;
        resp["message"] = "Добавление модуля прошло с ошибкой";
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::getServers(long long user_id)
{
    auto devices = db.getListOfServers(user_id);
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    res.write(json(devices).dump(2));  
    return res;
}

crow::response API::getModules(long long server_id)
{
    auto devices = db.getListOfModules(server_id);
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    res.write(json(devices).dump(2));  
    return res;
}

crow::response API::getAllModules()
{
    auto devices = db.getListOfAllModules();
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    res.write(json(devices).dump(2));  
    return res;
}

crow::response API::getDevices()
{
    auto devices = db.getListOfDevices();
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    res.write(json(devices).dump(2));  
    return res;
}

crow::response API::getActuatorsDevices()
{
    auto devices = db.getListOfDevices();
    for (int i = 0; i < devices.size(); i++){
        if (devices[i]["role"] != "actuator")
            devices.erase(devices.begin() + i);
    }
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    res.write(json(devices).dump(2));  
    return res;
}

crow::response API::getModuleCapabilities(long long record_id)
{
    auto capabilities = db.getCapabilities(record_id);
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    res.write(json(capabilities).dump(2)); 
    return res;
}

crow::response API::getModuleNecessaryDevices(long long module_id)
{
    auto necessary_devices = db.getListOfNecessaryDevicesForModule(module_id);
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    res.write(json(necessary_devices).dump(2)); 
    return res;
}

crow::response API::updateServerName(long long server_id, const std::string &new_server_name)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.updateServerName(server_id, new_server_name);
        resp["status"] = true;
        resp["message"] = "Обновление названия сервера прошло успешно";
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Обновление названия сервера прошло с ошибкой";
    }
    res.write(json(resp).dump(2)); 
    return res;
}

crow::response API::deleteServer(long long server_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.deleteServerFromTable(server_id);
        resp["status"] = true;
        resp["message"] = "Удаление сервера прошло успешно";
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Удаление сервера прошло с ошибкой";
    }
    res.write(json(resp).dump(2)); 
    return res;
}

crow::response API::deleteModule(long long record_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.deleteModuleFromTable(record_id);
        resp["status"] = true;
        resp["message"] = "Удаление модуля прошло успешно";
    }
    catch(DataBaseException &e){
        resp["status"] = false;
        resp["message"] = "Удаление модуля прошло с ошибкой";
    }
    res.write(json(resp).dump(2)); 
    return res;
}

crow::response API::auth(const std::string &username, const std::string &password)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    if (db.checkUserAuthentication(username, password)){
        resp["status"] = true;
        resp["message"] = "Аутентификация прошла успешно";
    }
    else{
        resp["status"] = false;
        resp["message"] = "У Вас нет учетной записи.\nДля регистрации нажмите кнопку 'Регистрация'";
    }
    res.write(json(resp).dump(2)); 
    return res;
}

crow::response API::registration(const std::string &username, const std::string &password, 
                                 long int tg_chat_id){
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.addUser(username, password, tg_chat_id);
        resp["status"] = true;
        resp["message"] = "Регистрация прошла успешно!";
    }
    catch(DataBaseException &e){
        std::cerr << "Error: " << e.what() << std::endl;
        resp["status"] = false;
        resp["message"] = "Регистрация прошла с ошибкой";
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::singleAction(long long int device_id, const std::string &action)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    
    try {
        // 1. Формируем MQTT команду
        json mqtt_command;
        mqtt_command["action"] = action;
        mqtt_command["timestamp"] = std::time(nullptr);
        mqtt_command["source"] = "api";  // Откуда пришла команда
        
        // 2. Получаем MQTT топик устройства из БД
        std::string mqtt_topic;
        try {
            
            mqtt_topic = db.getMQTTTopic(device_id);

            if (mqtt_topic.find("/command") == std::string::npos) {
                    mqtt_topic += "/command";
            }
        } catch (const std::exception& e) {
            mqtt_topic = "devices/" + std::to_string(device_id) + "/command";
        }
        
        // 3. Публикуем команду через MQTT
        if (mqtt.isConnected()) {
            mqtt.publish(mqtt_topic, mqtt_command.dump(), 1, false);
                
            resp["status"] = true;
            resp["message"] = "Команда '" + action + "' отправлена устройству №" + 
                            std::to_string(device_id);
            resp["mqtt_topic"] = mqtt_topic;
            resp["command"] = mqtt_command;
        } else {
            resp["status"] = "error";
            resp["message"] = "MQTT не подключен. Команда не отправлена.";
            res.code = 503; // Service Unavailable
        }
        
    } catch (const std::exception& e) {
        resp["status"] = false;
        resp["message"] = "Ошибка: " + std::string(e.what());
        res.code = 500;
    }
    
    res.write(resp.dump(2));
    return res;
}

std::string generateID(size_t length = 16) {
    const std::string characters = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::random_device rd;  
    std::mt19937 gen(rd()); 
    std::uniform_int_distribution<> dis(0, characters.size() - 1);

    std::string id;
    id.reserve(length); 

    for (size_t i = 0; i < length; ++i) {
        id += characters[dis(gen)];
    }

    return id;
}

void API::setupRoutes() {
    // CROW_ROUTE(app, "/")([this](){
    //     return this->handleRoot();
    // });
    
    CROW_ROUTE(app, "/api/devices")([this](){
        return this->getDevices();
    });
    
    CROW_ROUTE(app, "/api/devices/actuators")([this](){
        return this->getActuatorsDevices();
    });

    CROW_ROUTE(app, "/api/servers/add").methods("POST"_method)
    ([this](const crow::request& req){
        
        auto json = crow::json::load(req.body);

        if (!json || !json.has("tg_chat_id") || !json.has("server_name")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        long long user_id = db.getUserIDbyTGChatID(json["tg_chat_id"].i());
        std::string server_name = json["server_name"].s();
        std::string serverID = generateID();

        return this->addServer(user_id, server_name, serverID);
    });

    CROW_ROUTE(app, "/api/servers/edit").methods("PATCH"_method)
    ([this](const crow::request& req){
        
        auto json = crow::json::load(req.body);

        if (!json || !json.has("server_id") || !json.has("new_server_name")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        long long server_id = json["server_id"].i();
        std::string server_name = json["new_server_name"].s();

        return this->updateServerName(server_id, server_name);
    });

    CROW_ROUTE(app, "/api/servers/delete").methods("DELETE"_method)
    ([this](const crow::request& req){
        
        auto json = crow::json::load(req.body);

        if (!json || !json.has("server_id")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        long long server_id = json["server_id"].i();

        return this->deleteServer(server_id);
    });

    CROW_ROUTE(app, "/api/servers")([this](const crow::request& req){
        auto json = crow::json::load(req.body);
        if (!json || !json.has("tg_chat_id")) {
            return crow::response(400, "Invalid JSON or missing fields");
        }
        long long tg_chat_id = json["tg_chat_id"].i();
        long long user_id = db.getUserIDbyTGChatID(tg_chat_id);
        return this->getServers(user_id);        
    });

    CROW_ROUTE(app, "/api/modules/all")([this](){
        return this->getAllModules();        
    });

    CROW_ROUTE(app, "/api/modules")([this](const crow::request& req){
        auto json = crow::json::load(req.body);
        if (!json || !json.has("server_id")) {
            return crow::response(400, "Invalid JSON or missing fields");
        }
        long long server_id = json["server_id"].i();
        return this->getModules(server_id);        
    });

    CROW_ROUTE(app, "/api/modules/capabilities")([this](const crow::request& req){
        auto json = crow::json::load(req.body);
        if (!json || !json.has("record_id")) {
            return crow::response(400, "Invalid JSON or missing fields");
        }
        long long record_id = json["record_id"].i();
        return this->getModuleCapabilities(record_id);        
    });

    CROW_ROUTE(app, "/api/modules/necessary_devices")([this](const crow::request& req){
        auto json = crow::json::load(req.body);
        if (!json || !json.has("module_id")) {
            return crow::response(400, "Invalid JSON or missing fields");
        }
        long long module_id = json["module_id"].i();
        return this->getModuleNecessaryDevices(module_id);        
    });

    CROW_ROUTE(app, "/api/modules/add").methods("POST"_method)
    ([this](const crow::request& req){
        
        auto json = crow::json::load(req.body);

        if (!json || !json.has("module_id") || !json.has("server_id")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        long long server_id = json["server_id"].i();
        long long module_id = json["module_id"].i();

        return this->addModule(server_id, module_id);
    });

    CROW_ROUTE(app, "/api/modules/delete").methods("DELETE"_method)
    ([this](const crow::request& req){
        
        auto json = crow::json::load(req.body);

        if (!json || !json.has("record_id")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        long long record_id = json["record_id"].i();
        
        return this->deleteModule(record_id);
    });

    CROW_ROUTE(app, "/api/users/auth").methods("POST"_method)
    ([this](const crow::request& req){
        auto json = crow::json::load(req.body);
        
        if (!json || !json.has("username") || !json.has("password")) {
            return crow::response(400, "Invalid JSON or missing fields");
        }
        
        std::string username = json["username"].s();
        std::string password = json["password"].s();

        return auth(username, password);
    });

    CROW_ROUTE(app, "/api/users/registration").methods("POST"_method)
    ([this](const crow::request& req){
        auto json = crow::json::load(req.body);
        
        if (!json || !json.has("username") || !json.has("password") || !json.has("tg_chat_id")) {
            return crow::response(400, "Invalid JSON or missing fields");
        }
        
        std::string username = json["username"].s();
        std::string password = json["password"].s();
        long int tg_chat_id = json["tg_chat_id"].i();

        return registration(username, password, tg_chat_id);
    });

    CROW_ROUTE(app, "/api/actions/single").methods("POST"_method)
    ([this](const crow::request& req){
        auto json = crow::json::load(req.body);
        
        if (!json || !json.has("id") || !json.has("action")) {
            return crow::response(400, "Invalid JSON or missing fields");
        }
        
        long long int id = json["id"].i();
        std::string action = json["action"].s();

        return singleAction(id, action);
    });



}