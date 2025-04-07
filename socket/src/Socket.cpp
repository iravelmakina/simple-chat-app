#include "Socket.h"

#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>


Socket::Socket(const int socketFd) : _socketFd(socketFd) {
}


bool Socket::createS() {
    _socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_socketFd == -1) {
        perror("Error creating socket");
        return false;
    }
    return true;
}


void Socket::closeS() {
    if (_socketFd != -1) {
        close(_socketFd);
    }
    _socketFd = -1;
}


bool Socket::bindS(int port) const {
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(_socketFd, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1) {
        perror("Bind failed");
        return false;
    }
    return true;
}


bool Socket::listenS(const int backlog) const {
    if (listen(_socketFd, backlog) == -1) {
        perror("Listen failed");
        return false;
    }
    return true;
}


int Socket::acceptS(sockaddr_in *clientAddr, socklen_t *clientLen) const {
    const int clientSocket = accept(_socketFd,
                                    reinterpret_cast<struct sockaddr *>(clientAddr),
                                    clientLen);
    if (clientSocket == -1) {
        if (_shutdownFlag && errno == ECONNABORTED) {
            return -1;
        }
        perror("Accept failed");
        return -1;
    }

    return clientSocket;
}


void Socket::shutdownS() {
    if (_socketFd != -1) {
        shutdown(_socketFd, SHUT_RDWR);
        _shutdownFlag = true;
    }
}


bool Socket::connectS(const char *serverIp, int port) const {
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, serverIp, &serverAddr.sin_addr);

    if (connect(_socketFd,
                reinterpret_cast<sockaddr *>(&serverAddr),
                sizeof(serverAddr)) == -1) {
        perror("Connect failed");
        return false;
    }
    return true;
}



ssize_t Socket::sendTlv(TlvType type, const std::string &value) const {
    std::vector<uint8_t> tlvMessage;
    tlvMessage.push_back(static_cast<uint8_t>(type)); // Tag (1 byte)

    if (value.empty()) {
        tlvMessage.push_back(0);
    } else {
        if (value.size() <= 254) {
            tlvMessage.push_back(static_cast<uint8_t>(value.size())); // Length (1 byte)
        } else {
            tlvMessage.push_back(0xFF); // Length indicator for > 254
            const uint16_t len = value.size();
            tlvMessage.push_back(static_cast<uint8_t>(len >> 8 & 0xFF)); // High byte
            tlvMessage.push_back(static_cast<uint8_t>(len & 0xFF)); // Low byte
        }
    }

    // Value (ASCII string)
    tlvMessage.insert(tlvMessage.end(), value.begin(), value.end());

    return send(_socketFd, tlvMessage.data(), tlvMessage.size(), 0);
}


ssize_t Socket::receiveTlv(TlvType &tag, std::string &value) const {
    if (!setRecvTimeout()) {
        return -1; // failed to set receive timeout
    }
    // Read the Tag (1 byte)
    uint8_t tagByte;
    ssize_t bytesReceived = recv(_socketFd, &tagByte, sizeof(tagByte), MSG_WAITALL);
    if (bytesReceived != sizeof(tagByte)) {
        return -1; // Failed to read Tag
    }
    tag = static_cast<TlvType>(tagByte);

    // Read the Length (1 or 3 bytes)
    uint8_t lengthByte;
    bytesReceived = recv(_socketFd, &lengthByte, sizeof(lengthByte), MSG_WAITALL);
    if (bytesReceived != sizeof(lengthByte)) {
        return -1; // Failed to read Length
    }

    size_t length;
    if (lengthByte != 0xFF) {
        length = lengthByte; // Length is 1 byte
    } else {
        // Length is 3 bytes (0xFF followed by 2 bytes)
        uint8_t lengthBytes[2];
        bytesReceived = recv(_socketFd, lengthBytes, sizeof(lengthBytes), MSG_WAITALL);
        if (bytesReceived != sizeof(lengthBytes)) {
            return -1; // Failed to read Length
        }
        length = (lengthBytes[0] << 8) | lengthBytes[1];
    }

    if (length == 0) {
        value.clear(); // Ensure the value is empty
        return sizeof(tagByte) + sizeof(lengthByte); // Return the number of bytes read (tag + length)
    }

    // Read the Value (ASCII string)
    value.resize(length);
    bytesReceived = recv(_socketFd, &value[0], length, MSG_WAITALL);
    if (bytesReceived != static_cast<ssize_t>(length)) {
        return -1; // Failed to read Value
    }

    return sizeof(tagByte) + sizeof(lengthByte) + length;
}


void Socket::setTimeoutSeconds(const int timeoutSeconds) {
    _timeoutSeconds = timeoutSeconds;
}


int Socket::getS() const {
    return _socketFd;
}


void Socket::setS(const int s) {
    _socketFd = s;
}


bool Socket::setRecvTimeout() const {
    if (_timeoutSeconds == -1) {
        return true;
    }

    struct timeval tv{};
    tv.tv_sec = _timeoutSeconds;
    tv.tv_usec = 0;

    if (setsockopt(_socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        perror("error setting receive timeout");
        return false;
    }
    return true;
}
