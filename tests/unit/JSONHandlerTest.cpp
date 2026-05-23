#include <gtest/gtest.h>
#include "../../cpp-server/include/JSONHandler.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {

        nlohmann::json test_config = {
            {"server", {
                {"ip", "192.168.0.105"},
                {"port", 8080},
                {"name", "main_server"}
            }},
            {"mqtt", {
                {"broker", "localhost"},
                {"port", 1883},
                {"keepalive", 60},
                {"topics", {
                    {"temperature", "sensors/temp"},
                    {"humidity", "sensors/hum"},
                    {"motion", "sensors/motion"}
                }}
            }},
            {"websocket", {
                {"url", "ws://127.0.0.1:8000/ws/bind_server/"},
                {"retry_count", 5},
                {"timeout", 30.5}
            }},
            {"database", {
                {"path", "../Data/smart_home.db"},
                {"backup_enabled", true}
            }},
            {"debug", true},
            {"log_level", "info"},
            {"modules", {"temperature", "humidity", "motion", "light"}},
            {"thresholds", {
                {"temp_min", 18.0},
                {"temp_max", 26.0},
                {"hum_min", 30.0},
                {"hum_max", 70.0}
            }}
        };
        
        std::ofstream file("test_config.json");
        file << test_config.dump(4);
        file.close();
        
        std::ofstream bad_file("bad_config.json");
        bad_file << "{invalid json content}";
        bad_file.close();
        
        config_.open("test_config.json");
    }
    
    void TearDown() override {
        config_ = JSONHandler();
        std::remove("test_config.json");
        std::remove("bad_config.json");
        std::remove("test_output.json");
    }
    
    JSONHandler config_;
};

TEST_F(ConfigTest, OpenExistingFile) {
    JSONHandler config;
    EXPECT_TRUE(config.open("test_config.json"));
}

TEST_F(ConfigTest, OpenNonExistentFile) {
    JSONHandler config;
    EXPECT_FALSE(config.open("non_existent.json"));
}

TEST_F(ConfigTest, OpenBrokenJsonFile) {
    JSONHandler config;
    EXPECT_FALSE(config.open("bad_config.json"));
}

TEST_F(ConfigTest, ReopenFile) {
    JSONHandler config;
    EXPECT_TRUE(config.open("test_config.json"));
    EXPECT_TRUE(config.open("test_config.json")); 
}

TEST_F(ConfigTest, GetStringValue) {
    std::string ip = config_.getValueByKey<std::string>("server.ip");
    EXPECT_EQ(ip, "192.168.0.105");
    
    std::string name = config_.getValueByKey<std::string>("server.name");
    EXPECT_EQ(name, "main_server");
    
    std::string ws_url = config_.getValueByKey<std::string>("websocket.url");
    EXPECT_EQ(ws_url, "ws://127.0.0.1:8000/ws/bind_server/");
}

TEST_F(ConfigTest, GetIntValue) {
    int port = config_.getValueByKey<int>("server.port");
    EXPECT_EQ(port, 8080);
    
    int mqtt_port = config_.getValueByKey<int>("mqtt.port");
    EXPECT_EQ(mqtt_port, 1883);
    
    int retry_count = config_.getValueByKey<int>("websocket.retry_count");
    EXPECT_EQ(retry_count, 5);
}

TEST_F(ConfigTest, GetDoubleValue) {
    double timeout = config_.getValueByKey<double>("websocket.timeout");
    EXPECT_DOUBLE_EQ(timeout, 30.5);
    
    double temp_min = config_.getValueByKey<double>("thresholds.temp_min");
    EXPECT_DOUBLE_EQ(temp_min, 18.0);
    
    double temp_max = config_.getValueByKey<double>("thresholds.temp_max");
    EXPECT_DOUBLE_EQ(temp_max, 26.0);
}

TEST_F(ConfigTest, GetBoolValue) {
    bool debug = config_.getValueByKey<bool>("debug");
    EXPECT_TRUE(debug);
    
    bool backup_enabled = config_.getValueByKey<bool>("database.backup_enabled");
    EXPECT_TRUE(backup_enabled);
}

TEST_F(ConfigTest, GetStringArray) {
    auto modules = config_.getValueByKey<std::vector<std::string>>("modules");
    EXPECT_EQ(modules.size(), 4);
    EXPECT_EQ(modules[0], "temperature");
    EXPECT_EQ(modules[1], "humidity");
    EXPECT_EQ(modules[2], "motion");
    EXPECT_EQ(modules[3], "light");
}

TEST_F(ConfigTest, GetNestedObject) {
    auto thresholds = config_.getValueByKey<json>("thresholds");
    EXPECT_TRUE(thresholds.contains("temp_min"));
    EXPECT_TRUE(thresholds.contains("temp_max"));
    EXPECT_EQ(thresholds["temp_min"], 18.0);
}

// Получение значения по умолчанию (ключ существует)
TEST_F(ConfigTest, GetValueWithDefaultKeyExists) {
    std::string ip = config_.getValueByKey<std::string>("server.ip", "127.0.0.1");
    EXPECT_EQ(ip, "192.168.0.105");
    
    int port = config_.getValueByKey<int>("server.port", 3000);
    EXPECT_EQ(port, 8080);
}

