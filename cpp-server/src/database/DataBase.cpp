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




void DataBase::createServersTable(){
    
    /** 
     * @brief Создание таблицы для хранения серверов
     * @details name - имя сервера
     *          server_key - ключ сервера, по которому пользователь
     *                       может подключиться к данному серверу
    */

    executeRequest(R"(
        CREATE TABLE servers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,      
            server_key TEXT UNIQUE NOT NULL   
        ); 
    )");
}

void DataBase::createServersAndModulesTable(){
    
    /**
     * @brief Создание сводной таблицы для описания отношения серверов и модулей
     * @details server_id - id сервера
     *          module_id - id модуля, который привязан к серверу с данным server_id
     */

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

void DataBase::createModuleTypesTable(){
    
    /**
     * @brief Создание таблицы для хранения типов модулей
     * @details Таблица может заполняться только разработчиком, так как она содержит
     *          абстрактные типы модулей, с которыми может взаимодействовать разработанная система
     *          
     *          name - имя модуля
     *          description - описание модуля
     *          creatorID - id пользователя, который создал данный тип модуля
     */

    executeRequest(R"(
        CREATE TABLE module_types (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            description TEXT NOT NULL,
            creatorID INTEGER NOT NULL
        ); 
    )");
}

void DataBase::createModulesTable(){
    
    /**
     * @brief Создание таблицы для хранения модулей
     * @details module_type_id - id типа модуля
     *          alias - псевдоним модуля
     *          Alias нужен для распознавания одинаковых модулей на одном сервере. Например,
     *          у пользователя 2 умные розетки в квартире и чтобы их различать он назначает
     *          alias, который при выводе списка модулей на сервере будет выводиться
     *          справа от названия модуля в квадратных скобках (Умная розетка [Кухня]
     *                                                          Умная розетка [Спальня])
     */

    executeRequest(R"(
        CREATE TABLE modules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            module_type_id INTEGER NOT NULL,
            alias TEXT NOT NULL,
            FOREIGN KEY (module_type_id) REFERENCES module_types(id)
        ); 
    )");
}




void DataBase::createModuleTypesAndCapabilitiesTable(){
    /**
     * @brief Создание сводной таблицы для хранения отношения действий и типов устройств
     * @details module_type_id - id типа модуля
     *          capability_id - id возможности, которая предоставляется модулем с данным module_type_id
     */

    executeRequest(R"(
        CREATE TABLE module_types_capabilities (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            module_type_id INTEGER NOT NULL,
            capability_id INTEGER NOT NULL, 
            FOREIGN KEY (module_type_id) REFERENCES module_types(id),
            FOREIGN KEY (capability_id) REFERENCES capabilities(id)
        );   
    )");
}

void DataBase::createCapabilitiesTable(){
    executeRequest(R"(
        CREATE TABLE capabilities (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL
        );   
    )");
}

void DataBase::deleteServersTable(){

    /**
     * @brief Удаление таблицы с серверами
     */

    executeRequest(R"(
        DROP TABLE servers;    
    )");
}

