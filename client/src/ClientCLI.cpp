#include "ClientCLI.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <map>

ClientCLI::ClientCLI(const std::string &directory)
    : _client(directory), _isRunning(false),
      _processingThread(&ClientCLI::processMessages, this) {
}

ClientCLI::~ClientCLI() {
    _isRunning = false;
    _client.disconnect();
    if (_processingThread.joinable()) {
        _processingThread.join();
    }
}

void ClientCLI::run(const char *serverIp, const int port) {
    _isRunning = true;

    if (_client.connect(serverIp, port) != 0) {
        _isRunning = false;
        return;
    }

    if (authenticateUser() != 0) {
        _client.disconnect();
        _isRunning = false;
        return;
    }

    printMenu();
    processUserCommands();
}

void ClientCLI::processMessages() {
    while (_isRunning) {
        _client.processMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int ClientCLI::authenticateUser() {
    std::string username = promptForUsername();
    return _client.sendUsername(username);
}

std::string ClientCLI::promptForUsername() {
    std::string username;
    while (true) {
        std::cout << "Enter your username: ";
        std::getline(std::cin, username);

        if (!username.empty() && isValidAlnumString(username)) {
            break;
        }
        std::cout << "Invalid username. Only alphanumeric characters allowed." << std::endl;
    }
    return username;
}

void ClientCLI::processUserCommands() {
    std::string userInput;
    while (_isRunning && _client.isConnected()) {
        if (handleSpecialCases(userInput)) {
            continue;
        }

        std::cout << "\nEnter command: ";
        std::getline(std::cin, userInput);

        auto commandParts = parseInput(userInput);
        if (commandParts.empty()) {
            printInvalidCommand();
            continue;
        }

        executeCommand(commandParts);
    }
}

bool ClientCLI::handleSpecialCases(const std::string& input) {
    if (input.empty()) {
        printInvalidCommand();
        return true;
    }
    if (input == "EXIT") {
        _client.disconnect();
        _isRunning = false;
        return true;
    }
    return false;
}

void ClientCLI::executeCommand(const std::vector<std::string>& commandParts) {
    static const std::map<std::string, std::function<void()>> commands = {
        {"LIST", [this]() { _client.listRooms(); }},
        {"JOIN", [this, &commandParts]() {
            if (commandParts.size() == 2) _client.joinRoom(commandParts[1]);
            else printInvalidCommand();
        }},
        {"LEAVE", [this]() { _client.leaveRoom(); }},
        {"SEND", [this, &commandParts]() {
            if (commandParts.size() == 2) _client.sendMessage(commandParts[1]);
            else printInvalidCommand();
        }},
        {"SEND_FILE", [this, &commandParts]() {
            if (commandParts.size() == 2) _client.sendFile(commandParts[1]);
            else printInvalidCommand();
        }}
    };

    auto it = commands.find(commandParts[0]);
    if (it != commands.end()) {
        it->second();
    } else {
        printInvalidCommand();
    }
}


std::vector<std::string> ClientCLI::parseInput(const std::string &input) const {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;

    iss >> token;
    tokens.push_back(token);

    if (token == "SEND") {
        std::string message;
        std::getline(iss, message);
        if (!message.empty()) tokens.push_back(message.substr(1));
    } else {
        while (iss >> token) {
            tokens.push_back(token);
        }
    }

    return tokens;
}

void ClientCLI::printMenu() const {
    std::cout << "\n===== Available Commands =====\n"
              << "LIST               - List available rooms\n"
              << "JOIN <room>        - Join a chat room\n"
              << "LEAVE              - Leave current room\n"
              << "SEND <message>     - Send message\n"
              << "SEND_FILE <file>   - Send file\n"
              << "EXIT               - Disconnect\n"
              << "=============================\n";
}

void ClientCLI::printInvalidCommand() const {
    std::cout << "Invalid command. Type 'LIST', 'JOIN <room>', 'LEAVE', "
              << "'SEND <message>', 'SEND_FILE <file>', or 'EXIT'" << std::endl;
}

bool ClientCLI::isValidAlnumString(const std::string &str) {
    return std::all_of(str.begin(), str.end(), [](char c) {
        return isalnum(c);
    });
}
