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
            device_type_id INTEGER NOT NULL,
            mqtt_topic TEXT UNIQUE NOT NULL, 
            status BOOLEAN DEFAULT 0,
            FOREIGN KEY (device_type_id) REFERENCES device_types(id)
        );   
    )");
}

void DataBase::createDeviceTypeTable(){
    // role - роль устройства, например, датчик или актор
    executeRequest(R"(
        CREATE TABLE device_types (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,   
            role TEXT NOT NULL, 
            capabilities TEXT, 
            description TEXT            
        ); 
    )");
}

void DataBase::updateServerName(long long server_id, const std::string &new_server_name){
    std::string sql = R"(
        UPDATE servers 
        SET name = ?
        WHERE id = ?
    )";

    executeRequest(sql, {new_server_name, server_id});
}

void DataBase::updateDeviceType(int device_id, int device_type_id)
{
    std::string sql = R"(
        UPDATE devices 
        SET device_type_id = ?
        WHERE id = ?
    )";
    
    executeRequest(sql, {device_type_id, device_id});
}

void DataBase::updateDeviceStatus(const std::string &payload, const std::string &topic_pattern){
    std::string sql = R"(
        UPDATE devices 
        SET status = ?
        WHERE mqtt_topic LIKE ?
    )";
    executeRequest(sql, {payload, topic_pattern});      
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

void DataBase::createServerTable(){
    executeRequest(R"(
        CREATE TABLE servers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,      
            server_id TEXT UNIQUE NOT NULL   
        ); 
    )");
}

void DataBase::createMQTTMessagesTable(){
    executeRequest(R"(
        CREATE TABLE mqtt_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            topic TEXT NOT NULL,
            payload TEXT NOT NULL,
            direction TEXT NOT NULL,
            created_at TEXT
        ); 
    )");
}

void DataBase::createModulesTable(){
    executeRequest(R"(
        CREATE TABLE modules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            description TEXT NOT NULL
        ); 
    )");
}

void DataBase::createUsersAndServersTable(){
    executeRequest(R"(
        CREATE TABLE users_servers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            server_id INTEGER NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id),
            FOREIGN KEY (server_id) REFERENCES servers(id)
        ); 
    )");
}

void DataBase::createServersAndModulesTable(){
    executeRequest(R"(
        CREATE TABLE servers_modules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            server_id INTEGER NOT NULL,
            module_id INTEGER NOT NULL,
            FOREIGN KEY (server_id) REFERENCES servers(id),
            FOREIGN KEY (module_id) REFERENCES modules(id)
        ); 
    )");
}

void DataBase::createModuleFillingTable(){
    executeRequest(R"(
        CREATE TABLE modules_filling (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            module_id INTEGER NOT NULL,
            device_type_id INTEGER NOT NULL,
            FOREIGN KEY (device_type_id) REFERENCES device_types(id),
            FOREIGN KEY (module_id) REFERENCES modules(id)
        );
    )");
}

void DataBase::createModulesAndDevicesTable(){
    executeRequest(R"(
        CREATE TABLE modules_devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            module_id INTEGER NOT NULL,
            device_id INTEGER NOT NULL,
            FOREIGN KEY (device_id) REFERENCES devices(id),
            FOREIGN KEY (module_id) REFERENCES modules(id)
        );
    )");
}

void DataBase::deleteDeviceTypesTable(){
    std::string sql = R"(
        DROP TABLE device_types;
    )";
    executeRequest(sql);
}

void DataBase::deleteDeviceTable(){
    executeRequest(R"(
        DROP TABLE devices;    
    )");
}

void DataBase::deleteServerFromTable(long long server_id){

    std::string sql = R"(
        DELETE FROM servers
        WHERE id = ?
    )";

    executeRequest(sql, {server_id});

    sql = R"(
        DELETE FROM users_servers
        WHERE server_id = ?
    )";

    executeRequest(sql, {server_id});

    sql = R"(
        DELETE FROM servers_modules
        WHERE server_id = ?
    )";

    executeRequest(sql, {server_id});
}

void DataBase::deleteModuleFromTable(long long record_id){
    std::string sql = R"(
        DELETE FROM servers_modules
        WHERE id = ?
    )";
    executeRequest(sql, {record_id});
}

void DataBase::clearDeviceTypesTable(){
    std::string sql = R"(
        DELETE FROM device_types;
    )";
    executeRequest(sql);
}

