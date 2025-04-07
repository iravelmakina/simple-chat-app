#pragma once

#include <vector>

#include "Socket.h"
#include "ThreadPool.h"


struct AuthenticatedClient {
    Socket socket;
    std::string username;
    std::string currentRoom;
};


class Room {
public:
    explicit Room(size_t maxSimultaneousMessages);

    void addMember(const AuthenticatedClient &client);
    void removeMember(const AuthenticatedClient &client);

    bool isMember(const AuthenticatedClient &client) const;

    bool isFull() const;
    bool isEmpty() const;

    std::vector<AuthenticatedClient> getMembers() const;

    ThreadPool& getThreadPool();

private:
    std::vector<AuthenticatedClient> _members;
    ThreadPool _threadPool;
    size_t _maxSimultaneousMessages;
};
