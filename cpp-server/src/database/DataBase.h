#pragma once

#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <iostream>
#include <variant>
#include <string>
#include <vector>

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

    void createTelemetryTable();
    void createScenariosTable();
    void createModuleParamsTable();
    void createScenariosActsTable();

    void deleteTelemetryTable();
    void deleteScenariosTable();
    void deleteModuleParamsTable();
    void deleteScenariosActsTable();

    void clearTelemetryTable();
    void clearScenariosTable();
    void clearModuleParamsTable();
    void clearScenariosActsTable();

    void addTelemetry(long long module_id, const std::string& param_name,
                      double param_value, int timestamp, const std::string& meas_unit = "");
    void addModuleParams(long long module_id, double module_temp, int free_bytes,
                         int timestamp, bool anomaly = false);
    void addScenariosAct(long long scenario_id, long long act_id);
    long long addScenario(const std::string& name, const std::string& condition);

    std::vector<json> getTelemtry(long long module_id, const std::string& param_name,
                                  int time_interval);
    std::vector<json> getModuleParams(long long module_id, int time_interval, bool with_anomalies);
    std::vector<json> getUniqueModuleIDs();
    std::vector<long long> getScenariosActs(long long scenario_id);

    void anomalyTagging(std::vector<long long> record_ids);

    void deleteTableByName(const std::string& name);

};