void DataBase::clearModuleFillingTable(){
    std::string sql = R"(
        DELETE FROM modules_filling
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

void DataBase::addMQTTMessage(const std::string &topic, const std::string &payload,
                              bool incoming){
    std::string sql = R"(
        INSERT INTO mqtt_messages (topic, payload, direction, created_at)
        VALUES (?, ?, ?, datetime('now'))
    )";

    executeRequest(sql, {
        topic,
        payload,
        incoming ? "incoming" : "outgoing"
    });
}

void DataBase::addDeviceType(const std::string &name, const std::string &role, const std::string &description, const json &config){
    
    std::string sql = R"(
        INSERT INTO device_types (name, role, capabilities, description)
        VALUES (?, ?, ?, ?)
    )";
        
    executeRequest(sql, {name, role, config.dump(), description});
}

void DataBase::addDevice(const std::string &name, int device_type_id, const std::string &mqtt_topic, const std::string &location, bool status){
    std::string sql = R"(
        INSERT INTO devices (name, device_type_id, mqtt_topic, location, status)
        VALUES (?, ?, ?, ?, ?)
    )";
    
    executeRequest(sql, {name, device_type_id, mqtt_topic, location, status});
}

void DataBase::addServer(long long user_id, const std::string &server_name, 
                         const std::string &server_id){
    
    std::string sql = R"(
        INSERT INTO servers (name, server_id)
        VALUES (?, ?)
    )";
    
    executeRequest(sql, {server_name, server_id});

    sql = R"(
        SELECT
            id
        FROM servers
        WHERE server_id = ?
    )";

    QueryResult qr = executeQuery(sql, {server_id});
    long long sID = qr.get<long long>(0, 0);

    sql = R"(
        INSERT INTO users_servers (user_id, server_id)
        VALUES (?, ?)
    )";
    
    executeRequest(sql, {user_id, sID});
}

void DataBase::addModule(long long server_id, long long module_id){

    std::string sql = R"(
        INSERT INTO servers_modules(server_id, module_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {server_id, module_id});
}

void DataBase::adminAddModule(const std::string &name, const std::string &description){

    std::string sql = R"(
        INSERT INTO modules(name, description)
        VALUES (?, ?)
    )";
    executeRequest(sql, {name, description});
}

void DataBase::addModulesDevicesRecord(long long module_id, long long device_id){
    std::string sql = R"(
        INSERT INTO modules_devices (module_id, device_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {module_id, device_id});
}

void DataBase::adminFillModules(long long module_id, long long device_type_id){
    std::string sql = R"(
        INSERT INTO modules_filling (module_id, device_type_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {module_id, device_type_id});
}

std::vector<json> DataBase::getListOfDevices()
{
    std::vector<json> list_of_devices;
    std::string sql = R"(
        SELECT 
            d.id as device_id,
            d.name as device_name,
            dt.name as device_type,
            dt.role as device_role,
            dt.capabilities as device_actions  
        FROM devices d
        JOIN device_types dt ON d.device_type_id = dt.id
        ORDER BY d.name;
    )";
    QueryResult response = executeQuery(sql);
    json device;
    
    for (size_t i = 0; i < response.rows.size(); i++){
        device["id"] = response.get<long long>(i, "device_id");
        device["name"] = response.get<std::string>(i, "device_name");
        device["type"] = response.get<std::string>(i, "device_type");
        device["role"] = response.get<std::string>(i, "device_role");
        device["actions"] = json::parse(response.get<std::string>(i, "device_actions"));
        list_of_devices.push_back(device);
    }
    return list_of_devices;
}

std::vector<json> DataBase::getListOfServers(long long user_id)
{
    std::vector<json> list_of_servers;
    std::string sql = R"(
        SELECT 
            s.id as serverID,
            s.name as server_name,
            s.server_id as server_id  
        FROM users_servers us
        JOIN servers s ON us.server_id = s.id
        WHERE user_id = ?;
    )";

    QueryResult response = executeQuery(sql, {user_id});
    json server;
    
    for (size_t i = 0; i < response.rows.size(); i++){
        server["serverID"] = response.get<long long>(i, "serverID");
        server["name"] = response.get<std::string>(i, "server_name");
        server["server_id"] = response.get<std::string>(i, "server_id");
        list_of_servers.push_back(server);
    }
    return list_of_servers;
}

