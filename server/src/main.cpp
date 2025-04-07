#include <iostream>
#include <thread>
#include "Server.h"


int main() {
    Server server("files/", 8);
    std::thread serverThread([&server] { server.start(9080); });

    while (true) {
        if (std::cin.get() == 'q') {
            server.shutdown();
            serverThread.join();
            break;
        }
    }

    return 0;
}