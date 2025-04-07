#include "Room.h"

#include <iostream>


Room::Room(const size_t maxSimultaneousMessages) : _threadPool(maxSimultaneousMessages), _maxSimultaneousMessages(maxSimultaneousMessages) {}


void Room::addMember(const AuthenticatedClient &client) {
    _members.push_back(client);
}


void Room::removeMember(const AuthenticatedClient &client) {
    _members.erase(std::remove_if(_members.begin(), _members.end(), [&client](const AuthenticatedClient &member) {
        return member.socket.getS() == client.socket.getS();
    }), _members.end());
}


bool Room::isMember(const AuthenticatedClient &client) const {
    for (const AuthenticatedClient &member: _members) {
        if (member.socket.getS() == client.socket.getS()) {
            return true;
        }
    }
    return false;
}


bool Room::isFull() const {
    return _threadPool.activeThreads() > _maxSimultaneousMessages;
}


bool Room::isEmpty() const {
    return _members.empty();
}


std::vector<AuthenticatedClient> Room::getMembers() const {
    return _members;
}


ThreadPool &Room::getThreadPool() {
    return _threadPool;
}
