#include "../include/DataBase.hpp"

bool QueryResult::empty() const { return rows.empty(); }
size_t QueryResult::size() const { return rows.size(); }

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
                    row.push_back(std::string()); 
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

bool IDataBase::isTableExists(const std::string &table_name)
{
    QueryResult response = executeQuery("SELECT * FROM sqlite_sequence WHERE name = ?", {table_name});
    return !response.empty();
}

void IDataBase::vacuum(){
    executeRequest("VACUUM");
}

bool IDataBase::isOpen() const
{
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



void DataBase::createTelemetryTable(){

    /**
     * @brief Создание таблицы для хранения показаний датчиков
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
            meas_unit TEXT,
            timestamp INTEGER NOT NULL
        );   
    )");
}

void DataBase::createModuleParamsTable(){

    /**
     * @brief Создание таблицы для хранения параметров модуля для самодиагностики
     * @details module_id - id модуля 
     * @details module_temp - температура модуля
     * @details free_bytes - свободное место на ESP32 (для контроля утечки памяти)
     * @details timestamp - временная метка замера параметров модуля
     */

    executeRequest(R"(
        CREATE TABLE modules_params (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            module_id INTEGER NOT NULL,
            module_temp REAL NOT NULL,
            free_bytes INTEGER NOT NULL,
            timestamp INTEGER NOT NULL,
            anomaly BOOLEAN NOT NULL
        );   
    )");
}

void DataBase::createScenariosTable(){

    /**
     * @brief Создание таблицы для хранения сценариев
     * @details name - имя сценария 
     * @details condition - условие выполнения сценария
     */

    executeRequest(R"(
        CREATE TABLE scenarios (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            condition TEXT NOT NULL
        );
    )");
}

void DataBase::createScenariosActsTable(){

    /**
     * @brief Создание таблицы для хранения сценариев
     * @details scenario_id - id сценария 
     * @details act_id - id из таблицы modules_capabilities (действие, которое будет выполняться
     *                                                       при истинности условия сценария)
     */

    executeRequest(R"(
        CREATE TABLE scenarios_acts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            scenario_id INTEGER NOT NULL,
            act_id INTEGER NOT NULL,
            FOREIGN KEY (scenario_id) REFERENCES scenarios(id)
        );
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

void DataBase::deleteModuleParamsTable(){

    /**
     * @brief Удаление таблицы с параметрами модуля
     */

    executeRequest(R"(
        DROP TABLE modules_params;
    )");
}

void DataBase::deleteScenariosTable(){

    /**
     * @brief Удаление таблицы со сценариями
     */

    executeRequest(R"(
        DROP TABLE scenarios;
    )");
}

void DataBase::deleteScenariosActsTable(){

    /**
     * @brief Удаление таблицы с действиями сценария
     */

    executeRequest(R"(
        DROP TABLE scenarios_acts;
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

void DataBase::clearModuleParamsTable(){

    /**
     * @brief Очистка таблицы с параметрами модулей с сохранением структуры столбцов
     */

    executeRequest(R"(
        DELETE FROM modules_params;
    )");
}

void DataBase::clearScenariosTable(){

    /**
     * @brief Очистка таблицы со сценариями с сохранением структуры столбцов
     */

    executeRequest(R"(
        DELETE FROM scenarios;
    )");
}

void DataBase::clearScenariosActsTable(){

    /**
     * @brief Очистка таблицы с действиями сценариев с сохранением структуры столбцов
     */

    executeRequest(R"(
        DELETE FROM scenarios_acts;
    )");
}

void DataBase::addTelemetry(long long module_id, const std::string &param_name, 
                            double param_value, int timestamp, const std::string& meas_unit){
          
    /**
     * @brief Добавление телеметрии
     * @param module_id - id модуля 
     * @param param_name - название параметра
     * @param param_value - значение параметра
     * @param timestamp - временная метка замера показания датчика
    */

    // Добавление в таблицу с телеметрией
    std::string sql = R"(
        INSERT INTO telemetry(module_id, param_name, param_value, meas_unit, timestamp)
        VALUES (?, ?, ?, ?, ?)
    )";
    executeRequest(sql, {module_id, param_name, param_value, meas_unit, timestamp});
    
}

void DataBase::addModuleParams(long long module_id, double module_temp, int free_bytes, 
                               int timestamp, bool anomaly){

    /**
     * @brief Добавление параметров модуля для самодиагностики
     * @param module_id   - id модуля 
     * @param module_temp - температура модуля
     * @param free_bytes  - свободное место на ESP32 (для контроля утечки памяти)
     * @param timestamp   - временная метка замера параметров модуля
     * @param anomaly     - метка аномальности
    */

    // Добавление в таблицу с телеметрией
    std::string sql = R"(
        INSERT INTO telemetry(module_id, module_temp, free_bytes, timestamp, anomaly)
        VALUES (?, ?, ?, ?, ?)
    )";
    executeRequest(sql, {module_id, module_temp, free_bytes, timestamp, anomaly});
}

long long DataBase::addScenario(const std::string &name, const std::string &condition){

    std::string sql = R"(
        INSERT INTO scenarios(name, condition)
        VALUES (?, ?)
    )";
    executeRequest(sql, {name, condition});
    sql = R"(
        SELECT 
            id
        FROM scenarios
        ORDER BY id DESC 
        LIMIT 1;
    )";
    QueryResult response = executeQuery(sql);
    return response.get<long long>(0, "id");
}

