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

using json = nlohmann::json;
using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

typedef websocketpp::client<websocketpp::config::asio_client> client;

// std::atomic<bool> g_running(true);

// void signal_handler(int sig) {
//     std::cout << "\nПолучен сигнал остановки..." << std::endl;
//     g_running = false;
// }

class RPiWebSocketClient {
public:

    using CommandCallback = std::function<void(const std::string& mqtt_topic,
                                               const std::string& message,
                                               int qos)>;

    RPiWebSocketClient(const std::string& token, const std::string& server_url)
        : m_token(token), m_server_url(server_url), m_reconnect_delay(5), m_connected(false) {
        
        m_client.init_asio();
        
        m_client.set_open_handler(bind(&RPiWebSocketClient::on_open, this, ::_1));
        m_client.set_message_handler(bind(&RPiWebSocketClient::on_message, this, ::_1, ::_2));
        m_client.set_close_handler(bind(&RPiWebSocketClient::on_close, this, ::_1));
        m_client.set_fail_handler(bind(&RPiWebSocketClient::on_fail, this, ::_1));
    }
    
    void connect() {
        websocketpp::lib::error_code ec;
        auto con = m_client.get_connection(m_server_url + m_token, ec);
        if (ec) {
            std::cerr << "Ошибка подключения: " << ec.message() << std::endl;
            // schedule_reconnect();
            return;
        }
        m_client.connect(con);
        m_client.run();
    }
    
    void send_message(const std::string& message) {
        if (!m_hdl.expired() && m_connected) {
            websocketpp::lib::error_code ec;
            m_client.send(m_hdl, message, websocketpp::frame::opcode::text, ec);
            if (ec) {
                std::cerr << "Ошибка отправки: " << ec.message() << std::endl;
            } else {
                std::cout << "[->] Отправлено: " << message << std::endl;
            }
        } else {
            std::cerr << "Соединение не активно, отправка невозможна" << std::endl;
        }
    }
    
    void stop() {
        if (!m_hdl.expired()) {
            m_client.close(m_hdl, websocketpp::close::status::normal, "Client stopped");
        }
        m_client.stop();
    }
    
    bool is_connected() const { return m_connected; }
    
    void set_on_command(CommandCallback cb) {
        m_mqtt_publish = cb;
    }

private:
    void on_open(connection_hdl hdl) {
        std::cout << "[+] WebSocket соединение установлено" << std::endl;
        m_hdl = hdl;
        m_connected = true;
        
        json auth_msg = {{"type", "auth"}};
        send(auth_msg.dump());
        
        m_reconnect_delay = 5;
    }
    
    void on_message(connection_hdl hdl, client::message_ptr msg) {
        std::string payload = msg->get_payload();
        std::cout << "[<-] Получено: " << payload << std::endl;
        
        try {
            json data = json::parse(payload);
            if (data["type"] == "command")
                m_mqtt_publish(data["params"]["mqtt_topic"], data["params"]["payload"], 1);
        }
        catch (const json::parse_error& e) {
            std::cout << "  (Обычное текстовое сообщение)" << std::endl;
        }
    }
    
    void on_close(connection_hdl hdl) {
        std::cout << "[-] Соединение закрыто. Переподключение через " 
                  << m_reconnect_delay << " секунд..." << std::endl;
        m_connected = false;
        // schedule_reconnect();
    }
    
    void on_fail(connection_hdl hdl) {
        std::cout << "[!] Ошибка соединения. Переподключение через " 
                  << m_reconnect_delay << " секунд..." << std::endl;
        m_connected = false;
        // schedule_reconnect();
    }
    
    void send(const std::string& message) {
        if (!m_hdl.expired() && m_connected) {
            websocketpp::lib::error_code ec;
            m_client.send(m_hdl, message, websocketpp::frame::opcode::text, ec);
            if (ec) {
                std::cerr << "Ошибка отправки: " << ec.message() << std::endl;
            }
        }
    }
    
    // void schedule_reconnect() {
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
    
private:
    client m_client;
    std::string m_token;
    std::string m_server_url;
    connection_hdl m_hdl;
    int m_reconnect_delay;
    std::atomic<bool> m_connected;
    CommandCallback m_mqtt_publish;
};