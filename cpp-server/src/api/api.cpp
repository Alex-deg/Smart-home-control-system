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

crow::response API::addServer(long long user_id, const std::string &server_name, const std::string &server_key)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.addServer(user_id, server_name, server_key);
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

crow::response API::addModule(long long server_id, long long module_type_id, const std::string& alias)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        resp["module_id"] = db.addModule(server_id, module_type_id, alias);
        resp["status"] = true;
        resp["message"] = "Добавление модуля прошло успешно!";
    }
    catch(DataBaseException &e){
        std::cerr << "Error: " << e.what() << std::endl;
        resp["module_id"] = -1;
        resp["status"] = false;
        resp["message"] = "Добавление модуля прошло с ошибкой";
    }            
    res.write(json(resp).dump(2));           
    return res;
}

crow::response API::addDevice(long long module_id, long long device_type_id, const std::string &mqtt_topic, const std::string &alias){
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.addDevice(module_id, device_type_id, mqtt_topic, alias);
        resp["status"] = true;
        resp["message"] = "Добавление устройства прошло успешно!";
    }
    catch(DataBaseException &e){
        std::cerr << "Error: " << e.what() << std::endl;
        resp["status"] = false;
        resp["message"] = "Добавление устройства прошло с ошибкой";
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

crow::response API::getModulesTypes(long long user_id)
{
    auto devices = db.getListOfAllModuleTypes();
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    res.write(json(devices).dump(2));  
    return res;
}

crow::response API::getModuleCapabilities(long long module_id)
{
    auto capabilities = db.getCapabilities(module_id);
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    res.write(json(capabilities).dump(2)); 
    return res;
}

crow::response API::getModuleNecessaryDevices(long long module_type_id)
{
    auto necessary_devices = db.getListOfNecessaryDevicesForModule(module_type_id);
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
        db.deleteServerFromTables(server_id);
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

crow::response API::deleteModule(long long module_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.deleteModuleFromTables(module_id);
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
    auto info = db.checkUserAuthentication(username, password);
    if (info.first){
        resp["status"] = true;
        resp["message"] = "Аутентификация прошла успешно";
    }
    else{
        resp["status"] = false;
        resp["message"] = "У Вас нет учетной записи.\nДля регистрации нажмите кнопку 'Регистрация'";
    }
    resp["user_id"] = info.second;
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

crow::response API::capabilityHandler(long long module_id, long long capability_id)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;

    std::cout << "DONE1" << std::endl;
    std::vector<json> actions_devices = db.getListOfDevicesForActions(module_id, capability_id);
    std::cout << "DONE" << std::endl;
    for (auto &&action_device : actions_devices){
        actionHandler(action_device["action"], action_device["device"]);
    }
    return res;
}

crow::response API::actionHandler(json action_info, json device_info)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;

    if (action_info["name"] == "turn_on"){
        res = singleAction(device_info["mqtt_topic"], action_info["name"]);
    }

    return res;
}

crow::response API::singleAction(const std::string& mqtt_topic , const std::string &action)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    
    try {

        json mqtt_command;
        mqtt_command["action"] = action;
                
        if (mqtt.isConnected()) {
            mqtt.publish(mqtt_topic, mqtt_command.dump(), 1, false);
            resp["status"] = true;
            resp["message"] = "Команда '" + action + "' отправлена";
        } else {
            resp["status"] = "error";
            resp["message"] = "MQTT не подключен. Команда не отправлена.";
            res.code = 503; 
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

std::string API::generateMQTTTopic(long long user_id, long long server_id, 
                                   long long module_id){
    return "user/" + std::to_string(user_id) + "/server/" + std::to_string(server_id) + 
           "/module/" + std::to_string(module_id);
}

void API::setupRoutes()
{

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
        long int tg_chat_id  = json["tg_chat_id"].i();

        return registration(username, password, tg_chat_id);
    });

    CROW_ROUTE(app, "/api/users/<int>/servers")([this]
    (const crow::request& req, int user_id){
        return this->getServers(user_id);
    });

    CROW_ROUTE(app, "/api/users/<int>/servers/add").methods("POST"_method)
    ([this](const crow::request& req, int user_id){
        
        auto json = crow::json::load(req.body);

        if (!json || !json.has("server_name")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        std::string server_name = json["server_name"].s();
        std::string server_key = generateID();

        return this->addServer(user_id, server_name, server_key);
    });

    CROW_ROUTE(app, "/api/users/<int>/servers/<int>/edit").methods("PATCH"_method)
    ([this](const crow::request& req, int user_id, int server_id){
        
        auto json = crow::json::load(req.body);

        if (!json || !json.has("new_server_name")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        std::string server_name = json["new_server_name"].s();

        return this->updateServerName(server_id, server_name);
    });

    CROW_ROUTE(app, "/api/users/<int>/servers/<int>/delete").methods("DELETE"_method)
    ([this](const crow::request& req, int user_id, int server_id){
        return this->deleteServer(server_id);
    });


    CROW_ROUTE(app, "/api/users/<int>/servers/<int>/modules")([this]
    (const crow::request& req, int user_id, int server_id){
        return this->getModules(server_id);        
    });

    CROW_ROUTE(app, "/api/users/<int>/servers/<int>/modules/types")([this]
    (int user_id, int server_id){
        return this->getModulesTypes(user_id);
    });

    CROW_ROUTE(app, "/api/users/<int>/servers/<int>/modules/types/<int>/necessary_devices")([this]
    (const crow::request& req, int user_id, int server_id, int module_type_id){
        return this->getModuleNecessaryDevices(module_type_id);
    });

    CROW_ROUTE(app, "/api/users/<int>/servers/<int>/modules/<int>/capabilities")([this]
    (const crow::request& req, int user_id, int server_id, int module_id){
        return this->getModuleCapabilities(module_id);   
    });

    CROW_ROUTE(app, "/api/users/<int>/servers/<int>/modules/<int>/capabilities/<int>").methods("POST"_method)
    ([this](const crow::request& req, int user_id, int server_id, int module_id, int capability_id){
        return this->capabilityHandler(module_id, capability_id);   
    });

    CROW_ROUTE(app, "/api/users/<int>/servers/<int>/modules/<int>/add_devices").methods("POST"_method)
    ([this](const crow::request& req, int user_id, int server_id, int module_id){
        
        auto json = crow::json::load(req.body);

        if (!json || !json.has("device_type_id") || !json.has("alias")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        long long device_type_id = json["device_type_id"].i();
        std::string alias = json["alias"].s();

        return this->addDevice(module_id, device_type_id, generateMQTTTopic(user_id, server_id, module_id), alias);
    });

    CROW_ROUTE(app, "/api/users/<int>/servers/<int>/modules/add").methods("POST"_method)
    ([this](const crow::request& req, int user_id, int server_id){
        
        auto json = crow::json::load(req.body);

        if (!json || !json.has("module_type_id") || !json.has("alias")){
            return crow::response(400, "Invalid JSON or missing fields");
        }

        long long module_type_id = json["module_type_id"].i();
        std::string alias = json["alias"].s();

        return this->addModule(server_id, module_type_id, alias);
    });

    CROW_ROUTE(app, "/api/users/<int>/servers/<int>/modules/<int>/delete").methods("DELETE"_method)
    ([this](const crow::request& req, int user_id, int server_id, int module_id){       
        return this->deleteModule(module_id);
    });


}