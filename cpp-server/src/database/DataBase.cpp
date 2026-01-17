#include "DataBase.h"

bool QueryResult::empty() const { return rows.empty(); }
size_t QueryResult::size() const { return rows.size(); }

template<typename T>
T QueryResult::get(size_t row, size_t col) const {
    return std::get<T>(rows[row][col]);
}

template<typename T>
T QueryResult::get(size_t row, const std::string& columnName) const {
    auto it = std::find(columnNames.begin(), columnNames.end(), columnName);
    if (it == columnNames.end()) {
        throw std::runtime_error("Column not found: " + columnName);
    }
    size_t col = std::distance(columnNames.begin(), it);
    return get<T>(row, col);
}

QueryResult IDataBase::executeQuery(const std::string& sql,
                                   const std::vector<SQLValue>& params) {
    if (!db) throw DataBaseException("DataBase not opened");
    
    sqlite3_stmt* stmt = nullptr;
    QueryResult result;
    
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw DataBaseException("Failed to prepare statement: " + 
                               std::string(sqlite3_errmsg(db)));
    }
    
    // Привязка параметров 
    for (size_t i = 0; i < params.size(); ++i) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            
            if constexpr (std::is_same_v<T, std::string>) {
                sqlite3_bind_text(stmt, i + 1, arg.c_str(), -1, SQLITE_TRANSIENT);
            }
            else if constexpr (std::is_same_v<T, int>) {
                sqlite3_bind_int(stmt, i + 1, arg);
            }
            else if constexpr (std::is_same_v<T, double>) {
                sqlite3_bind_double(stmt, i + 1, arg);
            }
            else if constexpr (std::is_same_v<T, long long>) {
                sqlite3_bind_int64(stmt, i + 1, arg);
            }
        }, params[i]);
    }
    
    // Получение имен колонок
    int columnCount = sqlite3_column_count(stmt);
    for (int i = 0; i < columnCount; ++i) {
        result.columnNames.push_back(sqlite3_column_name(stmt, i));
    }
    
    // Получение данных
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::vector<SQLValue> row;
        
        for (int i = 0; i < columnCount; ++i) {
            int type = sqlite3_column_type(stmt, i);
            
            switch (type) {
                case SQLITE_INTEGER:
                    row.push_back(sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    row.push_back(sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT:
                    row.push_back(reinterpret_cast<const char*>(
                        sqlite3_column_text(stmt, i)));
                    break;
                case SQLITE_NULL:
                    row.push_back(std::string());  // Или специальное значение
                    break;
                default:
                    row.push_back(std::string());
            }
        }
        
        result.rows.push_back(row);
    }
    
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DataBaseException("Query failed: " + 
                               std::string(sqlite3_errmsg(db)));
    }
    
    sqlite3_finalize(stmt);
    return result;
}

IDataBase::IDataBase(const std::string &path_to_IDataBase){
    open(path_to_IDataBase);
}

IDataBase::~IDataBase(){
    close();
}

void IDataBase::open(const std::string& path_to_IDataBase){
    if (db)
        close();
    int db_session = sqlite3_open(path_to_IDataBase.c_str(), &db);
    if (db_session != SQLITE_OK){
        std::string err = sqlite3_errmsg(db);
        sqlite3_close(db);
        db = nullptr;
        throw DataBaseException("Cannot open DataBase: " + err);
    }
    std::cout << "DataBase has been opened successfully!" << std::endl;
}

void IDataBase::close(){
    if (db){
        sqlite3_close(db);
        db = nullptr;
    }
}

bool IDataBase::isOpen() const {
    return db != nullptr;
}

void IDataBase::executeRequest(const std::string &sql){
    if (!db)
        throw DataBaseException("DataBase not opened");
    char* err_msg = nullptr;
    int response = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (response != SQLITE_OK){
        std::string err = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw DataBaseException("SQL error: " + err + "\nQuery: " + sql);
    }
}

