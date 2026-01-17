#include "api.h"

// Перепроверить конструктор
API::API(DataBase &db){
    this->db = db;
}

void API::run(int _port, bool multithreaded) {
    setupRoutes();
    if (multithreaded) {
        app.port(_port).multithreaded().run();
    } else {
        app.port(_port).run();
    }
}

std::string API::handleRoot() {
    return R"(
        <html>
        <head><title>SmartHome</title></head>
        <body>
            <h1>Smart home system control</h1>
            <p>API server is working</p>
            <ul>
                <li><a href="/api/status">System status</a></li>
                <li><a href="/api/devices">Devices</a></li>
            </ul>
        </body>
        </html>
        )";
}

crow::response API::getDevices() {
    auto devices = db.getListOfDevices();
    crow::response res;
    res.add_header("Content-Type", "application/json; charset=utf-8");
    res.write(json(devices).dump(2));  
    return res;
}

bool API::checkUser()
{

    return false;
}

std::string API::handleStatus() {
    return "System status: Online";
}

void API::setupRoutes() {
    CROW_ROUTE(app, "/")([this](){
        return this->handleRoot();
    });
    
    CROW_ROUTE(app, "/api/devices")([this](){
        return this->getDevices();
    });
    
    CROW_ROUTE(app, "/api/status")([this](){
        return this->handleStatus();
    });
    
    CROW_ROUTE(app, "/api/users")([this](){
        
        return "List of users";
    });
}