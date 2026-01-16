#include <iostream>
#include "../include/DataBase.h"
#include "../include/crow_all.h"

int main(){

    DataBase db;
    db.open("../Data/smart_home.db");

    db.execute_request(
        "DELETE FROM users"
    );

    db.execute_request(
        "INSERT INTO users (name, password)"
        " VALUES "
        "('Alex', 'abc123')"   
    );

    crow::SimpleApp app;
    CROW_ROUTE(app, "/")([](){
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
    });

    CROW_ROUTE(app, "/api/devices/")([](){
        return "List of devices :)";
    });

    app.port(8080).run();

    return 0;
}