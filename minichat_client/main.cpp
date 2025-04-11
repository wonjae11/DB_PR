#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <string>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

void showInitialMenu() {
    std::cout << "==== MiniChat Client ====" << std::endl;
    std::cout << "1. Register\n2. Login\n3. Exit\n";
    std::cout << "Select option: ";
}

void showPostLoginMenu() {
    std::cout << "==== Chat Menu ====" << std::endl;
    std::cout << "1. Chat\n2. View Chat Log\n3. Logout\n4. Exit\n";
    std::cout << "Select option: ";
}

int main() {
    SetConsoleOutputCP(65001);  // UTF-8 콘솔 출력 설정

    WSADATA wsa;
    SOCKET sock;
    sockaddr_in serverAddr;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    char buffer[1024] = { 0 };
    int bytes = 0;

    bool running = true;
    bool loggedIn = false;
    int userId = -1;

    while (running) {
        if (!loggedIn) {
            showInitialMenu();
            int choice;
            std::cin >> choice;
            std::cin.ignore();

            std::string username, password;
            std::string message;

            switch (choice) {
            case 1:
                std::cout << "Username: ";
                std::getline(std::cin, username);
                std::cout << "Password: ";
                std::getline(std::cin, password);
                message = "REGISTER:" + username + ":" + password + ":";
                send(sock, message.c_str(), static_cast<int>(message.size()), 0);
                bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                buffer[bytes] = '\0';
                std::cout << "[Server] " << buffer;
                break;
            case 2:
                std::cout << "Username: ";
                std::getline(std::cin, username);
                std::cout << "Password: ";
                std::getline(std::cin, password);
                message = "LOGIN:" + username + ":" + password + ":";
                send(sock, message.c_str(), static_cast<int>(message.size()), 0);
                bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                buffer[bytes] = '\0';
                std::cout << "[Server] " << buffer;
                if (std::string(buffer).find("LOGIN_SUCCESS:") != std::string::npos) {
                    loggedIn = true;
                    userId = std::stoi(std::string(buffer).substr(14));
                }
                break;
            case 3:
                running = false;
                break;
            default:
                std::cout << "Invalid option!\n";
            }
        }
        else {
            showPostLoginMenu();
            int choice;
            std::cin >> choice;
            std::cin.ignore();

            std::string msg;

            switch (choice) {
            case 1:
                while (true) {
                    std::cout << "Message (type 'exit' to leave): ";
                    std::getline(std::cin, msg);
                    if (msg == "exit") break;

                    std::string fullMsg = "CHAT:" + std::to_string(userId) + ":" + msg;
                    send(sock, fullMsg.c_str(), static_cast<int>(fullMsg.size()), 0);
                    bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                    buffer[bytes] = '\0';
                    std::cout << "[Server] " << buffer;
                }
                break;
            case 2:
                msg = "VIEW_LOG:" + std::to_string(userId);
                send(sock, msg.c_str(), static_cast<int>(msg.size()), 0);
                bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                buffer[bytes] = '\0';
                std::cout << "[Chat Log]\n" << buffer;
                break;
            case 3:
                loggedIn = false;
                break;
            case 4:
                running = false;
                break;
            default:
                std::cout << "Invalid option!\n";
            }
        }
    }

    closesocket(sock);
    WSACleanup();
    std::cout << "Program terminated." << std::endl;
    return 0;
}
