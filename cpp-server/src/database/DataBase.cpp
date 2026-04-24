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




void DataBase::createModulesTable(){
    
    /**
     * @brief Создание таблицы для хранения модулей
     * @details name - имя модуля
     *          alias - псевдоним модуля
     *          Alias нужен для распознавания одинаковых модулей на одном сервере. Например,
     *          у пользователя 2 умные розетки в квартире и чтобы их различать он назначает
     *          alias, который при выводе списка модулей на сервере будет выводиться
     *          справа от названия модуля в квадратных скобках (Умная розетка [Кухня]
     *                                                          Умная розетка [Спальня])
     *          mqtt_topic - топик для обработки пользовательских команд
     */

    executeRequest(R"(
        CREATE TABLE modules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            alias TEXT NOT NULL,
            mqtt_topic TEXT NOT NULL
        ); 
    )");
}

void DataBase::createModulesAndCapabilitiesTable(){

    /**
     * @brief Создание таблицы для хранения модулей
     * @details module_id - id модуля
     *          alias - псевдоним модуля
     *          Alias нужен для распознавания одинаковых модулей на одном сервере. Например,
     *          у пользователя 2 умные розетки в квартире и чтобы их различать он назначает
     *          alias, который при выводе списка модулей на сервере будет выводиться
     *          справа от названия модуля в квадратных скобках (Умная розетка [Кухня]
     *                                                          Умная розетка [Спальня])
     *          mqtt_topic - топик для обработки пользовательских команд
     */

    executeRequest(R"(
        CREATE TABLE modules_capabilities (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            module_id INTEGER NOT NULL,
            capability_id INTEGER NOT NULL,
            FOREIGN KEY (module_id) REFERENCES modules(id),
            FOREIGN KEY (capability_id) REFERENCES capabilities(id)
        ); 
    )");

}

void DataBase::createCapabilitiesTable(){

    /**
     * @brief Создание таблицы для хранения функий удаленного доступа к модулю
     * @details name - имя функции
     */

    executeRequest(R"(
        CREATE TABLE capabilities (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL
        );   
    )");
}

void DataBase::createTelemetryTable(){

    /**
     * @brief Создание таблицы для хранения функий удаленного доступа к модулю
     * @details module_id - id модуля 
     * @details param_name - название параметра
     * @details param_value - значение параметра
     * @details timestamp - временная метка замера показания датчика
     */

    executeRequest(R"(
        CREATE TABLE telemetry (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            module_id INTEGER NOT NULL,
            param_name TEXT NOT NULL,
            param_value REAL NOT NULL,
            timestamp INTEGER NOT NULL,
            FOREIGN KEY (module_id) REFERENCES module_id(id)
        );   
    )");
}

void DataBase::deleteModulesTable(){
    
    /**
     * @brief Удаление таблицы с модулями
     */

    executeRequest(R"(
        DROP TABLE modules;    
    )");
}

void DataBase::deleteModulesAndCapabilities(){

    /**
     * @brief Удаление таблицы с модулями и привязанными к ним возможностями
     */

    executeRequest(R"(
        DROP TABLE modules_capabilities;
    )");
}

void DataBase::deleteCapabilitiesTable(){

    /**
     * @brief Удаление таблицы с возможностями
     */

    executeRequest(R"(
        DROP TABLE capabilities;    
    )");
}

void DataBase::deleteTelemetryTable(){

    /**
     * @brief Удаление таблицы с телеметрией
     */

    executeRequest(R"(
        DROP TABLE telemetry;
    )");
}

void DataBase::deleteModuleFromTables(long long module_id){

    /**
     * @brief Удаление модуля из БД
     * @param module_id id модуля, который необходимо удалить
     */
   
    // Удаление из таблицы модулей
    std::string sql = R"(
        DELETE FROM modules
        WHERE id = ?
    )";
    executeRequest(sql, {module_id});

}

void DataBase::deleteCapabilityFromTable(long long capability_id){
    
    /**
     * @brief Удаление возможности из БД
     * @param capability_id id возможности, которую необходимо удалить
     */
   
    // Удаление из таблицы возможностей
    std::string sql = R"(
        DELETE FROM capabilities
        WHERE id = ?
    )";
    executeRequest(sql, {capability_id});

}

void DataBase::unbindCapabilityInModule(long long module_id, long long capability_id){

    /**
     * @brief отвязка возможности у модуля
     * @param module_id id модуля, у которого необходимо отвязать возможность
     * @param capability_id id возможности, которую необходимо отвязать
     */
   
    // Удаление из таблицы возможностей
    std::string sql = R"(
        DELETE FROM modules_capabilities
        WHERE module_id = ? AND capability_id = ?
    )";
    executeRequest(sql, {module_id, capability_id});

}

void DataBase::updateModuleInfo(long long module_id, const std::string &name, const std::string &alias){

    /**
     * @brief Обновление полей модуля
     * @param module_id id модуля, который необходимо обновить
     * @param name имя, которое будет установлено
     * @param alias псевдоним, который будет установлен
     */    

    std::string sql = R"(
        UPDATE modules
        SET name = ?, alias = ?
        WHERE id = ?
    )";
    executeRequest(sql, {name, alias, module_id});
}

void DataBase::clearModulesTable()
{

    /**
     * @brief Очистка таблицы модулей с сохранением структуры столбцов
     */

    executeRequest(R"(
        DELETE FROM modules;
    )");
}

void DataBase::clearModulesAndCapabilities(){

    /**
     * @brief Очистка сводной таблицы модулей и их функционала с сохранением структуры столбцов
     */

    executeRequest(R"(
        DELETE FROM modules_capabilities;
    )");
    
}

