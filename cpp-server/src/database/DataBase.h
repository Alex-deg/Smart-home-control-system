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
    void createUsersTable();                

    void createUsersAndServersTable();      
    void createServersTable();              
    
    void createServersAndModulesTable();    

    void createModuleTypesTable();          
    void createModulesTable();              

    void createModulesAndDevicesTable(); // таблица в которой содержится отношение
                                         // конкретный модуль <=> конкретное устройство 
                                         // (с mqtt топиком для управления)

    void createModulesFillingTable(); // таблица описывающая функционал модуля 
                                      // в абстракции. Сводная таблица типов модулей и
                                      // типов устройств

    void createDeviceTypesTable();          
    void createDevicesTable();

    void createActionsAndDeviceTypesTable();

    void createModuleTypesAndCapabilitiesTable();

    void createCapabilitiesTable();
    
    void createCapabilitiesAndActionsTable();

    void createActionsTable();
    ///////////////////////////////////////////////////////////////////////////////////



    ////////////////////СОЗДАНИЕ ТАБЛИЦ ДЛЯ ФОРМИРОВАНИЯ СЦЕНАРИЕВ/////////////////////
    void createTriggersTable();
    void createScenariosTable();
    void createMQTTMessagesTable();
    ///////////////////////////////////////////////////////////////////////////////////
      


    //////////////////////////////////УДАЛЕНИЕ ТАБЛИЦ//////////////////////////////////
    void deleteUsersTable();                
    void deleteUsersAndServersTable();      
    void deleteServersTable();              
    void deleteServersAndModulesTable();    
    void deleteModuleTypesTable();          
    void deleteModulesTable();              
    void deleteModulesAndDevicesTable(); 
    void deleteModulesFillingTable(); 
    void deleteDeviceTypesTable();          
    void deleteDevicesTable();
    void deleteActionsAndDeviceTypes();
    void deleteModuleTypesAndCapabilities();
    void deleteCapabilitiesTable();
    void deleteCapabilitiesAndActionsTable();
    void deleteActionsTable();

    void deleteModulesCapabilities();
    ///////////////////////////////////////////////////////////////////////////////////



    ////////////////////////////УДАЛЕНИЕ КОНКРЕТНЫХ ЗАПИСЕЙ////////////////////////////
    void deleteServerFromTables(long long server_id);
    void deleteModuleFromTables(long long module_id);
    void deleteDeviceFromTables(long long device_id);
    ///////////////////////////////////////////////////////////////////////////////////



    //////////////////////////////////ОЧИСТКА ТАБЛИЦ///////////////////////////////////
    void clearDeviceTypesTable();
    void clearModuleFillingTable();
    ///////////////////////////////////////////////////////////////////////////////////



    //////////////////ИЗМЕНЕНИЕ КОНКРЕТНОГО ПОЛЯ В КОНКРЕТНОЙ ТАБЛИЦЕ//////////////////
    void updateServerName(long long server_id, const std::string& new_server_name);
    void updateDeviceType(int device_id, int device_type_id);
    // void updateDeviceStatus(const std::string &payload, const std::string &topic_pattern){
    ///////////////////////////////////////////////////////////////////////////////////



    ///////////////////////////ЗАПОЛНЕНИЕ ТАБЛИЦ ЗНАЧЕНИЯМИ////////////////////////////
    void addUser(const std::string& user_name, const std::string& password,
                       long int tg_chat_id = 1);
    void addServer(long long user_id, const std::string& server_name, 
                   const std::string& server_key);
    void addModule(long long server_id, long long module_type_id, 
                   const std::string& alias);
    void addDevice(long long module_id, int device_type_id, 
                   const std::string &mqtt_topic); 
    void addCapability(long long module_type_id, const std::string& name);
    void addModuleType(const std::string& name, const std::string& description,
                       long long creatorID);
    void fillModules(long long module_type_id, long long device_type_id);
    void addCapabilitiesActions(long long capability_id, long long action_id);
    void addMQTTMessage(const std::string &topic, const std::string &payload,
                        bool incoming);
    ///////////////////////////////////////////////////////////////////////////////////



    ////////////////////////////ПОЛУЧЕНИЕ ДАННЫХ ИЗ ТАБЛИЦ/////////////////////////////
    std::vector<json> getListOfServers(long long user_id);
    std::vector<json> getListOfDevices();
    std::vector<json> getListOfModules(long long server_id);
    std::vector<json> getListOfAllModuleTypes();
    std::vector<std::string> getCapabilities(long long module_id);   
    std::vector<std::string> getListOfNecessaryDevicesForModule(long long module_id);
    long long getModuleIDFromRecordID(long long record_id);
    long long getUserIDbyTGChatID(long long tg_chat_id);
    std::pair<bool, long long> checkUserAuthentication(const std::string &username, const std::string &password);
    std::string getMQTTTopic(unsigned int id);
    //int getIDFromDeviceName(const std::string &device_name);
    ///////////////////////////////////////////////////////////////////////////////////

    /////////////ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ДЛЯ ЗАПОЛНЕНИЯ АБСТРАКТНЫХ ТАБЛИЦ"/////////////
    
    void adminAddDeviceType(const std::string &name, const std::string &role, 
                            const std::string &description);
    void adminAddAction(const std::string& name);
    void adminAddActionsDeviceTypes(long long action_id, long long device_type_id);
    ///////////////////////////////////////////////////////////////////////////////////
};

// Возможно выделить все методы с префиксом admin в отдельный
// класс AdminDatabase унаследовавшись от IDataBase