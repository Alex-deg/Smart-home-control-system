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

    CROW_ROUTE(app, "/api/database/telemetry/<int>/<string>/<int>")
    ([this](long long module_id, std::string param_name, long long time_interval){
        return getTelemetry(module_id, param_name, time_interval);
    });

}