void DataBase::clearCapabilitiesTable(){

    /**
     * @brief Очистка таблицы возможностей с сохранением структуры столбцов
     */

    executeRequest(R"(
        DELETE FROM capabilities;
    )");
}

void DataBase::clearTelemetryTable(){

    /**
     * @brief Очистка таблицы телеметрии с сохранением структуры столбцов
     */

    executeRequest(R"(
        DELETE FROM telemetry;
    )");
}

long long DataBase::addModule(const std::string& name, const std::string& alias, 
                              const std::string& mqtt_topic){

    /**
     * @brief Добавление модуля 
     * @param name имя модуля
     * @param alias псевдоним для добавляемого модуля, чтобы различать одинаковые по
     *              типу модули
     * @param mqtt_topic mqtt топик для обработки удаленных пользовательских команд
     */

    // Добавление в таблицу модулей
    std::string sql = R"(
        INSERT INTO modules(name, alias, mqtt_topic)
        VALUES (?, ?, ?)
    )";
    executeRequest(sql, {name, alias, mqtt_topic});

    // Получение id только что добавленного модуля
    sql = R"(
        SELECT 
            id
        FROM modules 
        ORDER BY id DESC 
        LIMIT 1;
    )";
    QueryResult response = executeQuery(sql);
    long long module_id = response.get<long long>(0, "id");

    return module_id;
}


void DataBase::addCapability(long long module_id, const std::string &name){

    /**
     * @brief Добавление устройства 
     * @param module_id id модуля, к которому подвязывается добавляемая возможность
     * @param name имя возможности
    */

    // Добавление в таблицу возможностей
    std::string sql = R"(
        INSERT INTO capabilities(name)
        VALUES (?)
    )";
    executeRequest(sql, {name});

    // Получение id только что добавленной возможности
    sql = R"(
        SELECT 
            id
        FROM capabilities 
        ORDER BY id DESC 
        LIMIT 1;
    )";
    QueryResult response = executeQuery(sql);
    long long capability_id = response.get<long long>(0, "id");

    // Добавление в сводную таблицу модули-возможности
    sql = R"(
        INSERT INTO modules_capabilities(module_id, capability_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {module_id, capability_id});  
}

void DataBase::addTelemetry(long long module_id, const std::string &param_name, 
                            double param_value, int timestamp){
          
    /**
     * @brief Добавление телеметрии
     * @param module_id - id модуля 
     * @param param_name - название параметра
     * @param param_value - значение параметра
     * @param timestamp - временная метка замера показания датчика
    */

    // Добавление в таблицу с телеметрией
    std::string sql = R"(
        INSERT INTO telemetry(module_id, param_name, param_value, timestamp)
        VALUES (?, ?, ?, ?)
    )";
    executeRequest(sql, {module_id, param_name, param_value, timestamp});
}

std::vector<json> DataBase::getListOfModules()
{
    std::vector<json> list_of_modules;
    std::string sql = R"(
        SELECT 
            m.id as module_id,
            m.name as module_name,
            m.alias as module_alias,
            m.mqtt_topic as module_mqtt_topic,
        FROM modules m
    )";

    QueryResult response = executeQuery(sql);
    json module_;
    
    for (size_t i = 0; i < response.rows.size(); i++){
        module_["id"] = response.get<long long>(i, "module_id");
        module_["name"] = response.get<std::string>(i, "module_name");
        module_["alias"] = response.get<std::string>(i, "module_alias");
        module_["mqtt_topic"] = response.get<std::string>(i, "module_mqtt_topic");
        list_of_modules.push_back(module_);
    }
    return list_of_modules;
}

std::vector<json> DataBase::getCapabilities(long long module_id)
{
    std::vector<json> capabilities;
    
    std::string sql = R"(
        SELECT 
            c.id,
            c.name
        FROM modules_capabilities mc
        JOIN capabilities c ON mc.capability_id = c.id
        WHERE module_id = ?;
    )";

    QueryResult response = executeQuery(sql, {module_id});

    json cur_capability;

    for (int i = 0; i < response.size(); i++){
        cur_capability["id"] = response.get<long long>(i, "id");
        cur_capability["name"] = response.get<std::string>(i, "name");
        capabilities.push_back(cur_capability);
    }
    return capabilities;
}

json DataBase::getModuleInfo(long long module_id){

    std::string sql = R"(
        SELECT 
            m.name,
            m.alias,
            m.mqtt_topic,
        FROM modules m
        WHERE m.id = ?
    )";

    QueryResult response = executeQuery(sql, {module_id});
    json module_info;
    module_info["name"] = response.get<std::string>(0, "name");
    module_info["alias"] = response.get<std::string>(0, "alias");
    module_info["mqtt_topic"] = response.get<std::string>(0, "mqtt_topic");
    return module_info;
}

/// @brief Получение значений param_name параметра с module_id модуля за последние time_interval минут

std::vector<double> DataBase::getTelemtry(long long module_id, const std::string &param_name, int time_interval)
{
    std::vector<double> telemetry;

    int start_time = time(NULL) - time_interval * 60;

    std::string sql = R"(
        SELECT
            param_value
        FROM telemetry
        WHERE module_id = ? AND param_name = ? AND timestamp >= ?
    )";

    QueryResult response = executeQuery(sql, {module_id, param_name, time_interval});

    for (int i = 0; i < response.size(); i++){
        telemetry.push_back(response.get<double>(i, "param_value"));
    }
    
    return telemetry;
}