void DataBase::addScenariosAct(long long scenario_id, long long act_id){

    std::string sql = R"(
        INSERT INTO scenarios_acts(scenario_id, act_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {scenario_id, act_id});
}

/// @brief Получение значений param_name параметра с module_id модуля за последние time_interval МИНУТ
std::vector<json> DataBase::getTelemtry(long long module_id, const std::string &param_name, int time_interval)
{
    std::vector<json> telemetry;

    long long start_time = time(NULL) - time_interval * 60;

    std::string sql = R"(
        SELECT
            param_value,
            timestamp,
            meas_unit
        FROM telemetry
        WHERE module_id = ? AND param_name = ? AND timestamp >= ?
    )";

    json cur_tel;
    QueryResult response = executeQuery(sql, {module_id, param_name, start_time});

    for (int i = 0; i < response.size(); i++){
        cur_tel["value"] = response.get<double>(i, "param_value");
        cur_tel["timestamp"] = response.get<long long>(i, "timestamp");
        cur_tel["meas_unit"] = response.get<std::string>(i, "meas_unit");
        telemetry.push_back(cur_tel);
    }
    
    return telemetry;
}

/// @brief Получение параметров module_id модуля за последние time_interval ЧАСОВ
std::vector<json> DataBase::getModuleParams(long long module_id, int time_interval, bool with_anomalies)
{
    std::vector<json> moduleParams;

    long long start_time = time(NULL) - time_interval * 60 * 60;

    std::string sql = R"(
        SELECT
            *
        FROM modules_params
        WHERE module_id = ? AND timestamp >= ? AND anomaly <= ?
    )";
    QueryResult response = executeQuery(sql, {module_id, start_time, with_anomalies});
    json curParams;
    for(int i = 0; i < response.size(); i++){
        curParams["id"] = response.get<long long>(i, "id");
        curParams["module_temp"] = response.get<double>(i, "module_temp");
        curParams["free_bytes"] = response.get<double>(i, "free_bytes");
        curParams["timestamp"] = response.get<double>(i, "timestamp");
        moduleParams.push_back(curParams);
    }
    return moduleParams;
}

std::vector<long long> DataBase::getScenariosActs(long long scenario_id){
    
    std::vector<long long> actIDs;
    std::string sql = R"(
        SELECT
            act_id
        FROM scenarios_acts
        WHERE scenario_id = ?
    )";
    QueryResult response = executeQuery(sql, {scenario_id});
    for (int i = 0; i < response.size(); i++){
        actIDs.push_back(response.get<long long>(i, "act_id"));
    }
    return actIDs;
}

void DataBase::anomalyTagging(std::vector<long long> record_ids){
    std::string sql = R"(
        UPDATE modules_params
        SET anomaly = 1
        WHERE id = ?
    )";
    for (auto &&id : record_ids){
        executeRequest(sql, {id});
    }
}

std::vector<json> DataBase::getUniqueModuleIDs()
{
    std::vector<json> ids;
    std::string sql = R"(
        SELECT DISTINCT module_id FROM modules_params
    )";
    QueryResult response = executeQuery(sql);
    json curID;
    for (int i = 0; i < response.size(); i++){
        curID["id"] = response.get<long long>(i, "module_id");
        ids.push_back(ids);
    }
    return ids;
}

void DataBase::deleteTableByName(const std::string &name){
    std::string sql = "DROP TABLE " + name;
    executeRequest(sql);
}
