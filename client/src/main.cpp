#include "ClientCLI.h"

int main() {
    ClientCLI cli("files/");
    cli.run("127.0.0.1", 9080);
    return 0;
}
