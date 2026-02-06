#pragma once

#include <sqlite3.h>
#include <iostream>
#include <vector>
#include <string>
#include <variant>
#include <nlohmann/json.hpp>
//#include "../entities.h"

using json = nlohmann::json;
using SQLValue = std::variant<std::string, int, double, long long>;

class DataBaseException : public std::runtime_error{
public:
    DataBaseException(const std::string &msg) : std::runtime_error(msg) {}
};

class QueryResult {
public:
    std::vector<std::vector<SQLValue>> rows;
    std::vector<std::string> columnNames;
    
    bool empty() const;
    size_t size() const;
    
    template<typename T>
    T get(size_t row, size_t col) const;
    template<typename T>
    T get(size_t row, const std::string& columnName) const;
};

class IDataBase{

private:
    sqlite3 *db = nullptr;
    void close();

protected:
    void executeRequest(const std::string &sql);
    void executeRequest(const std::string& sql, 
                        const std::vector<SQLValue>& params);
    QueryResult executeQuery(const std::string& sql,
                             const std::vector<SQLValue>& params = {});
public:
    IDataBase() = default;
    explicit IDataBase(const std::string &path_to_database);
    ~IDataBase();

    void open(const std::string &path_to_database);
    bool isOpen() const;

};

class DataBase : public IDataBase{
public:
    void commit();
    void rollback();
    
    void createUserTable();
    void createDeviceTable();
    void createDeviceTypeTable();
    void createTriggerTable();
    void createScenarioTable();
    void createServerTable();
    void createMQTTMessagesTable();
    void createModulesTable();
    void createUsersAndServersTable();
    void createServersAndModulesTable();
    void createModuleFillingTable(); // таблица описывающая функционал модуля 
                                     // в абстракции. Сводная таблица модулей и
                                     // типов устройств
    void createModulesAndDevicesTable(); // таблица в которой содержится отношение
                                         // модуль <=> конкретное устройство 
                                         // (с mqtt топиком для управления)

    void deleteDeviceTypesTable();
    void deleteDeviceTable();

    void deleteServerFromTable(long long server_id);
    void deleteModuleFromTable(long long record_id);
    
    void clearDeviceTypesTable();
    void clearModuleFillingTable();

    void updateServerName(long long server_id, const std::string& new_server_name);
    void updateDeviceType(int device_id, int device_type_id);
    void updateDeviceStatus(const std::string &payload, const std::string &topic_pattern);

    void addUser(const std::string& user_name, const std::string& password,
                       long int tg_chat_id = 1, const std::string& role="user",
                       const std::string& status="active");
    void addMQTTMessage(const std::string &topic, const std::string &payload,
                        bool incoming);
    void addDeviceType(const std::string &name, const std::string &role, 
                       const std::string &description, const json &config);
    void addDevice(const std::string &name, int device_type_id, const std::string &mqtt_topic,
                   const std::string &location="kitchen", bool status=true); 
    void addServer(long long user_id, const std::string& server_name, 
                   const std::string& server_id);
    void addModule(long long server_id, long long module_id);
    void addModulesDevicesRecord(long long module_id, long long device_id);
    
    void adminAddModule(const std::string& name, const std::string& description);
    void adminFillModules(long long module_id, long long device_type_id);

    std::vector<json> getListOfDevices();
    std::vector<json> getListOfServers(long long user_id);
    std::vector<json> getListOfModules(long long server_id);
    std::vector<json> getListOfAllModules();
    std::vector<json> getCapabilities(long long record_id);
    std::vector<std::string> getListOfNecessaryDevicesForModule(long long module_id);
    long long getModuleIDFromRecordID(long long record_id);

    long long getUserIDbyTGChatID(long long tg_chat_id);

    bool checkUserAuthentication(const std::string &username, const std::string &password);
    std::string getMQTTTopic(unsigned int id);
    //int getIDFromDeviceName(const std::string &device_name);
};