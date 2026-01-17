#include <iostream>
#include "database/DataBase.h"
#include "api/api.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main(){

    DataBase db;
    db.open("../Data/smart_home.db");
    API api(db);
    api.run();
    return 0;
}