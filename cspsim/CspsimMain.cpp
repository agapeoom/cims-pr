#include <vector>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "SimSession.h"
#include "Log.h"

// Basic Console Logger
class CConsoleLog : public ILogCallBack
{
public:
    void Print( EnumLogLevel eLevel, const char * fmt, ... )
    {
        if( (eLevel & LOG_NETWORK) == 0 ) return;

        va_list ap;
        char    szBuf[LOG_MAX_SIZE];

        va_start( ap, fmt );
        vsnprintf( szBuf, sizeof(szBuf), fmt, ap );
        va_end( ap );

        printf( "%s\n", szBuf );
    }
};

CConsoleLog gclsConsoleLog;

// Helper to get command line args
std::string GetArg(int argc, char* argv[], const char* pszKey, const char* pszDefault = "") {
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], pszKey) == 0) {
            return argv[i + 1];
        }
    }
    return pszDefault;
}

std::string GetLocalIp() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return "127.0.0.1";

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8"); 
    serv.sin_port = htons(53);

    if (connect(sock, (const struct sockaddr*)&serv, sizeof(serv)) < 0) {
        close(sock);
        return "127.0.0.1";
    }

    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    if (getsockname(sock, (struct sockaddr*)&name, &namelen) < 0) {
        close(sock);
        return "127.0.0.1";
    }

    char buffer[INET_ADDRSTRLEN];
    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, sizeof(buffer));
    close(sock);

    if (p) return std::string(buffer);
    return "127.0.0.1";
}

int main(int argc, char* argv[])
{
    if( argc < 2 || GetArg(argc, argv, "-help") != "")
    {
        printf( "[Usage] %s -server_ip <ip> -server_port <port> -user_count <N> -user <start_id> -mode <call|ptt>\n", argv[0] );
        return 0;
    }

    // Parse Args
    std::string strServerIp = GetArg(argc, argv, "-server_ip", "127.0.0.1");
    int iServerPort = atoi(GetArg(argc, argv, "-server_port", "5060").c_str());
    int iUserCount = atoi(GetArg(argc, argv, "-user_count", "1").c_str());
    int iStartUser = atoi(GetArg(argc, argv, "-user", "1000").c_str());
    std::string strMode = GetArg(argc, argv, "-mode", "call");
    bool bPttMode = (strMode == "ptt");
    
    // Local IP/Port
    std::string strLocalIp = GetArg(argc, argv, "-local_ip", "");
    if (strLocalIp.empty()) strLocalIp = GetLocalIp();
    
    // Setup Logging
    CLog::SetDirectory("log");
    CLog::SetLevel( LOG_NETWORK | LOG_DEBUG | LOG_INFO );
    CLog::SetCallBack( &gclsConsoleLog );

    printf("Starting cspsim with %d sessions...\n", iUserCount);
    printf("Server: %s:%d\n", strServerIp.c_str(), iServerPort);
    printf("Local IP: %s\n", strLocalIp.c_str());
    printf("Mode: %s\n", bPttMode ? "PTT" : "Call");

    std::vector<SimSession*> sessions;

    for (int i = 0; i < iUserCount; i++) {
        int userId = iStartUser + i;
        char szUser[32];
        sprintf(szUser, "%d", userId);
        
        // Random local port for SIP (let stack/OS decide if 0, but usually we give 0 to let it bind)
        // SipStack usually takes a specific port or fails if busy.
        // We can let them try to bind to sequential ports or 0 (random).
        // The original code tried explicit ports.
        // SipUserAgent::Start receives Setup info.
        // If we define port 0, does the stack handle unique binding?
        // Let's assume passing 0 lets it pick an ephemeral port or we need to assign.
        // To be safe, let's assign ports starting from 5060 + i * 2 (if available) or similar, 
        // OR just pass 0 and hope the stack does the right thing.
        // SipClientMain used random port if 0.
        // Assign explicit unique port for each session
        int iLocalPort = 6000 + (i * 2);

        SimSession* session = new SimSession(i, szUser, "csp", "1234", 
                                             strServerIp, iServerPort, strLocalIp, iLocalPort, bPttMode);
        
        if (session->Start()) {
            sessions.push_back(session);
        } else {
            printf("Failed to start session %d (User %s)\n", i, szUser);
            delete session;
        }
        
        // Stagger starts slightly to avoid thundering herd on network/logs
        usleep(100000); 
    }

    printf("All sessions started. Press 'q' to quit.\n");
    
    // Main Command Loop (Global scope, affects all or specific?)
    // For now, simple loop to keep alive.
    
    char szCommand[1024];
    while( fgets( szCommand, sizeof(szCommand), stdin ) )
    {
        if( szCommand[0] == 'q' || szCommand[0] == 'Q' ) break;
        
        // Broadcast generic commands to all sessions for testing?
        // e.g. 'c' calls all? 't' talks all?
        // Usage: "c" -> All call
        // "t" -> All sending talk burst
        
        if (szCommand[0] == 't') { // PTT Talk
             for(auto* s : sessions) s->SendPttRequest();
        } else if (szCommand[0] == 'r') { // PTT Release
             for(auto* s : sessions) s->SendPttRelease();
        } else if (szCommand[0] == 'c') { // Call
             // Check if arguments provided: c <idx> <target>
             int sessionIdx = -1;
             char szTarget[64];
             int nArgs = sscanf(szCommand + 1, "%d %63s", &sessionIdx, szTarget);
             
             if (nArgs == 2) {
                 if (sessionIdx >= 0 && sessionIdx < (int)sessions.size()) {
                     sessions[sessionIdx]->StartCall(szTarget);
                 } else {
                     printf("Invalid session index: %d\n", sessionIdx);
                 }
             } else {
                 printf("Broadcasting call start to ALL sessions...\n");
                 for(auto* s : sessions) s->StartCall();
             }
        } else if (szCommand[0] == 'e') { // End Call
             for(auto* s : sessions) s->StopCall();
        }
    }

    printf("Stopping all sessions...\n");
    for(auto* s : sessions) {
        delete s;
    }
    sessions.clear();

    return 0;
}
