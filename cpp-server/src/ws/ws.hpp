#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <memory>
#include <atomic>
#include <signal.h>

#include "../database/DataBase.h"
#include "../scenario-handler/ScenarioEngine.hpp"

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
    WebSocketClient(const std::string& token, const std::string& server_url, DataBase &_db, ScenarioEngine &_se);
    void connect();
    void send_message(const std::string& message);
    void stop();
    bool is_connected() const;
    void set_on_command(CommandCallback cb);
private:
    void on_open(connection_hdl hdl);
    void on_message(connection_hdl hdl, client::message_ptr msg);
    void on_close(connection_hdl hdl);
    void on_fail(connection_hdl hdl);
    void send(const std::string& message);
    // void schedule_reconnect();
private:
    ScenarioEngine& scenarioHandler;
    client m_client;
    std::string m_token;
    std::string m_server_url;
    connection_hdl m_hdl;
    int m_reconnect_delay;
    std::atomic<bool> m_connected;
    CommandCallback m_mqtt_publish;
    DataBase& db;
};