void DataBase::deleteModuleTypesTable(){

    /**
     * @brief Удаление таблицы с типами модулей
     */

    executeRequest(R"(
        DROP TABLE module_types;    
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

void DataBase::deleteModuleTypesAndCapabilities(){

    /**
     * @brief Удаление таблицы с типами модулей и привязанными к ним возможностями
     */

    executeRequest(R"(
        DROP TABLE module_types_capabilities;    
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

void DataBase::deleteServersAndModulesTable(){

    /**
     * @brief Удаление сводной таблицы с серверами и привязанными к ним модулями
     */

    executeRequest(R"(
        DROP TABLE servers_modules;    
    )");
}


void DataBase::deleteServerFromTables(long long server_id){

    /**
     * @brief Удаление сервера и всех к нему привязанных сущностей (модулей) из БД
     * @param server_id id сервера, который необходимо удалить
     */

    // Удаление из сводной таблицы пользователь-сервер
    std::string sql = R"(
        DELETE FROM users_servers
        WHERE server_id = ?
    )";
    executeRequest(sql, {server_id});
    
    // Удаление из таблицы модулей
    sql = R"(
        DELETE FROM servers
        WHERE id = ?
    )";
    executeRequest(sql, {server_id});

    // Получение id'шников всех модулей, которые привязаны к этому серверу
    sql = R"(
        SELECT
            module_id
        FROM servers_modules
        WHERE server_id = ?
    )";

    // Удаление всех привязанных модулей
    QueryResult response = executeQuery(sql, {server_id});
    long long module_id;
    for(int i = 0; i < response.size(); i++){
        module_id = response.get<long long>(i, "module_id");
        deleteModuleFromTables(module_id);
    }

}

void DataBase::deleteModuleFromTables(long long module_id){

    /**
     * @brief Удаление модуля и всех к нему привязанных сущностей (устройств) из БД
     * @param module_id id модуля, который необходимо удалить
     */

    // Удаление из сводной таблицы сервер-модуль
    std::string sql = R"(
        DELETE FROM servers_modules
        WHERE module_id = ?
    )";
    executeRequest(sql, {module_id});
    
    // Удаление из таблицы модулей
    sql = R"(
        DELETE FROM modules
        WHERE id = ?
    )";
    executeRequest(sql, {module_id});

    // Получение id'шников всех привязанных устройств
    sql = R"(
        SELECT
            device_id
        FROM modules_devices
        WHERE module_id = ?
    )";

}








void DataBase::clearServersAndModulesTable(){
    
    /**
     * @brief Очистка сводной таблицы серверов и модулей с сохранением структуры столбцов
     */

    std::string sql = R"(
        DELETE FROM servers_modules;
    )";
    executeRequest(sql);
}

void DataBase::clearModulesTable()
{

    /**
     * @brief Очистка таблицы модулей с сохранением структуры столбцов
     */

    std::string sql = R"(
        DELETE FROM modules;
    )";
    executeRequest(sql);
}



void DataBase::updateServerName(long long server_id, const std::string &new_server_name){
   
    /**
     * @brief Обновление имени сервера
     * @param server_id id сервера, у которого собираемся менять имя
     * @param new_server_name новое имя сервера
     */

    std::string sql = R"(
        UPDATE servers 
        SET name = ?
        WHERE id = ?
    )";
    executeRequest(sql, {new_server_name, server_id});
}


// void DataBase::updateDeviceStatus(bool new_status, const std::string &topic_pattern){
//     std::string sql = R"(
//         UPDATE devices 
//         SET status = ?
//         WHERE mqtt_topic LIKE ?
//     )";
//     executeRequest(sql, {new_status, topic_pattern});      
// }




void DataBase::addServer(long long user_id, const std::string &server_name, 
                         const std::string &server_key){
    
    /**
     * @brief Добавление сервера
     * @param user_id id пользователя, который добавляет сервер
     * @param server_name Имя создаваемого сервера
     * @param server_key Ключ сервера, по которому пользователи
     *                   могут подключаться к этому серверу
     */

    // Добавление в таблицу с серверами
    std::string sql = R"(
        INSERT INTO servers (name, server_key)
        VALUES (?, ?)
    )";
    executeRequest(sql, {server_name, server_key});

    // Получение id сервера, который только что добавили
    sql = R"(
        SELECT 
            id
        FROM servers 
        ORDER BY id DESC 
        LIMIT 1;
    )";
    QueryResult response = executeQuery(sql);
    long long server_id = response.get<long long>(0, "id");
    
    // Добавление сервера в сводную таблицу пользователи-сервера
    sql = R"(
        INSERT INTO users_servers (user_id, server_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {user_id, server_id});
}

long long DataBase::addModule(long long server_id, long long module_type_id, 
                         const std::string& alias){

    /**
     * @brief Добавление модуля 
     * @param server_id id сервера, к которому подвязывается добавляемый модуль
     * @param module_type_id id типа модуля, к которому относится добавляемый модуль
     * @param alias псевдоним для добавляемого модуля, чтобы различать одинаковые по
     *              типу модули
     */

    // Добавление в таблицу модулей
    std::string sql = R"(
        INSERT INTO modules(module_type_id, alias)
        VALUES (?, ?)
    )";
    executeRequest(sql, {module_type_id, alias});

    sql = R"(
        SELECT 
            id
        FROM modules 
        ORDER BY id DESC 
        LIMIT 1;
    )";
    QueryResult response = executeQuery(sql);
    long long module_id = response.get<long long>(0, "id");

    // Добавление в сводную таблицу серверы-модули
    sql = R"(
        INSERT INTO servers_modules(server_id, module_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {server_id, module_id});

    return module_id;
}


