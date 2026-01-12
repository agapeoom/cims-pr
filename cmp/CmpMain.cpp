

#include "CmpServer.h"

int main(int argc, char** argv) {
    std::string configFile = "cmp.conf";
    if (argc > 1) {
        configFile = argv[1];
    }
    CmpServer server("cmp", configFile);

    if (!server.startServer()) {
        return 1;
    }

    // Port is now printed in startServer logs or here via config? 
    // We can't access server member unless we make getPort(). 
    // Or just print "Started" without port here, relying on startServer logging or config.
    // Better yet, update startServer to print.
    printf("cmp started.\n");
    
    // Keep main thread alive
    while(true) {
        msleep(1000);
    }
    return 0;
}
