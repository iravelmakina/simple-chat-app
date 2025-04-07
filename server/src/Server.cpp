#include "Server.h"
#include "ThreadPool.h"

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <thread>
#include <sys/fcntl.h>



Server::Server(const std::string &directory, const size_t maxSimultaneousClients) : _directory(directory),
    _threadPool(maxSimultaneousClients), _maxSimultaneousClients(maxSimultaneousClients) {
}


void Server::start(const int port) {
    if (!_serverSocket.createS()) {
        return;
    }

    if (!_serverSocket.bindS(port) || !_serverSocket.listenS(SOMAXCONN)) {
        _serverSocket.closeS();
        return;
    }
    std::cout << "Server listening on port " << port << std::endl;
    run();
}


void Server::shutdown() {
    _stopFlag = true;
    _serverSocket.shutdownS();
    _serverSocket.closeS();
    _threadPool.shutdown();
    std::cout << "Server stopped." << std::endl;
}


void Server::handleListRooms(const AuthenticatedClient &client) {
    std::lock_guard<std::mutex> lock(_roomMutex);
    std::ostringstream rooms;
    if (_rooms.empty()) {
        rooms << "No rooms available." << std::endl;
    }

    for (const auto &roomEntry : _rooms) {
        const std::string &roomName = roomEntry.first;
        const Room &room = roomEntry.second;
        rooms << "Room: " + roomName << std::endl;
        for (const AuthenticatedClient &member : room.getMembers()) {
            rooms << "  Member: " << member.username << std::endl;
        }
    }
    client.socket.sendTlv(TlvType::OK, rooms.str());
}


void Server::handleJoin(AuthenticatedClient &client, const std::string &roomName) {
    std::lock_guard<std::mutex> lock(_roomMutex);
    auto it = _rooms.find(roomName);

    if (!client.currentRoom.empty()) {
        client.socket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: You are already in a room.");
        return;
    }

    if (it == _rooms.end()) {
        it = _rooms.emplace(std::piecewise_construct,
                            std::forward_as_tuple(roomName),
                            std::forward_as_tuple(_maxSimultaneousClients / 2)).first;
    }

    if (it->second.isMember(client)) {
        client.socket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: You are already in this room.");
        return;
    }

    if (it->second.isFull()) {
        client.socket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: Room is full.");
        return;
    }

    it->second.addMember(client);
    client.currentRoom = roomName;
    client.socket.sendTlv(TlvType::OK);

    const std::string joinMessage = "Client " + client.username + " has joined the room.";
    broadcastMessage(it->second.getMembers(), joinMessage, client, TlvType::NOTIFICATION);
} // cleanup if client disconnected


void Server::handleLeave(AuthenticatedClient &client) {
    std::lock_guard<std::mutex> lock(_roomMutex);

    if (client.currentRoom.empty()) {
        client.socket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: You are not in a room.");
        return;
    }

    const auto it = _rooms.find(client.currentRoom);

    const std::string leaveMessage = "Client " + client.username + " has left the room.";
    broadcastMessage(it->second.getMembers(), leaveMessage, client, TlvType::NOTIFICATION);

    it->second.removeMember(client);
    client.currentRoom = "";

    if (it->second.isEmpty()) {
        _rooms.erase(it); // destroy?
    }
    client.socket.sendTlv(TlvType::OK);
}


void Server::handleSend(const AuthenticatedClient &client, const std::string &message) {
    std::lock_guard<std::mutex> lock(_roomMutex);
    const auto it = _rooms.find(client.currentRoom);

    if (it == _rooms.end()) {
        client.socket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: You are not in a room.");
        return;
    }

    if (it->second.getMembers().size() == 1) {
        client.socket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: You are the only member in the room.");
        return;
    }

    const std::string notification = "Client " + client.username + ": " + message;
    broadcastMessage(it->second.getMembers(), notification, client, TlvType::NOTIFICATION);

    client.socket.sendTlv(TlvType::OK);
}


Server::~Server() {
    if (!_stopFlag) {
        shutdown();
    }
}


void Server::broadcastMessage(const std::vector<AuthenticatedClient>& members, const std::string& message, const AuthenticatedClient& sender, TlvType type) {
    for (const AuthenticatedClient& member : members) {
        if (member.socket.getS() != sender.socket.getS()) {
            _threadPool.submit([member, message, type] {
                member.socket.sendTlv(type, message);
            });
        }
    }
}


void Server::run() {
    while (!_stopFlag) {
        Socket clientSocket = acceptClient();
        if (clientSocket.getS() != -1) {
            std::cout << "Client connected." << std::endl;
            _threadPool.submit([this, clientSocket] { defineVersionAndHandleClient(clientSocket); });
        }
    }
}


Socket Server::acceptClient() const {
    sockaddr_in clientAddr{};
    socklen_t clientAddrLen = sizeof(clientAddr);

    const int clientFd = _serverSocket.acceptS(&clientAddr, &clientAddrLen);
    if (clientFd == -1) {
        return Socket(-1);
    }

    Socket clientSocket(clientFd);
    clientSocket.setTimeoutSeconds(600);

    if (_threadPool.activeThreads() >= _maxSimultaneousClients) {
        clientSocket.sendTlv(TlvType::ERROR, "503 SERVICE UNAVAILABLE: Server is busy.");
        clientSocket.closeS();
        return clientSocket;
    }

    clientSocket.sendTlv(TlvType::OK);

    return clientSocket;
}


