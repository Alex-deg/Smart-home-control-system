#pragma once

#include "ScenarioHandler.hpp"
#include "DataBase.hpp"
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <signal.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>

using json = nlohmann::json;
using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

typedef websocketpp::client<websocketpp::config::asio_client> client;

class WebSocketClient {
public:
    using CommandCallback = std::function<void(const std::string& mqtt_topic,
                                               const std::string& message,
                                               int qos)>;
    WebSocketClient(const std::string& token, const std::string& server_url, DataBase &_db, ScenarioHandler &_sh);
    void stop();
    void connect();
    bool is_connected() const;
    void send_message(const std::string& message);
    void set_on_command(CommandCallback cb);
private:
    void send(const std::string& message);
    void on_open(connection_hdl hdl);
    void on_fail(connection_hdl hdl);
    void on_close(connection_hdl hdl);
    void on_message(connection_hdl hdl, client::message_ptr msg);
    // void schedule_reconnect();
private:
    ScenarioHandler& scenarioHandler;
    client m_client;
    std::string m_token;
    std::string m_server_url;
    connection_hdl m_hdl;
    int m_reconnect_delay;
    std::atomic<bool> m_connected;
    CommandCallback m_mqtt_publish;
    DataBase& db;
};