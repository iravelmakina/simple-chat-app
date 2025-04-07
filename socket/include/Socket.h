#pragma once

#include <string>
#include <netinet/in.h>


// constants for buffer sizes
constexpr int FILE_BUFFER_SIZE = 1024;
constexpr int MESSAGE_SIZE = 512;


enum class TlvType : uint8_t {
    VERSION = 0x01,
    USERNAME = 0x02,
    LIST_ROOMS = 0x03,
    JOIN_ROOM = 0x04,
    LEAVE_ROOM = 0x05,
    SEND_MESSAGE = 0x06,
    SEND_FILE_REQUEST = 0x07,
    SEND_FILE_RESPONSE = 0x08,
    FILE_TRANSFER = 0x09,
    NOTIFICATION = 0x0A,
    EXIT = 0x0B,
    OK = 0x0C,
    ERROR = 0x0D,
    FILE_DOWNLOAD = 0x0E,
    INVALID = 0xFF // invalid TLV
};


class Socket {
public:
    explicit Socket(int socketFd = -1);

    bool createS();
    void closeS();

    bool bindS(int port) const;
    bool listenS(int backlog) const;

    int acceptS(sockaddr_in *clientAddr, socklen_t *clientLen) const;
    void shutdownS();

    bool connectS(const char *serverIp, int port) const;

    ssize_t sendTlv(TlvType type, const std::string &value = "") const;
    ssize_t receiveTlv(TlvType &tag, std::string &value) const;

    void setTimeoutSeconds(int timeoutSeconds);

    int getS() const;
    void setS(int s);

private:
    int _socketFd;
    int _timeoutSeconds{-1};
    bool _shutdownFlag{false};

    bool setRecvTimeout() const;
};
