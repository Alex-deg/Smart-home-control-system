#include "../../include/DataBase.h"

DataBase::DataBase(const std::string &path_to_database){
    open(path_to_database);
}

DataBase::~DataBase(){
    close();
}

void DataBase::open(const std::string& path_to_database){
    if (db)
        close();
    int db_session = sqlite3_open(path_to_database.c_str(), &db);
    if (db_session != SQLITE_OK){
        std::string err = sqlite3_errmsg(db);
        sqlite3_close(db);
        db = nullptr;
        throw DataBaseException("Cannot open database: " + err);
    }
    std::cout << "Database has been opened successfully!" << std::endl;
}

void DataBase::close(){
    if (db){
        sqlite3_close(db);
        db = nullptr;
    }
}

bool DataBase::isOpen() const {
    return db != nullptr;
}

void DataBase::execute_request(const std::string &sql){
    if (!db)
        throw DataBaseException("Database not opened");
    char* err_msg = nullptr;
    int response = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (response != SQLITE_OK){
        std::string err = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw DataBaseException("SQL error: " + err + "\nQuery: " + sql);
    }
}

void DataBase::execute_request(const std::string& sql, const std::vector<std::string>& params) {
    if (!db) throw DataBaseException("Database not opened");
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        throw DataBaseException("Failed to prepare statement: " + 
                               std::string(sqlite3_errmsg(db)));
    }
    // Привязка параметров
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        throw DataBaseException("Execution failed: " + err + "\nQuery: " + sql);
    }
    sqlite3_finalize(stmt);
}

void DataBase::commit(){
    execute_request("COMMIT");
}

void DataBase::rollback(){
    execute_request("ROLLBACK");
}