void Server::defineVersionAndHandleClient(Socket clientSocket) {
    TlvType tag;
    std::string value;
    const ReceiveResult result = receiveMessage(clientSocket, tag, value);
    if (result.status != ReceiveStatus::SUCCESS) {
        std::cout << "\033[31m" << result.message << "\033[0m" << std::endl;
        cleanupClient(clientSocket);
        return;
    }

    const std::string initialMessage(value);

    if (initialMessage == "1.0") {
        clientSocket.sendTlv(TlvType::OK);
        handleClient1dot0(clientSocket);
    } else {
        clientSocket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: Invalid version.");
        std::cout << "\033[31m" << "Invalid version." << "\033[0m" << std::endl;
        cleanupClient(clientSocket);
    }
}


void Server::handleClient1dot0(Socket &clientSocket) {
    std::string username;
    if (!authenticateClient(clientSocket, username)) {
        cleanupClient(clientSocket);
        return;
    }

    AuthenticatedClient client = {clientSocket, username, ""};

    clientSocket.sendTlv(TlvType::OK);

    processCommands(client);
    cleanupClient(client.socket, client.username.c_str(), client.currentRoom.c_str());
}


bool Server::authenticateClient(const Socket &clientSocket, std::string &username) {
    TlvType tag;
    std::string value;
    const ReceiveResult result = receiveMessage(clientSocket, tag, value);
    if (result.status != ReceiveStatus::SUCCESS) {
        std::cout << "\033[31m" << result.message << "\033[0m" << std::endl;
        return false;
    }

    username = value;
    if (!isValidAlnumString(username)) {
        clientSocket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: Invalid username.");
        std::cout << "\033[31m" << "Invalid username." << "\033[0m" << std::endl;
        return false;
    }

    std::cout << "Client's name: " << username << std::endl;
    return true;
}


void Server::processCommands(AuthenticatedClient &client) {
    TlvType tag;
    std::string value;
    while (true) {
        ReceiveResult result = receiveMessage(client.socket, tag, value, client.username.c_str());
        if (result.status != ReceiveStatus::SUCCESS) {
            std::cout << "\033[31m" << result.message << "\033[0m" << std::endl;
            break;
        }

        std::cout << "Received command from " << client.username << ": Tag=" << static_cast<int>(tag)
                  << ", Value=" << value << std::endl;

        std::istringstream stream(value);
        std::string roomName, filename, message;
        size_t filesize;

        if (tag == TlvType::SEND_MESSAGE) {
            std::getline(stream, message);
            if (message.empty()) {
                client.socket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: Empty message.");
                return;
            }
        } else if (tag == TlvType::JOIN_ROOM) {
            stream >> roomName;
            if (!isValidAlnumString(roomName)) {
                client.socket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: Invalid room name.");
                return;
            }
        }


        if (tag == TlvType::LIST_ROOMS) {
            handleListRooms(client);
        } else if (tag == TlvType::JOIN_ROOM) {
            handleJoin(client, roomName);
        } else if (tag == TlvType::LEAVE_ROOM) {
            handleLeave(client);
        } else if (tag == TlvType::SEND_MESSAGE) {
            handleSend(client, message);
            break;
        } else {
            client.socket.sendTlv(TlvType::ERROR, "400 BAD REQUEST: Invalid command.");
        }
    }
}


void Server::cleanupClient(Socket &clientSocket, const char *username, const char *roomName) {
    if (roomName != nullptr) {
        std::lock_guard<std::mutex> lock(_roomMutex);
        const auto it = _rooms.find(roomName);
        if (it != _rooms.end()) {
            it->second.removeMember({clientSocket, username, roomName});
            if (it->second.isEmpty()) {
                _rooms.erase(it);
            }
        }
    }

    if (username == nullptr) {
        std::cout << "Closing socket of not authenticated client." << std::endl;
    } else {
        std::cout << "Closing socket of client " << username << "." << std::endl;
    }
    clientSocket.closeS();
}


ReceiveResult Server::receiveMessage(const Socket &clientSocket, TlvType &tag, std::string &value, const char *username) {
    ReceiveResult result;
    result.bytesReceived = clientSocket.receiveTlv(tag, value);

    if (username == nullptr) {
        username = "not authenticated yet";
    }
    const std::string usernameStr(username);

    if (result.bytesReceived > 0) {
        result.status = ReceiveStatus::SUCCESS;
        result.message = "Data received successfully from client " + usernameStr + ".";
    } else if (result.bytesReceived == 0) {
        result.status = ReceiveStatus::CLIENT_DISCONNECTED;
        result.message = "Client " + usernameStr + " disconnected.";
    } else {
        switch (errno) {
            case EAGAIN:
                result.status = ReceiveStatus::TIMEOUT;
                result.message = "Receive timeout from client " + usernameStr + ".";
                break;
            case ECONNRESET:
                result.status = ReceiveStatus::CLIENT_DISCONNECTED;
                result.message = "Client " + usernameStr + " disconnected.";
                break;
            default:
                result.status = ReceiveStatus::ERROR;
                result.message = std::string("Receive error: ") + strerror(errno) + " from client " + usernameStr + ".";
                break;
        }
    }

    return result;
}


bool Server::isValidAlnumString(const std::string &username) {
    for (const char c: username) {
        if (!isalnum(c) || isspace(c)) {
            return false;
        }
    }
    return true;
}
