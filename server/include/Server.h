#pragma once

#include <iostream>

#include "ThreadPool.h"
#include "Socket.h"
#include "Room.h"


enum class ReceiveStatus {
    SUCCESS,
    TIMEOUT,
    CLIENT_DISCONNECTED,
    ERROR
};

struct ReceiveResult {
    ReceiveStatus status;
    std::string message;
    ssize_t bytesReceived;
};


class Server {
public:
    explicit Server(const std::string &directory, size_t maxSimultaneousClients);

    void start(int port);
    void shutdown();

    void handleListRooms(const AuthenticatedClient &client);
    void handleJoin(AuthenticatedClient &client, const std::string &roomName);
    void handleLeave(AuthenticatedClient &client);
    void handleSend(const AuthenticatedClient &client, const std::string &message);

    ~Server();

    void broadcastMessage(const std::vector<AuthenticatedClient> &members, const std::string &message,
                          const AuthenticatedClient &sender, TlvType type);

private:
    Socket _serverSocket;
    const std::string _directory;
    std::unordered_map<std::string, Room> _rooms;

    ThreadPool _threadPool;
    size_t _maxSimultaneousClients;
    std::atomic<bool> _stopFlag{false};
    std::mutex _roomMutex;
    std::condition_variable _requestCv;
    std::condition_variable _uploadCv;
    std::mutex _uploadMutex;

    void run();
    Socket acceptClient() const;
    void defineVersionAndHandleClient(Socket clientSocket);

    void handleClient1dot0(Socket &clientSocket);

    static bool authenticateClient(const Socket &clientSocket, std::string &username);
    void processCommands(AuthenticatedClient &client);
    void cleanupClient(Socket &clientSocket, const char* username = nullptr, const char* roomName = nullptr);

    static ReceiveResult receiveMessage(const Socket &clientSocket, TlvType &tag, std::string &value, const char *username = nullptr);

    static bool isValidAlnumString(const std::string &username);
    bool createClientFolderIfNotExists(const std::string &username) const;
};
