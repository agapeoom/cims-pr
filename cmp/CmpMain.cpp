

#include "CmpServer.h"

int main(int argc, char** argv) {
    CmpServer server("CmpServer");

    if (!server.startServer()) {
        return 1;
    }

    // Port is now printed in startServer logs or here via config? 
    // We can't access server member unless we make getPort(). 
    // Or just print "Started" without port here, relying on startServer logging or config.
    // Better yet, update startServer to print.
    printf("CmpServer started.\n");
    
    // Keep main thread alive
    while(true) {
        msleep(1000);
    }
    return 0;
}
