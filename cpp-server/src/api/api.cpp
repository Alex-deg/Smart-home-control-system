#include "api.h"

// Перепроверить конструктор
API::API(DataBase &db_) : db(db_){}

void API::run(int _port, bool multithreaded) {
    setupRoutes();
    if (multithreaded) {
        app.port(_port).multithreaded().run();
    } else {
        app.port(_port).run();
    }
}

crow::response API::getTelemetry(long long module_id, const std::string &param_name, int time_interval)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        auto telemetry = db.getTelemtry(module_id, param_name, time_interval);
        res.write(json(telemetry).dump(2));  
        return res;
    }
    catch(DataBaseException &e){
        std::cerr << "Error: " << e.what() << std::endl;
        resp["status"] = false;
        resp["message"] = "Получение данных прошло с ошибкой";
    }            
    res.write(json(resp).dump(2));           
    return res;
}

void API::setupRoutes()
{

    CROW_ROUTE(app, "/api/database/telemetry")
    ([this](const crow::request& req){
        auto json = crow::json::load(req.body);
        
        if (!json || !json.has("module_id") || !json.has("param_name") || !json.has("time_interval")) {
            return crow::response(400, "Invalid JSON or missing fields");
        }
        
        long long module_id = json["module_id"].i();
        std::string param_name = json["param_name"].s();
        long long time_interval = json["time_interval"].i();
        
        return getTelemetry(module_id, param_name, time_interval);
    });

}