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
     * @details input_amperage - входной ток
     * @details input_voltage - входное напряжение
     * @details module_temp - температура модуля
     * @details timestamp - временная метка замера параметров модуля
     */

    executeRequest(R"(
        CREATE TABLE modules_params (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            module_id INTEGER NOT NULL,
            input_amperage TEXT NOT NULL,
            input_voltage REAL NOT NULL,
            module_temp TEXT,
            timestamp INTEGER NOT NULL
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

void DataBase::addModuleParams(long long module_id, double input_amperage, double input_voltage, double module_temp, int timestamp){

    /**
     * @brief Добавление параметров модуля для самодиагностики
     * @param module_id - id модуля 
     * @param input_amperage - входной ток
     * @param input_voltage - входное напряжение
     * @param module_temp - температура модуля
     * @param timestamp - временная метка замера параметров модуля
    */

    // Добавление в таблицу с телеметрией
    std::string sql = R"(
        INSERT INTO telemetry(module_id, input_amperage, input_voltage, module_temp, timestamp)
        VALUES (?, ?, ?, ?, ?)
    )";
    executeRequest(sql, {module_id, input_amperage, input_voltage, module_temp, timestamp});
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
std::vector<json> DataBase::getModuleParams(long long module_id, int time_interval)
{
    std::vector<json> moduleParams;

    long long start_time = time(NULL) - time_interval * 60 * 60;

    std::string sql = R"(
        SELECT
            *
        FROM modules_params
        WHERE module_id = ? AND timestamp >= ?
    )";
    QueryResult response = executeQuery(sql, {module_id, start_time});
    json curParams;
    for(int i = 0; i < response.size(); i++){
        curParams["input_amperage"] = response.get<double>(i, "input_amperage");
        curParams["input_voltage"] = response.get<double>(i, "input_voltage");
        curParams["module_temp"] = response.get<double>(i, "module_temp");
        curParams["timestamp"] = response.get<double>(i, "timestamp");
        moduleParams.push_back(curParams);
    }
    return moduleParams;
}

void DataBase::deleteTableByName(const std::string &name){
    std::string sql = "DROP TABLE " + name;
    executeRequest(sql);
}
