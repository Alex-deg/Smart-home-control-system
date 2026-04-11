#pragma once

#include <sqlite3.h>
#include <iostream>
#include <vector>
#include <string>
#include <variant>
#include <nlohmann/json.hpp>

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
    


    /////////////////СОЗДАНИЕ ТАБЛИЦ ОСНОВНЫХ СУЩНОСТЕЙ И ИХ ОТНОШЕНИЙ/////////////////  
    void createModulesTable();
    void createModuleTypesTable();
    void createModuleTypesAndCapabilitiesTable();
    void createCapabilitiesTable();
    ///////////////////////////////////////////////////////////////////////////////////



    //////////////////////////////////УДАЛЕНИЕ ТАБЛИЦ//////////////////////////////////
    void deleteModuleTypesTable();
    void deleteModulesTable();
    void deleteModuleTypesAndCapabilities();
    void deleteCapabilitiesTable();
    ///////////////////////////////////////////////////////////////////////////////////



    ////////////////////////////УДАЛЕНИЕ КОНКРЕТНЫХ ЗАПИСЕЙ////////////////////////////
    void deleteModuleFromTables(long long module_id);
    ///////////////////////////////////////////////////////////////////////////////////



    //////////////////////////////////ОЧИСТКА ТАБЛИЦ///////////////////////////////////;
    void clearModuleTypesTable();          
    void clearModulesTable();              
    void clearModuleTypesAndCapabilities();
    void clearCapabilitiesTable();
    ///////////////////////////////////////////////////////////////////////////////////



    ///////////////////////////ЗАПОЛНЕНИЕ ТАБЛИЦ ЗНАЧЕНИЯМИ////////////////////////////
    long long addModule(long long module_type_id, const std::string& alias, 
                        const std::string& mqtt_topic);
    void addCapability(long long module_type_id, const std::string& name);
    void addModuleType(const std::string& name, const std::string& description);
    ///////////////////////////////////////////////////////////////////////////////////



    ////////////////////////////ПОЛУЧЕНИЕ ДАННЫХ ИЗ ТАБЛИЦ/////////////////////////////
    std::vector<json> getListOfModules();
    std::vector<json> getListOfModuleTypes();
    std::vector<json> getCapabilities(long long module_id);
    ///////////////////////////////////////////////////////////////////////////////////

};