#pragma once

#include "Client.h"
#include <vector>


class ClientCLI {
public:
    explicit ClientCLI(const std::string &directory);

    ~ClientCLI();

    void run(const char *serverIp, int port);


private:
    Client _client;
    std::thread _processingThread;
    bool _isRunning;

    void processMessages();
    int authenticateUser();
    std::string promptForUsername();
    void processUserCommands();
    void printMenu() const;
    void printInvalidCommand() const;
    bool handleSpecialCases(const std::string& input);
    void executeCommand(const std::vector<std::string>& commandParts);
    std::vector<std::string> parseInput(const std::string &input) const;
    static bool isValidAlnumString(const std::string &str);
};
