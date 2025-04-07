#pragma once

#include <Socket.h>
#include <thread>


class Client {
public:
    explicit Client(const std::string &directory);

    int connect(const char *serverIp, int port);
    void disconnect();
    bool isConnected() const;

    int sendUsername(const std::string &username);

    void listRooms();
    void joinRoom(const std::string &roomName);
    void leaveRoom();
    void sendMessage(const std::string &message);

    void processMessages();

private:
    Socket _socket;
    const std::string _directory;
    std::atomic<bool> _isRunning{false};

    std::mutex _mutex;
    std::condition_variable _cv;
    std::mutex _responseMutex;
    std::condition_variable _responseCondition;
    std::atomic<bool> _expectingResponse{false};
    std::atomic<bool> _hasResponse{false};
    std::pair<TlvType, std::string> _currentResponse;
    std::pair<TlvType, std::string> _response;
    std::thread _receiverThread;
    std::queue<std::pair<TlvType, std::string>> _messageQueue;
    std::mutex _messageQueueMutex;
    std::condition_variable _messageCondition;

    std::pair<TlvType, std::string> waitForResponse();

    void receiveMessagesLoop();
};