// Получение значения по умолчанию (ключ не существует)
TEST_F(ConfigTest, GetValueWithDefaultKeyMissing) {
    std::string none = config_.getValueByKey<std::string>("nonexistent.key", "default_value");
    EXPECT_EQ(none, "default_value");
    
    int missing_int = config_.getValueByKey<int>("nonexistent.int", 123);
    EXPECT_EQ(missing_int, 123);
    
    bool missing_bool = config_.getValueByKey<bool>("nonexistent.bool", true);
    EXPECT_TRUE(missing_bool);
    
    double missing_double = config_.getValueByKey<double>("nonexistent.double", 99.9);
    EXPECT_DOUBLE_EQ(missing_double, 99.9);
}

TEST_F(ConfigTest, GetValueWithPartialPath) {
    std::string none = config_.getValueByKey<std::string>("server.nonexistent", "default");
    EXPECT_EQ(none, "default");
}

TEST_F(ConfigTest, SetStringValue) {
    config_.setValueByKey("server.ip", std::string("192.168.1.100"));
    std::string ip = config_.getValueByKey<std::string>("server.ip");
    EXPECT_EQ(ip, "192.168.1.100");
}

TEST_F(ConfigTest, SetIntValue) {
    config_.setValueByKey("server.port", 9090);
    int port = config_.getValueByKey<int>("server.port");
    EXPECT_EQ(port, 9090);
}

TEST_F(ConfigTest, SetBoolValue) {
    config_.setValueByKey("debug", false);
    bool debug = config_.getValueByKey<bool>("debug");
    EXPECT_FALSE(debug);
}

TEST_F(ConfigTest, SetDoubleValue) {
    config_.setValueByKey("websocket.timeout", 60.0);
    double timeout = config_.getValueByKey<double>("websocket.timeout");
    EXPECT_DOUBLE_EQ(timeout, 60.0);
}

TEST_F(ConfigTest, SetValueCreatePath) {
    config_.setValueByKey("new_group.nested.value", 42);
    int value = config_.getValueByKey<int>("new_group.nested.value");
    EXPECT_EQ(value, 42);
}

TEST_F(ConfigTest, SetValueDeepNestedPath) {
    config_.setValueByKey("a.b.c.d.e.f.g.value", "deep");
    std::string value = config_.getValueByKey<std::string>("a.b.c.d.e.f.g.value");
    EXPECT_EQ(value, "deep");
}

TEST_F(ConfigTest, SaveToFile) {
    config_.setValueByKey("server.port", 9999);
    config_.saveJsonObjectToFile("test_output.json");
    
    JSONHandler new_config;
    EXPECT_TRUE(new_config.open("test_output.json"));
    int port = new_config.getValueByKey<int>("server.port");
    EXPECT_EQ(port, 9999);
}

TEST_F(ConfigTest, SaveToCurrentPath) {
    config_.setValueByKey("server.port", 7777);
    EXPECT_TRUE(config_.save());
    
    JSONHandler new_config;
    EXPECT_TRUE(new_config.open("test_config.json"));
    int port = new_config.getValueByKey<int>("server.port");
    EXPECT_EQ(port, 7777);
}

TEST_F(ConfigTest, SaveWithoutOpenFile) {
    JSONHandler empty_config;
    EXPECT_FALSE(empty_config.save());
}

TEST_F(ConfigTest, UpdateNestedObject) {
    nlohmann::json new_thresholds = {
        {"temp_min", 20.0},
        {"temp_max", 30.0},
        {"hum_min", 40.0},
        {"hum_max", 80.0},
        {"new_param", 100.0}
    };
    
    config_.setValueByKey("thresholds", new_thresholds);
    auto thresholds = config_.getValueByKey<json>("thresholds");
    EXPECT_EQ(thresholds["temp_min"], 20.0);
    EXPECT_EQ(thresholds["new_param"], 100.0);
}

TEST_F(ConfigTest, GetNonExistentKeyThrowsException) {
    JSONHandler config;
    config.open("test_config.json");
    
    EXPECT_THROW(config.getValueByKey<std::string>("nonexistent.key"), std::out_of_range);
}

TEST_F(ConfigTest, SetVectorValue) {
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    config_.setValueByKey("test.numbers", numbers);
    
    auto retrieved = config_.getValueByKey<std::vector<int>>("test.numbers");
    EXPECT_EQ(retrieved.size(), 5);
    EXPECT_EQ(retrieved[0], 1);
    EXPECT_EQ(retrieved[4], 5);
}

TEST_F(ConfigTest, NestedVectorValue) {
    std::vector<std::vector<int>> matrix = {{1, 2}, {3, 4}, {5, 6}};
    config_.setValueByKey("test.matrix", matrix);
    
    auto retrieved = config_.getValueByKey<std::vector<std::vector<int>>>("test.matrix");
    EXPECT_EQ(retrieved.size(), 3);
    EXPECT_EQ(retrieved[0][0], 1);
    EXPECT_EQ(retrieved[1][1], 4);
}

TEST_F(ConfigTest, SequentialOpen) {
    JSONHandler config;
    EXPECT_TRUE(config.open("test_config.json"));
    EXPECT_TRUE(config.getValueByKey<int>("server.port") == 8080);
    
    // Создаём второй файл
    nlohmann::json other_config = {{"key", "value"}};
    std::ofstream file("other_config.json");
    file << other_config.dump();
    file.close();
    
    EXPECT_TRUE(config.open("other_config.json"));
    EXPECT_EQ(config.getValueByKey<std::string>("key"), "value");
    
    std::remove("other_config.json");
}

TEST_F(ConfigTest, EmptyConfig) {
    JSONHandler empty_config;
    // При обращении без open должно выброситься исключение
    EXPECT_THROW(empty_config.getValueByKey<std::string>("any.key"), std::out_of_range);
}
