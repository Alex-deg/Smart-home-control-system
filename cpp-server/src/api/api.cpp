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

crow::response API::getDevices() {
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