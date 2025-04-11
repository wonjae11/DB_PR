#include <iostream>
#include <winsock2.h>
#include <mysql/jdbc.h>
#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;
    SOCKET serverSocket, clientSocket;
    sockaddr_in serverAddr, clientAddr;
    int addrSize = sizeof(clientAddr);

    sql::mysql::MySQL_Driver* driver;
    sql::Connection* conn;

    // 1. WinSock 초기화
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "WSAStartup failed" << std::endl;
        return 1;
    }

    // 2. 소켓 생성
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cout << "Socket creation failed" << std::endl;
        return 1;
    }

    // 3. 주소 정보 설정
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(9000);

    // 4. 바인딩
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "Bind failed" << std::endl;
        return 1;
    }

    // 5. 리슨 시작
    listen(serverSocket, SOMAXCONN);
    std::cout << "Server listening on port 9000..." << std::endl;

    // 6. 클라이언트 연결
    clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &addrSize);
    if (clientSocket == INVALID_SOCKET) {
        std::cout << "Accept failed" << std::endl;
        return 1;
    }

    std::cout << "Client connected!" << std::endl;

    // 7. MySQL 연결
    try {
        driver = sql::mysql::get_mysql_driver_instance();
        conn = driver->connect("tcp://127.0.0.1:3306", "root", "1234");
        conn->setSchema("minichat_db");

        // UTF-8 인코딩 설정
        std::unique_ptr<sql::Statement> charsetStmt(conn->createStatement());
        charsetStmt->execute("SET NAMES utf8mb4");
        std::cout << "MySQL connected with UTF-8." << std::endl;
    }
    catch (sql::SQLException& e) {
        std::cerr << "MySQL connection failed: " << e.what() << std::endl;
        return 1;
    }

    // 8. 메시지 수신 및 처리
    char buffer[1024] = { 0 };
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) break;

        buffer[bytesReceived] = '\0';
        std::string msg(buffer);
        std::cout << "Client: " << msg << std::endl;

        // REGISTER 처리
        if (msg.rfind("REGISTER:", 0) == 0) {
            size_t pos1 = msg.find(":", 9);
            size_t pos2 = msg.find(":", pos1 + 1);
            if (pos1 != std::string::npos && pos2 != std::string::npos) {
                std::string username = msg.substr(9, pos1 - 9);
                std::string password = msg.substr(pos1 + 1, pos2 - pos1 - 1);

                try {
                    std::unique_ptr<sql::PreparedStatement> stmt(
                        conn->prepareStatement("INSERT INTO users (username, password) VALUES (?, ?)")
                    );
                    stmt->setString(1, username);
                    stmt->setString(2, password);
                    stmt->executeUpdate();

                    const char* successMsg = "REGISTER_SUCCESS\n";
                    send(clientSocket, successMsg, strlen(successMsg), 0);
                }
                catch (...) {
                    const char* failMsg = "REGISTER_FAIL\n";
                    send(clientSocket, failMsg, strlen(failMsg), 0);
                }
            }
        }

        // LOGIN 처리
        else if (msg.rfind("LOGIN:", 0) == 0) {
            size_t pos1 = msg.find(":", 6);
            size_t pos2 = msg.find(":", pos1 + 1);
            if (pos1 != std::string::npos && pos2 != std::string::npos) {
                std::string username = msg.substr(6, pos1 - 6);
                std::string password = msg.substr(pos1 + 1, pos2 - pos1 - 1);

                try {
                    std::unique_ptr<sql::PreparedStatement> stmt(
                        conn->prepareStatement("SELECT user_id FROM users WHERE username = ? AND password = ?")
                    );
                    stmt->setString(1, username);
                    stmt->setString(2, password);
                    std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

                    if (res->next()) {
                        int user_id = res->getInt("user_id");

                        std::string loginSuccess = "LOGIN_SUCCESS:" + std::to_string(user_id) + "\n";
                        send(clientSocket, loginSuccess.c_str(), loginSuccess.size(), 0);
                    }
                    else {
                        send(clientSocket, "LOGIN_FAIL\n", 11, 0);
                    }
                }
                catch (...) {
                    send(clientSocket, "LOGIN_FAIL\n", 11, 0);
                }
            }
        }

        // 채팅 메시지 처리: CHAT:user_id:message
        else if (msg.rfind("CHAT:", 0) == 0) {
            size_t pos1 = msg.find(":", 5);
            if (pos1 != std::string::npos) {
                std::string userIdStr = msg.substr(5, pos1 - 5);
                std::string content = msg.substr(pos1 + 1);
                int user_id = std::stoi(userIdStr);

                try {
                    std::unique_ptr<sql::PreparedStatement> stmt(
                        conn->prepareStatement("INSERT INTO message_log (sender_id, content) VALUES (?, ?)")
                    );
                    stmt->setInt(1, user_id);
                    stmt->setString(2, content);
                    stmt->executeUpdate();

                    std::string echo = "[Echo] " + content + "\n";
                    send(clientSocket, echo.c_str(), static_cast<int>(echo.size()), 0);
                }
                catch (...) {
                    send(clientSocket, "CHAT_FAIL\n", 10, 0);
                }
            }
        }

        // 채팅 로그 조회 처리: VIEW_LOG:user_id
        else if (msg.rfind("VIEW_LOG:", 0) == 0) {
            std::string userIdStr = msg.substr(9);
            int user_id = std::stoi(userIdStr);
            try {
                std::unique_ptr<sql::PreparedStatement> stmt(
                    conn->prepareStatement("SELECT sent_at, content FROM message_log WHERE sender_id = ? ORDER BY sent_at DESC LIMIT 10")
                );
                stmt->setInt(1, user_id);
                std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
                std::string logs = "Chat Log:\n";
                while (res->next()) {
                    logs += res->getString("sent_at") + " - " + res->getString("content") + "\n";
                }
                send(clientSocket, logs.c_str(), logs.length(), 0);
            }
            catch (...) {
                send(clientSocket, "LOG_FETCH_FAIL\n", 15, 0);
            }
        }

        else {
            send(clientSocket, "INVALID_COMMAND\n", 17, 0);
        }
    }

    conn->close();
    delete conn;
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
