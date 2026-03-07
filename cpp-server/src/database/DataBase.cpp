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



void DataBase::createUsersTable(){
    
    /**
     * @brief Создание таблицы для хранения пользователей
     * @details username - имя пользователя
     *          password - пароль пользователя
     *          telegram_chat_id - id чата пользователя в telegram
     *          status - статус пользователя (authorized/non_authorized : true/false)
    */  
       
    executeRequest(R"(
        CREATE TABLE users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL,
            password TEXT NOT NULL, 
            telegram_chat_id BIGINT UNIQUE
        );    
    )");
}

void DataBase::createUsersAndServersTable(){
   
    /**
     * @brief Создание сводной таблицы для описания отношения пользователей и серверов
     * @details user_id - id пользователя
     *          server_id - id сервера, которым владеет пользователь с данным user_id
     */

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

void DataBase::createModulesAndDevicesTable(){
    
    /**
     * @brief Создание сводной таблицы для хранения отношения модулей и устройств
     * @details module_id - id модуля
     *          device_id - id устройства, которое входит в модуль с данным module_id
     */

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

void DataBase::createModulesFillingTable(){
    
    /**
     * @brief Создание таблицы для хранения "начинки" модуля
     * @details В данной таблице содержится информация о том, какие типы устройств
     *          должны быть в модуле для его корреткного функционирования
     *          module_type_id - id типа модуля
     *          device_type_id - id типа устройства, которое должно входить в состав
     *          модуля с данным module_type_id
     *          count - количество устройств типа device_type_id входящих в тип модуля
     *          module_type_id
     */

    executeRequest(R"(
        CREATE TABLE modules_filling (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            module_type_id INTEGER NOT NULL,
            device_type_id INTEGER NOT NULL,
            count INTEGER NOT NULL, 
            FOREIGN KEY (device_type_id) REFERENCES device_types(id),
            FOREIGN KEY (module_type_id) REFERENCES module_types(id)
        );
    )");
}

void DataBase::createDeviceTypesTable()
{

    /**
     * @brief Создание таблицы для хранения типов устройств
     * @details Таблица может заполняться только разработчиком, так как она содержит
     *          абстрактные типы устройств, с которыми могут взаимодействовать 
     *          определенные в таблице module_types типы модулей
     *          
     *          name - имя типа устройства
     *          role - роль устройства (actuator - исполняющее устройство (выполняет инструкции),
     *                                  sensor - датчик (собирает информацию),
     *                                  aux - вспомогательные устройства (например МК, который
     *                                  и не исполняет инструкции, и не собирает данные, а 
     *                                  является неким мостом между actuator и сервером))
     *          description - описание типа устройства
     */

    executeRequest(R"(
        CREATE TABLE device_types (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,   
            role TEXT NOT NULL, 
            description TEXT            
        ); 
    )");
}

void DataBase::createDevicesTable(){
    
    /**
     * @brief Создание таблицы для хранения устройств
     * @details device_type_id - id типа устройства
     *          mqtt_topic - mqtt топик для связи с сервером
     *          alias - псевдоним, чтобы пользователь мог определять устройства
     *          одинакового типа в рамках одного модуля
     */

    executeRequest(R"(
        CREATE TABLE devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_type_id INTEGER NOT NULL,
            mqtt_topic TEXT UNIQUE NOT NULL, 
            alias TEXT NOT NULL,
            FOREIGN KEY (device_type_id) REFERENCES device_types(id)
        );   
    )");
}

void DataBase::createActionsAndDeviceTypesTable(){

    /**
     * @brief Создание сводной таблицы для хранения отношения действий и типов устройств
     * @details action_id - id действия
     *          device_type_id - id типа устройства, которое выполняет действие с данным action_id
     */

    executeRequest(R"(
        CREATE TABLE actions_device_types (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            action_id INTEGER NOT NULL,
            device_type_id INTEGER NOT NULL, 
            FOREIGN KEY (action_id) REFERENCES actions(id),
            FOREIGN KEY (device_type_id) REFERENCES device_types(id)
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

void DataBase::createActionsTable(){
    executeRequest(R"(
        CREATE TABLE actions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL
        );   
    )");
}

void DataBase::createCapabilitiesAndActionsTable(){
    executeRequest(R"(
        CREATE TABLE capabilities_actions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            capability_id INTEGER NOT NULL,
            action_id INTEGER NOT NULL,
            FOREIGN KEY (capability_id) REFERENCES capabilities(id),
            FOREIGN KEY (action_id) REFERENCES actions(id)
        );   
    )");
}

void DataBase::createTriggersTable(){
   
    /**
     * @brief Создание таблицы для хранения триггеров сценариев
     * @details name - название триггера
     *          description - описание триггера
     */

    // Добавить условие по которому определяется триггер ли наступившее событие или нет
    executeRequest(R"(
        CREATE TABLE triggers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL, 
            description TEXT
        ); 
    )");
}

void DataBase::createScenariosTable(){
    
    /**
     * @brief Создание таблицы для хранения сценариев
     * @details trigger_id - id триггера при наступлении которого запускается данный сценарий
     *          device_id - id устройства которое участвует в выполнении сценария
     */

    // Добавить действие, которое будет выполнять устройство device_id
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

void DataBase::createMQTTMessagesTable(){
    
    /**
     * @brief Создание таблицы с логом mqtt сообщений
     * @details topic - mqtt топик {от}куда отправилось сообщение
     *          payload - само сообщение
     *          direction - направление сообщения (от сервера, к серверу)
     *          created_at - время создания записи в таблице
     */

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



void DataBase::deleteUsersTable(){

    /**
     * @brief Удаление таблицы с пользователями
     */

    executeRequest(R"(
        DROP TABLE users;    
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

void DataBase::deleteDeviceTypesTable(){
    
    /**
     * @brief Удаление таблицы с типами устройств
     */

    std::string sql = R"(
        DROP TABLE device_types;
    )";
    executeRequest(sql);
}

void DataBase::deleteDevicesTable(){
    
    /**
     * @brief Удаление таблицы с устройствами
     */

    executeRequest(R"(
        DROP TABLE devices;    
    )");
}

void DataBase::deleteActionsAndDeviceTypes(){
    
    /**
     * @brief Удаление таблицы с действиями и привязанными к ним типами устройств
     */

    executeRequest(R"(
        DROP TABLE actions_device_types;    
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

void DataBase::deleteCapabilitiesAndActionsTable(){

    /**
     * @brief Удаление таблицы с возможностями и привязанными к ним действиями
     */

    executeRequest(R"(
        DROP TABLE capabilities_actions;    
    )");
}

void DataBase::deleteActionsTable(){

    /**
     * @brief Удаление таблицы с действиями
     */

    executeRequest(R"(
        DROP TABLE actions;    
    )");
}

void DataBase::deleteModulesCapabilities(){
    executeRequest(R"(
        DROP TABLE modules_capabilities;    
    )");
}

void DataBase::deleteUsersAndServersTable(){

    /**
     * @brief Удаление сводной таблицы с пользователями и привязанными к ним серверами
     */

    executeRequest(R"(
        DROP TABLE users_servers;    
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

void DataBase::deleteModulesAndDevicesTable(){

    /**
     * @brief Удаление сводной таблицы с модулями и привязанными к ним устройствами
     */

    executeRequest(R"(
        DROP TABLE modules_devices;    
    )");
}

void DataBase::deleteModulesFillingTable(){

    /**
     * @brief Удаление сводной таблицы с типами модулей и типами устройств
     */

    executeRequest(R"(
        DROP TABLE modules_filling;    
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

    // Удаление всех привязанных устройств
    QueryResult response = executeQuery(sql, {module_id});
    int device_id;
    for(int i = 0; i < response.size(); i++){
        device_id = response.get<long long>(i, "device_id");
        deleteDeviceFromTables(device_id);
    }   

}

void DataBase::deleteDeviceFromTables(long long device_id){

    /**
     * @brief Удаление устройств из БД
     * @param device_id id устройства, которое необходимо удалить
     */

    // Удаление из сводной таблицы модули-устройства
    std::string sql = R"(
        DELETE FROM modules_devices
        WHERE device_id = ?
    )";
    executeRequest(sql, {device_id});
    
    // Удаление из таблцы устройств
    sql = R"(
        DELETE FROM devices
        WHERE id = ?
    )";
    executeRequest(sql, {device_id});
}



void DataBase::clearDeviceTypesTable(){
    
    /**
     * @brief Очистка таблицы типов устройств с сохранением структуры столбцов
     */

    std::string sql = R"(
        DELETE FROM device_types;
    )";
    executeRequest(sql);
}

void DataBase::clearModuleFillingTable(){
    
    /**
     * @brief Очистка таблицы наполнения модулей с сохранением структуры столбцов
     */

    std::string sql = R"(
        DELETE FROM modules_filling
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

void DataBase::updateDeviceType(int device_id, int device_type_id)
{

    /**
     * @brief Обновление типа устройства
     * @param device_id id устройства, у которого собираемся менять тип устройства
     * @param device_type_id новый тип устройства
     */

    std::string sql = R"(
        UPDATE devices 
        SET device_type_id = ?
        WHERE id = ?
    )";
    executeRequest(sql, {device_type_id, device_id});
}

// void DataBase::updateDeviceStatus(bool new_status, const std::string &topic_pattern){
//     std::string sql = R"(
//         UPDATE devices 
//         SET status = ?
//         WHERE mqtt_topic LIKE ?
//     )";
//     executeRequest(sql, {new_status, topic_pattern});      
// }



void DataBase::addUser(const std::string& user_name, const std::string& password,
                       long int tg_chat_id){
    
    /**
     * @brief Добавление пользователя 
     * @param user_name Имя пользователя
     * @param password Пароль пользователя
     * @param tg_chat_id id чата в telegram
    */

    // Добавление в таблицу с пользователями
    std::string sql = R"(
        INSERT INTO users (username, password, telegram_chat_id)
        VALUES (?, ?, ?)
    )";
    executeRequest(sql, {
        user_name,       
        password,        
        tg_chat_id
    });
}

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

void DataBase::addModule(long long server_id, long long module_type_id, 
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
}

void DataBase::addDevice(long long module_id, int device_type_id, 
                         const std::string &mqtt_topic){
    
    /**
     * @brief Добавление устройства 
     * @param module_id id модуля, к которому подвязывается добавляемое устройство
     * @param device_type_id id типа устройства, к которому относится добавляемое устройство
     * @param mqtt_topic mqtt топик, по которому будет осуществляться взаимодействие с сервером
    */

    // Добавление в таблицу устройств
    std::string sql = R"(
        INSERT INTO devices (device_type_id, mqtt_topic)
        VALUES (?, ?)
    )";
    executeRequest(sql, {device_type_id, mqtt_topic});

    // Получение id только что добавленного устройства
    sql = R"(
        SELECT 
            id
        FROM devices 
        ORDER BY id DESC 
        LIMIT 1;
    )";
    QueryResult response = executeQuery(sql);
    long long device_id = response.get<long long>(0, "id");

    // Добавление в сводную таблицу модули-устройства
    sql = R"(
        INSERT INTO modules_devices (module_id, device_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {module_id, device_id});                
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

void DataBase::addMQTTMessage(const std::string &topic, const std::string &payload,
                              bool incoming){
    
    /**
     * @brief Добавление лога mqtt сообщения
     * @param topic mqtt топик {от}куда отправлялось сообщение
     * @param payload отправленное сообщение 
     * @param incoming показывает направление сообщения: входящее или выходящее
    */

    // Добавление в таблицу mqtt сообщений
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

void DataBase::fillModules(long long module_type_id, long long device_type_id,
                           int count){
    
    /**
     * @brief Добавление данных в сводную таблицу типы модулей-типы устройств
     * @param module_type_id id типа модуля
     * @param device_type_id id типа устройства, которое необходимо
     *                       для функционирования типа модуля module_type_id
    */

    std::string sql = R"(
        INSERT INTO modules_filling (module_type_id, device_type_id, count)
        VALUES (?, ?, ?)
    )";
    executeRequest(sql, {module_type_id, device_type_id, count});
}

void DataBase::adminAddDeviceType(const std::string &name, const std::string &role, 
                                  const std::string &description){

    /**
     * @brief Добавление типа устройства
     * @param name имя добавлемого типа устройства
     * @param role роль устройства (actuator - исполняющее устройство (выполняет инструкции),
     *                                  sensor - датчик (собирает информацию),
     *                                  aux - вспомогательные устройства (например МК, который
     *                                  и не исполняет инструкции, и не собирает данные, а 
     *                                  является неким мостом между actuator и сервером))
     * @param description описание типа устройства
    */

    // Добавление в таблицу с типами устройств
    std::string sql = R"(
        INSERT INTO device_types(name, role, description)
        VALUES (?, ?, ?)
    )";
    executeRequest(sql, {name, role, description});

}

void DataBase::adminAddAction(const std::string &name){
    
    /**
     * @brief Добавление действия
     * @param name имя добавлемого действия
    */

    std::string sql = R"(
        INSERT INTO actions(name)
        VALUES (?)
    )";
    executeRequest(sql, {name});
}

void DataBase::addCapabilitiesActions(long long capability_id, long long action_id){

    /**
     * @brief Добавление данных в сводную таблицу возможности - действия
     * @param capability_id id возможности
     * @param action_id id действия, которое входит в состав данной возможности
    */

    std::string sql = R"(
        INSERT INTO capabilities_actions(capability_id, action_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {capability_id, action_id});
}

void DataBase::adminAddActionsDeviceTypes(long long action_id, long long device_type_id){
    
    /**
     * @brief Добавление данных в сводную таблицу действия - типы устройств
     * @param action_id id действия
     * @param device_type_id id типа устройства, которое выполняет данное действие
    */

    std::string sql = R"(
        INSERT INTO actions_device_types(action_id, device_type_id)
        VALUES (?, ?)
    )";
    executeRequest(sql, {action_id, device_type_id});
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

std::vector<json> DataBase::getListOfAllModuleTypes()
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

std::vector<std::string> DataBase::getCapabilities(long long module_id)
{
    std::vector<std::string> capabilities;

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
            action as module_capability
        FROM modules_capabilities
        WHERE module_type_id = ?;
    )";

    response = executeQuery(sql, {module_type_id});
    for (int i = 0; i < response.size(); i++){
        auto capability = response.get<std::string>(i, "module_capability");
        capabilities.push_back(capability);
    }
    return capabilities;
}

std::vector<json> DataBase::getListOfNecessaryDevicesForModule(long long module_type_id)
{
    std::vector<json> necessary_devices;
    
    std::string sql = R"(
        SELECT 
            device_type_id,
            count
        FROM modules_filling
        WHERE module_type_id = ?
    )";
    QueryResult response = executeQuery(sql, {module_type_id});

    for (int i = 0; i < response.size(); i++){
        long long device_type_id = response.get<long long>(i, "device_type_id");
        long long count = response.get<long long>(i, "count");
        necessary_devices.push_back({device_type_id, count});
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

std::pair<bool, long long> DataBase::checkUserAuthentication(const std::string &username, const std::string &password)
{
    std::string sql = R"(
        SELECT 
            id,
            username,
            password 
        FROM users WHERE username = ?
    )";

    QueryResult response = executeQuery(sql, {username});

    if (response.size() == 0)
        return {false, -1};
    if (response.get<std::string>(0, "password") != password)
        return {false, -1};

    return {true, response.get<long long>(0, "id")};
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
