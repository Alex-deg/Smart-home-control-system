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

using json = nlohmann::json;
using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

typedef websocketpp::client<websocketpp::config::asio_client> client;

std::atomic<bool> g_running(true);

void signal_handler(int sig) {
    std::cout << "\nПолучен сигнал остановки..." << std::endl;
    g_running = false;
}

class RPiWebSocketClient {
public:
    RPiWebSocketClient(const std::string& token, const std::string& server_uri)
        : m_token(token), m_server_uri(server_uri), m_reconnect_delay(5), m_connected(false) {
        
        m_client.init_asio();
        
        m_client.set_open_handler(bind(&RPiWebSocketClient::on_open, this, ::_1));
        m_client.set_message_handler(bind(&RPiWebSocketClient::on_message, this, ::_1, ::_2));
        m_client.set_close_handler(bind(&RPiWebSocketClient::on_close, this, ::_1));
        m_client.set_fail_handler(bind(&RPiWebSocketClient::on_fail, this, ::_1));
    }
    
    void connect() {
        websocketpp::lib::error_code ec;
        auto con = m_client.get_connection(m_server_uri, ec);
        if (ec) {
            std::cerr << "Ошибка подключения: " << ec.message() << std::endl;
            schedule_reconnect();
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
            
            if (data.contains("echo")) {
                std::cout << "  (Эхо-ответ от сервера)" << std::endl;
            }
            else if (data.contains("status") && data["status"] == "ok") {
                std::cout << "  (Подтверждение от сервера)" << std::endl;
            }
            else if (data.contains("action")) {
                std::string action = data["action"];
                std::cout << "  (Команда: " << action << ")" << std::endl;
                
                if (action == "turn_on") {
                    std::string device = data.value("device", "");
                    std::cout << "🔌 Выполняем: включить " << device << std::endl;
                    bool success = execute_turn_on(device);
                    
                    json result = {
                        {"action", "command_result"},
                        {"command_id", data.value("command_id", 0)},
                        {"result", success ? "ok" : "fail"}
                    };
                    send(result.dump());
                }
                else if (action == "turn_off") {
                    std::string device = data.value("device", "");
                    std::cout << "🔌 Выполняем: выключить " << device << std::endl;
                    bool success = execute_turn_off(device);
                    
                    json result = {
                        {"action", "command_result"},
                        {"command_id", data.value("command_id", 0)},
                        {"result", success ? "ok" : "fail"}
                    };
                    send(result.dump());
                }
                else if (action == "ping") {
                    json pong = {{"action", "pong"}};
                    send(pong.dump());
                }
            }
        } catch (const json::parse_error& e) {
            std::cout << "  (Обычное текстовое сообщение)" << std::endl;
        }
    }
    
    void on_close(connection_hdl hdl) {
        std::cout << "[-] Соединение закрыто. Переподключение через " 
                  << m_reconnect_delay << " секунд..." << std::endl;
        m_connected = false;
        schedule_reconnect();
    }
    
    void on_fail(connection_hdl hdl) {
        std::cout << "[!] Ошибка соединения. Переподключение через " 
                  << m_reconnect_delay << " секунд..." << std::endl;
        m_connected = false;
        schedule_reconnect();
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
    
    void schedule_reconnect() {
        if (!g_running) return;
        
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(m_reconnect_delay));
            if (m_reconnect_delay < 60) m_reconnect_delay *= 2;
            
            if (g_running) {
                m_client.reset();
                connect();
            }
        }).detach();
    }
    
    bool execute_turn_on(const std::string& device) {
        std::cout << "  -> Включение " << device << std::endl;
        return true;
    }
    
    bool execute_turn_off(const std::string& device) {
        std::cout << "  -> Выключение " << device << std::endl;
        return true;
    }
    
private:
    client m_client;
    std::string m_token;
    std::string m_server_uri;
    connection_hdl m_hdl;
    int m_reconnect_delay;
    std::atomic<bool> m_connected;
};

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::string server_id;
    
    std::cout << "=== WebSocket клиент для тестирования ===" << std::endl;
    std::cout << "Введите server_id (или нажмите Enter для использования demo): ";
    std::getline(std::cin, server_id);
    
    if (server_id.empty()) {
        server_id = "550e8400-e29b-41d4-a716-446655440000";
        std::cout << "Используем demo server_id: " << server_id << std::endl;
    }
    
    std::string server_uri = "ws://127.0.0.1:8000/ws/" + server_id;
    std::cout << "Подключение к: " << server_uri << std::endl;
    
    auto client = std::make_unique<RPiWebSocketClient>("", server_uri);
    
    std::thread ws_thread([&]() {
        client->connect();
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\n=== Управление ===" << std::endl;
    std::cout << "  • 'msg <текст>' - отправить произвольное сообщение" << std::endl;
    std::cout << "  • 'turn_on <устройство>' - отправить команду включения" << std::endl;
    std::cout << "  • 'turn_off <устройство>' - отправить команду выключения" << std::endl;
    std::cout << "  • 'ping' - отправить ping" << std::endl;
    std::cout << "  • 'quit' - выход" << std::endl;
    std::cout << "==========================================\n" << std::endl;
    
    std::string input;
    while (g_running) {
        std::getline(std::cin, input);
        
        if (input == "quit" || input == "q") {
            g_running = false;
            break;
        }
        
        if (!client->is_connected()) {
            std::cout << "Соединение не установлено. Подождите..." << std::endl;
            continue;
        }
        
        if (input.substr(0, 4) == "msg ") {
            std::string message = input.substr(4);
            client->send_message(message);
        }
        else if (input.substr(0, 8) == "turn_on ") {
            std::string device = input.substr(8);
            json command = {
                {"action", "turn_on"},
                {"device", device},
                {"command_id", 123}
            };
            client->send_message(command.dump());
        }
        else if (input.substr(0, 9) == "turn_off ") {
            std::string device = input.substr(9);
            json command = {
                {"action", "turn_off"},
                {"device", device},
                {"command_id", 124}
            };
            client->send_message(command.dump());
        }
        else if (input == "ping") {
            json ping = {{"action", "ping"}};
            client->send_message(ping.dump());
        }
        else if (input.empty()) {
            continue;
        }
        else {
            std::cout << "Неизвестная команда. Доступные команды:" << std::endl;
            std::cout << "  msg <текст>, turn_on <устройство>, turn_off <устройство>, ping, quit" << std::endl;
        }
    }
    
    std::cout << "Останавливаем клиент..." << std::endl;
    client->stop();
    if (ws_thread.joinable()) {
        ws_thread.join();
    }
    
    std::cout << "Клиент остановлен." << std::endl;
    return 0;
}