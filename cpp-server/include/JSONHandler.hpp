#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <fstream>
#include <string>

using json = nlohmann::json;

class JSONHandler {
public:

    bool open(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        try {
            file >> jsonObj_;
            current_path_ = path;
            return true;
        } catch (const json::parse_error& e) {
            return false;
        }
    }
    
    template<typename T>
    T getValueByKey(const std::string& key, const T& default_value = T{}) const {
        try {
            return getNestedValue(key).get<T>();
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
            return default_value;
        }
    }
    
    template<typename T>
    void setValueByKey(const std::string& key, const T& new_value) {
        setNestedValue(key, new_value);
    }
    
    bool saveJsonObjectToFile(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        file << jsonObj_.dump(4);
        return true;
    }

    bool save() const {
        if (current_path_.empty()) {
            return false;
        }
        return saveJsonObjectToFile(current_path_);
    }
          
private:
    json jsonObj_;
    std::string current_path_;
    
    json getNestedValue(const std::string& key) const {
        
        size_t pos = 0;
        size_t end;
        const json* current = &jsonObj_;
        
        while ((end = key.find('.', pos)) != std::string::npos) {
            std::string part = key.substr(pos, end - pos);
            if (!current->contains(part)) {
                throw std::out_of_range("Key not found: " + part);
            }
            current = &(*current)[part];
            pos = end + 1;
        }
        
        std::string last_part = key.substr(pos);
        if (!current->contains(last_part)) {
            throw std::out_of_range("Key not found: " + last_part);
        }
        
        return (*current)[last_part];
    }
    
    template<typename T>
    void setNestedValue(const std::string& key, const T& value) {
        size_t pos = 0;
        size_t end;
        json* current = &jsonObj_;
        
        while ((end = key.find('.', pos)) != std::string::npos) {
            std::string part = key.substr(pos, end - pos);
            if (!current->contains(part)) {
                (*current)[part] = json::object();
            }
            current = &(*current)[part];
            pos = end + 1;
        }
        
        std::string last_part = key.substr(pos);
        (*current)[last_part] = value;
    }
};
