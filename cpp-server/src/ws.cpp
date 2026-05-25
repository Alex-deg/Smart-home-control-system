#include "../include/ws.hpp"

WebSocketClient::WebSocketClient(const std::string& token, const std::string& server_url, 
                                 DataBase &_db, ScenarioHandler &_sh, std::shared_ptr<liblog::Logger> _logger, bool _debugFlag)
    : m_token(token), m_server_url(server_url), m_reconnect_delay(5), 
      m_connected(false), db(_db), scenarioHandler(_sh), logger(_logger), debugFlag(_debugFlag) {
    
    m_client.init_asio();
    logger->debug("WebSocketClient::WebSocketClient(): ASIO has been initialized");
    m_client.set_open_handler(bind(&WebSocketClient::on_open, this, ::_1));
    m_client.set_message_handler(bind(&WebSocketClient::on_message, this, ::_1, ::_2));
    m_client.set_close_handler(bind(&WebSocketClient::on_close, this, ::_1));
    m_client.set_fail_handler(bind(&WebSocketClient::on_fail, this, ::_1));
    logger->debug("WebSocketClient::WebSocketClient(): Callbacks have been set");
}

void WebSocketClient::connect() {
    websocketpp::lib::error_code ec;
    auto con = m_client.get_connection(m_server_url + m_token, ec);
    if (ec) {
        logger->error("WebSocketClient::connect(): WebSocketClient::connect(): Connect error: " + ec.message());
        // schedule_reconnect();
        return;
    }
    logger->info("WebSocketClient::connect(): Connection establishing was successful");
    m_client.connect(con);
    m_client.run();
}

void WebSocketClient::send_message(const std::string& message) {
    if (!m_hdl.expired() && m_connected) {
        websocketpp::lib::error_code ec;
        m_client.send(m_hdl, message, websocketpp::frame::opcode::text, ec);
        if (ec) {
            logger->error("WebSocketClient::send_message(): Sending error: " + ec.message());
        } else {
            logger->info("WebSocketClient::send_message(): [=>] Sends: " + message);
        }
    } else {
        logger->error("WebSocketClient::send_message(): Connection isn't active, sending impossible");
    }
}

void WebSocketClient::stop() {
    if (!m_hdl.expired()) {
        m_client.close(m_hdl, websocketpp::close::status::normal, "Client stopped");
    }
    m_client.stop();
}

bool WebSocketClient::is_connected() const { return m_connected; }

void WebSocketClient::set_on_command(CommandCallback cb) {
    m_mqtt_publish = cb;
}

void WebSocketClient::on_open(connection_hdl hdl) {
    logger->info("WebSocketClient::on_open(): [+] WebSocket соединение установлено");
    m_hdl = hdl;
    m_connected = true;
    
    json auth_msg = {{"type", "auth"}};
    send(auth_msg.dump());
    
    m_reconnect_delay = 5;
}

void WebSocketClient::on_message(connection_hdl hdl, client::message_ptr msg) {
    std::string payload = msg->get_payload();
    logger->info("WebSocketClient::on_message(): [<-] Recieved: " + payload);
    
    try {
        json data = json::parse(payload);
        if(data["type"] == "command"){
            json payload;
            payload["request_id"] = data["request_id"];
            payload["payload"] = data["params"]["payload"];
            m_mqtt_publish(data["params"]["mqtt_topic"], payload.dump(), 1);
        }
        if(data["type"] == "scenario"){
            long long scenarioID;
            try{
                scenarioID = db.addScenario(data["scenario"]["name"], data["scenario"]["condition"]);
                std::vector<long long> actIDs = data["scenario"]["acts"];
                for (auto actID : actIDs){
                    db.addScenariosAct(scenarioID, actID);
                }
                scenarioHandler.addScenario(scenarioID, data["scenario"]["condition"]);
                logger->info("WebSocketClient::on_message(): Scenario has been added into database and ScenarioHandler successfully");
            }
            catch(std::runtime_error &e){
                logger->error("WebSocketClient::on_message(): Error occured while adding scenario: " + e.what());
            }
        }
    }
    catch (const json::parse_error& e) {
        logger->warning("WebSocketClient::on_message(): Not allowed message has been received from remote server");
    }
}

void WebSocketClient::on_close(connection_hdl hdl) {
    logger->warning("WebSocketClient::on_close(): [-] Connection closed. Reconnect after " 
                + std::to_string(m_reconnect_delay) + " seconds...");
    m_connected = false;
    // schedule_reconnect();
}

void WebSocketClient::on_fail(connection_hdl hdl) {
    logger->error("WebSocketClient::on_fail(): [!] Connection error. Reconnect after " 
                + std::to_string(m_reconnect_delay) + " seconds...");
    m_connected = false;
    // schedule_reconnect();
}

void WebSocketClient::send(const std::string& message) {
    if (!m_hdl.expired() && m_connected) {
        websocketpp::lib::error_code ec;
        m_client.send(m_hdl, message, websocketpp::frame::opcode::text, ec);
        if (ec) {
            logger->error("WebSocketClient::send(): Sending error: " + ec.message());
        }
    }
}

// void WebSocketClient::schedule_reconnect() {
//     if (!g_running) return;
    
//     std::thread([this]() {
//         std::this_thread::sleep_for(std::chrono::seconds(m_reconnect_delay));
//         if (m_reconnect_delay < 60) m_reconnect_delay *= 2;
        
//         if (g_running) {
//             m_client.reset();
//             connect();
//         }
//     }).detach();
// }