void DataBase::addCapability(long long module_type_id, const std::string &name){

    /**
     * @brief Добавление устройства 
     * @param module_type_id id типа модуля, к которому подвязывается добавляемая возможность
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

    // Добавление в сводную таблицу типы модулей-возможности
    sql = R"(
        INSERT INTO module_types_capabilities(module_type_id, capability_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {module_type_id, capability_id});  
}




void DataBase::addModuleType(const std::string &name, const std::string &description,
                             long long creatorID){

    /**
     * @brief Добавление типа модуля
     * @param name имя добавлемого типа модуля
     * @param description описание добавляемого типа модуля
     * @param creatorID id пользователя, который создал данный тип модуля
    */

    // Добавление в таблицу с типами модулей
    std::string sql = R"(
        INSERT INTO module_types(name, description, creatorID)
        VALUES (?, ?, ?)
    )";
    executeRequest(sql, {name, description, creatorID});
}


std::vector<json> DataBase::getListOfServers(long long user_id)
{

    /**
     * @brief Получение клиентом списка доступных серверов 
     * @param user_id id пользователя, список серверов которого
     *                мы хотим получить
     * @return список json объектов, которые описывают сервера
     */

    std::cout << "user_id = " << user_id << std::endl;

    std::vector<json> list_of_servers;
    std::string sql = R"(
        SELECT 
            s.id as server_id,
            s.name as server_name,
            s.server_key as server_key  
        FROM users_servers us
        JOIN servers s ON us.server_id = s.id
        WHERE user_id = ?;
    )";

    QueryResult response = executeQuery(sql, {user_id});
    json server;
    
    for (size_t i = 0; i < response.rows.size(); i++){
        server["server_id"] = response.get<long long>(i, "server_id");
        server["name"] = response.get<std::string>(i, "server_name");
        server["server_key"] = response.get<std::string>(i, "server_key");
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
            m.alias as module_alias,
            mt.name as module_name,
            mt.description as module_description 
        FROM servers_modules sm
        JOIN modules m ON sm.module_id = m.id
        JOIN module_types mt ON m.module_type_id = mt.id
        WHERE server_id = ?;
    )";

    QueryResult response = executeQuery(sql, {server_id});
    json module_;
    
    for (size_t i = 0; i < response.rows.size(); i++){
        module_["id"] = response.get<long long>(i, "module_id");
        module_["alias"] = response.get<std::string>(i, "module_alias");
        module_["name"] = response.get<std::string>(i, "module_name");
        module_["description"] = response.get<std::string>(i, "module_description");
        list_of_modules.push_back(module_);
    }
    return list_of_modules;
}

std::vector<json> DataBase::getListOfModuleTypes()
{
    std::vector<json> list_of_modules;
    std::string sql = R"(
        SELECT 
            *
        FROM module_types;
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

std::vector<json> DataBase::getCapabilities(long long module_id)
{
    std::vector<json> capabilities;

    std::string sql = R"(
        SELECT
            module_type_id
        FROM modules
        WHERE id = ?
    )";

    QueryResult response = executeQuery(sql, {module_id});
    long long module_type_id = response.get<long long>(0, "module_type_id");
    
    sql = R"(
        SELECT 
            c.id,
            c.name
        FROM module_types_capabilities mtc
        JOIN capabilities c ON mtc.capability_id = c.id
        WHERE module_type_id = ?;
    )";

    response = executeQuery(sql, {module_type_id});

    json cur_capability;

    for (int i = 0; i < response.size(); i++){
        cur_capability["capability_id"] = response.get<long long>(i, "id");
        cur_capability["capability_name"] = response.get<std::string>(i, "name");
        capabilities.push_back(cur_capability);
    }
    return capabilities;
}