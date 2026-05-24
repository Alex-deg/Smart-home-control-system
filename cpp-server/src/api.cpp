#include "../include/api.hpp"

API::API(DataBase &_db, bool _debugFlag) : db(_db), debugFlag(_debugFlag) {}

void API::run(int _port, bool multithreaded) {
    setupRoutes();
    if (multithreaded) {
        app.port(_port).multithreaded().run();
        if (debugFlag){
            std::cout << "API has been launched in multithread mode on " << _port << " port" << std::endl;
        }
    } else {
        app.port(_port).run();
        if (debugFlag)
            std::cout << "API has been launched in basic mode on " << _port << " port" << std::endl;
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
        if(debugFlag)
            std::cout << "Receiving telemetry was successful" << std::endl;
        return res;
    }
    catch(DataBaseException &e){
        std::cerr << "API::getTelemetry(): " << e.what() << std::endl;
        resp["status"] = false;
        resp["message"] = "Получение телеметрии прошло с ошибкой";
        if(debugFlag)
            std::cout << "Receiving telemetry occured with error" << std::endl;
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
        if(debugFlag)
            std::cout << "Anomaly tagging was successful" << std::endl;
    }
    catch(DataBaseException &e){
        std::cerr << "API::anomalyTagging(): " << e.what() << std::endl;
        resp["status"] = false;
        resp["message"] = "Отметка аномальных данных прошла с ошибкой";
        if(debugFlag)
            std::cout << "Anomaly tagging occured with error" << std::endl;
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
        if(debugFlag)
            std::cout << "Receiving diagnostic data was successful" << std::endl;
        return res;
    }
    catch(DataBaseException &e){
        std::cerr << "API::getModuleParams(): " << e.what() << std::endl;
        resp["status"] = false;
        resp["message"] = "Получение данных прошло с ошибкой";
        if(debugFlag)
            std::cout << "Receiving diagnostic data occured with error" << std::endl;
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
        if(debugFlag)
            std::cout << "Receiving unique module ids was successful" << std::endl;
        return res;
    }
    catch(DataBaseException &e){
        std::cerr << "API::getUniqueModuleIDs(): " << e.what() << std::endl;
        resp["status"] = false;
        resp["message"] = "Получение данных прошло с ошибкой";
        if(debugFlag)
            std::cout << "Receiving unique module ids occured with error" << std::endl;
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