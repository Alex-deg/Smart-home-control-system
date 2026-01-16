#pragma once

#include <sqlite3.h>
#include <iostream>
#include <vector>
#include <string>

class DataBaseException : public std::runtime_error{
public:
    DataBaseException(const std::string &msg) : std::runtime_error(msg) {}
};

class DataBase{

private:
    sqlite3 *db = nullptr;

    void close();

public:
    
    DataBase() = default;
    explicit DataBase(const std::string &path_to_database);
    ~DataBase();

    void open(const std::string &path_to_database);
    bool isOpen() const;

    void execute_request(const std::string &sql);
    void execute_request(const std::string &sql,
                         const std::vector<std::string>& params);
    
    void commit();
    void rollback();

};