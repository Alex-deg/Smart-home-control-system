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
    void deleteUsersTable();
    void clearDeviceTypesTable();
    void addUser(const std::string& user_name, const std::string& password,
                       long int tg_chat_id = 1, const std::string& role="user",
                       const std::string& status="active");
    void addDeviceType(const std::string &name, const std::string &description, const json &config);
    void addDevice(const std::string &name, int device_type_id, const std::string &mqtt_topic,
                    const std::string &location="kitchen", bool status=true); 
    std::vector<json> getListOfDevices();
    bool checkUserAuthentication(const std::string &username, const std::string &password);
};