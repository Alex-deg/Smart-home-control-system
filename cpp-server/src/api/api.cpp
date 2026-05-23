#include "api.h"

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

crow::response API::getModuleParams(long long module_id, int time_interval, bool with_anomaly)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        auto params = db.getModuleParams(module_id, time_interval, with_anomaly);
        res.write(json(params).dump(2));
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

crow::response API::getUniqueModuleIDs()
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        auto params = db.getUniqueModuleIDs();
        res.write(json(params).dump(2));
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

crow::response API::anomalyTagging(std::vector<long long> record_ids)
{
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    json resp;
    try{
        db.anomalyTagging(record_ids);
        resp["status"] = true;
        resp["message"] = "Получение данных прошло успешно";
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

    CROW_ROUTE(app, "/")
    ([this](){
        return "HELLO :)";
    });

    CROW_ROUTE(app, "/api/database/telemetry")
    .methods(crow::HTTPMethod::GET)
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
    .methods(crow::HTTPMethod::GET)
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
    .methods(crow::HTTPMethod::GET)
    ([this](const crow::request& req){
        return getUniqueModuleIDs();
    });

    CROW_ROUTE(app, "/api/database/params/anomaly_tagging")
    .methods(crow::HTTPMethod::POST)
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

}