std::vector<json> DataBase::getListOfModules(long long server_id)
{
    std::vector<json> list_of_modules;
    std::string sql = R"(
        SELECT 
            m.id as module_id,
            m.name as module_name,
            m.description as module_description, 
            sm.id as record_id 
        FROM servers_modules sm
        JOIN modules m ON sm.module_id = m.id
        WHERE server_id = ?;
    )";

    QueryResult response = executeQuery(sql, {server_id});
    json module_;
    
    for (size_t i = 0; i < response.rows.size(); i++){
        module_["id"] = response.get<long long>(i, "module_id");
        module_["name"] = response.get<std::string>(i, "module_name");
        module_["description"] = response.get<std::string>(i, "module_description");
        module_["record_id"] = response.get<long long>(i, "record_id");
        list_of_modules.push_back(module_);
    }
    return list_of_modules;
}

std::vector<json> DataBase::getListOfAllModules()
{
    std::vector<json> list_of_modules;
    std::string sql = R"(
        SELECT 
            *
        FROM modules;
    )";

    QueryResult response = executeQuery(sql);
    json module_;
    
    for (size_t i = 0; i < response.rows.size(); i++){
        module_["id"] = response.get<long long>(i, "id");
        module_["name"] = response.get<std::string>(i, "name");
        module_["description"] = response.get<std::string>(i, "description");
        list_of_modules.push_back(module_);
    }
    return list_of_modules;
}

std::vector<json> DataBase::getCapabilities(long long record_id)
{
    std::vector<json> capabilities;
    std::string sql = R"(
        SELECT 
            capabilities as module_capabilities
        FROM modules_filling mf
        JOIN device_types dt ON mf.device_type_id = dt.id
        WHERE module_id = ?;
    )";

    long long module_id = getModuleIDFromRecordID(record_id);
    QueryResult response = executeQuery(sql, {module_id});
    for (int i = 0; i < response.size(); i++){
        auto capability = json::parse(response.get<std::string>(i, "module_capabilities"));
        if (capability != NULL)
            capabilities.push_back(capability);
    }
    return capabilities;
}

std::vector<std::string> DataBase::getListOfNecessaryDevicesForModule(long long module_id)
{
    std::vector<std::string> necessary_devices;
    std::string sql = R"(
        SELECT 
            name as device_type
        FROM modules_filling mf
        JOIN device_types dt ON mf.device_type_id = dt.id
        WHERE module_id = ?;
    )";
    QueryResult response = executeQuery(sql, {module_id});
    for (int i = 0; i < response.size(); i++){
        auto device_type = response.get<std::string>(i, "device_type");
        necessary_devices.push_back(device_type);
    }
    return necessary_devices;
}

long long DataBase::getModuleIDFromRecordID(long long record_id)
{
    std::string sql = R"(
        SELECT 
            module_id
        FROM servers_modules
        WHERE id = ?
    )";
    std::cout << "record_id = " << record_id << std::endl;
    QueryResult response = executeQuery(sql, {record_id});
    return response.get<long long>(0, "module_id");
}

long long DataBase::getUserIDbyTGChatID(long long tg_chat_id)
{
    std::string sql = R"(
        SELECT 
            users.id 
        FROM users WHERE telegram_chat_id = ?
    )";

    QueryResult response = executeQuery(sql, {tg_chat_id});

    return response.get<long long>(0, 0);
}

bool DataBase::checkUserAuthentication(const std::string &username, const std::string &password)
{
    std::string sql = R"(
        SELECT 
            users.username,
            users.password 
        FROM users WHERE username = ?
    )";

    QueryResult response = executeQuery(sql, {username});

    if (response.size() == 0)
        return false;
    if (response.get<std::string>(0, "password") != password)
        return false;

    return true;
}

std::string DataBase::getMQTTTopic(unsigned int id){
    std::string sql = "SELECT mqtt_topic FROM devices WHERE id = ?";
    auto result = executeQuery(sql, {id});
    return result.get<std::string>(0, 0);
}

// int DataBase::getIDFromDeviceName(const std::string &device_name){
//     std::string sql = R"(
//         SELECT id FROM devices WHERE name = ?
//     )";
//     auto result = executeQuery(sql, {device_name});
//     return result.get<int>(0,0);
// }