void IDataBase::executeRequest(const std::string& sql, 
                              const std::vector<SQLValue>& params) {
    if (!db) 
        throw DataBaseException("DataBase not opened");
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        throw DataBaseException("Failed to prepare statement: " + 
                               std::string(sqlite3_errmsg(db)));
    }
    
    // Привязка параметров с разными типами
    for (size_t i = 0; i < params.size(); ++i) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            
            if constexpr (std::is_same_v<T, std::string>) {
                sqlite3_bind_text(stmt, i + 1, arg.c_str(), -1, SQLITE_TRANSIENT);
            }
            else if constexpr (std::is_same_v<T, int>) {
                sqlite3_bind_int(stmt, i + 1, arg);
            }
            else if constexpr (std::is_same_v<T, double>) {
                sqlite3_bind_double(stmt, i + 1, arg);
            }
            else if constexpr (std::is_same_v<T, long long>) {
                sqlite3_bind_int64(stmt, i + 1, arg);
            }
        }, params[i]);
    }
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_OK && rc != SQLITE_ROW) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        throw DataBaseException("Execution failed: " + err + "\nQuery: " + sql);
    }
    
    sqlite3_finalize(stmt);
}

void DataBase::commit(){
    executeRequest("COMMIT");
}

void DataBase::rollback(){
    executeRequest("ROLLBACK");
}

void DataBase::createUserTable(){
    executeRequest(R"(
        CREATE TABLE users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL, 
            telegram_chat_id BIGINT UNIQUE,  
            role TEXT DEFAULT 'user',
            status TEXT DEFAULT 'active'
        );    
    )");
}

void DataBase::createDeviceTable(){
    executeRequest(R"(
        CREATE TABLE devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL, 
            device_type_id INTEGER NOT NULL,
            mqtt_topic TEXT UNIQUE NOT NULL, 
            location TEXT,
            status BOOLEAN DEFAULT 0,
            FOREIGN KEY (device_type_id) REFERENCES device_types(id)
        );   
    )");
}

void DataBase::createDeviceTypeTable(){
    executeRequest(R"(
        CREATE TABLE device_types (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL, 
            description TEXT,
            capabilities JSON 
        ); 
    )");
}

void DataBase::createTriggerTable(){
    executeRequest(R"(
        CREATE TABLE triggers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL, 
            description TEXT
        ); 
    )");
}

void DataBase::createScenarioTable(){
    executeRequest(R"(
        CREATE TABLE scenarios (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            trigger_id INTEGER NOT NULL,
            device_id INTEGER NOT NULL,
            FOREIGN KEY (trigger_id) REFERENCES triggers(id),
            FOREIGN KEY (device_id) REFERENCES devices(id)
        ); 
    )");
}

void DataBase::deleteUsersTable(){
    std::string sql = R"(
        DROP TABLE users;
    )";
    executeRequest(sql);
}

void DataBase::clearDeviceTypesTable(){
    std::string sql = R"(
        DELETE FROM device_types;
    )";
    executeRequest(sql);
}

void DataBase::addUser(const std::string& user_name, const std::string& password,
                       long int tg_chat_id, const std::string& role,
                       const std::string& status){
    
    std::string sql = R"(
        INSERT INTO users (username, password, telegram_chat_id, role, status)
        VALUES (?, ?, ?, ?, ?)
    )";
    
    executeRequest(sql, {
        user_name,       
        password,        
        tg_chat_id,   
        role,
        status
    });

}

void DataBase::addDeviceType(const std::string &name, const std::string &description, const json &config){
    
    std::string sql = R"(
        INSERT INTO device_types (name, description, capabilities)
        VALUES (?, ?, ?)
    )";
    
    std::string config_str = config.dump();
    
    executeRequest(sql, {name, description, config_str});
}

void DataBase::addDevice(const std::string &name, int device_type_id, const std::string &mqtt_topic, const std::string &location, bool status){
    std::string sql = R"(
        INSERT INTO devices (name, device_type_id, mqtt_topic, location, status)
        VALUES (?, ?, ?, ?, ?)
    )";
    
    executeRequest(sql, {name, device_type_id, mqtt_topic, location, status});
}

std::vector<json> DataBase::getListOfDevices()
{
    std::vector<json> list_of_devices;
    std::string sql = R"(
        SELECT 
            d.name as device_name,
            dt.name as device_type
        FROM devices d
        JOIN device_types dt ON d.device_type_id = dt.id
        ORDER BY d.name;
    )";
    QueryResult response = executeQuery(sql);
    json device;
    for (size_t i = 0; i < response.rows.size(); i++){
        device["device_name"] = response.get<std::string>(i, "device_name");
        device["type"] = response.get<std::string>(i, "device_type");
        list_of_devices.push_back(device);
    }
    return list_of_devices;
}

bool DataBase::checkUserAuthentication()
{
    
    return false;
}
