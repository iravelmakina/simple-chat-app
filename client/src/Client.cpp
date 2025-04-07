#include "Client.h"

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


Client::Client(const std::string &directory) : _directory(directory) {
}


int Client::connect(const char *serverIp, const int port) {
    if (!_socket.createS()) {
        return -1;
    }

    if (!_socket.connectS(serverIp, port)) {
        _socket.closeS();
        return -1;
    }

    _isRunning = true;
    _receiverThread = std::thread(&Client::receiveMessagesLoop, this);

    const std::pair<TlvType, std::string> connectionResponse = waitForResponse();
    if (connectionResponse.first != TlvType::OK) {
        std::cout << connectionResponse.second << std::endl;
        return -1;
    }

    _socket.sendTlv(TlvType::VERSION, "1.0");
    const std::pair<TlvType, std::string> versionResponse = waitForResponse();
    if (versionResponse.first != TlvType::OK) {
        std::cout << versionResponse.second << std::endl;
        return -1;
    }

    std::cout << "\nConnected to server at " << serverIp << ":" << port << "." << std::endl;
    return 0;
}


void Client::disconnect() {
    _isRunning = false;
    _socket.sendTlv(TlvType::EXIT, "");
    _socket.closeS();
    if (_receiverThread.joinable()) {
        _receiverThread.join();
    }
    std::cout << "\nDisconnected from server." << std::endl;
}


bool Client::isConnected() const {
    return _socket.getS() != -1;
}


int Client::sendUsername(const std::string& username) {
    _socket.sendTlv(TlvType::USERNAME, username);

    const std::pair<TlvType, std::string> usernameResponse = waitForResponse();
    if (usernameResponse.first != TlvType::OK) {
        std::cout << usernameResponse.second << std::endl;
        return -1;
    }

    return 0;
}


void Client::listRooms() {
    _socket.sendTlv(TlvType::LIST_ROOMS, "");

    const std::pair<TlvType, std::string> listResponse = waitForResponse();
    if (listResponse.first == TlvType::OK) {
        std::cout << listResponse.second << std::endl;
    } else {
        std::cout << listResponse.second << std::endl;
    }
}


void Client::joinRoom(const std::string &roomName) {
    _socket.sendTlv(TlvType::JOIN_ROOM, roomName);

    const std::pair<TlvType, std::string> joinResponse = waitForResponse();
    if (joinResponse.first == TlvType::OK) {
        std::cout << "Joined room: " << roomName << std::endl;
    } else {
        std::cout << joinResponse.second << std::endl;
    }
}


void Client::leaveRoom() {
    _socket.sendTlv(TlvType::LEAVE_ROOM, "");

    const std::pair<TlvType, std::string> leaveResponse = waitForResponse();
    if (leaveResponse.first == TlvType::OK) {
        std::cout << "Left room." << std::endl;
    } else {
        std::cout << leaveResponse.second << std::endl;
    }
}


void Client::sendMessage(const std::string &message) {
    _socket.sendTlv(TlvType::SEND_MESSAGE, message);

    const std::pair<TlvType, std::string> sendMessageResponse = waitForResponse();
    if (sendMessageResponse.first == TlvType::OK) {
        std::cout << "Sent." << std::endl;
    } else {
        std::cout << sendMessageResponse.second << std::endl;
    }
}


std::pair<TlvType, std::string> Client::waitForResponse() {
    std::unique_lock<std::mutex> lock(_mutex);
    _expectingResponse = true;
    _cv.wait(lock, [this] { return !_expectingResponse; });
    return _response;
}


void Client::receiveMessagesLoop() {
    while (_isRunning) {
        TlvType tag;
        std::string value;
        const ssize_t bytesReceived = _socket.receiveTlv(tag, value);

        if (bytesReceived <= 0) {
            if (_isRunning) {
                std::cout << "\033[31m" << (bytesReceived == 0 ?
                    "Server closed the connection." : "No response from server.") << "\033[0m" << std::endl;
            }
            break;
        }

        std::unique_lock<std::mutex> lock(_responseMutex);
        if (_expectingResponse) {
            _currentResponse = {tag, value};
            _hasResponse = true;
            _expectingResponse = false;
            lock.unlock();
            _responseCondition.notify_one();
        } else {
            lock.unlock();
            // Instead of processing here, push to queue
            std::lock_guard<std::mutex> queueLock(_messageQueueMutex);
            _messageQueue.push({tag, value});
            _messageCondition.notify_one();
        }
    }
}


void Client::processMessages() {
    while (_isRunning) {
        std::pair<TlvType, std::string> message;
        {
            std::unique_lock<std::mutex> lock(_messageQueueMutex);
            _messageCondition.wait(lock, [this] {
                return !_messageQueue.empty() || !_isRunning;
            });

            if (!_isRunning) break;
            if (_messageQueue.empty()) continue;

            message = _messageQueue.front();
            _messageQueue.pop();
        }

        switch (message.first) {
            case TlvType::NOTIFICATION:
                std::cout << "\n\nNotification: " << message.second << std::endl;
            break;

            default:
                std::cerr << "Unknown message type received." << std::endl;
            break;
        }
    }
}

