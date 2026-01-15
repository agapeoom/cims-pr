#ifndef __CMP_SERVER_H__
#define __CMP_SERVER_H__

#include <string>
#include <map>
#include <iostream>
//#include "pbase.h"
#include "pmodule.h"
#include "PRtpHandler.h"
#include "McpttGroup.h"

class CmpServer : public PModule {
public:
    CmpServer(const std::string& name, const std::string& configFile = "cmp.conf");
    virtual ~CmpServer();

    bool startServer();
    void stopServer();

    void runControlLoop(); // Main loop for UDP control

protected:
    void handlePacket(char* buf, int len, const std::string& ip, int port);
    void processAdd(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header);
    void processRemove(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header);
    void processModify(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header);
    void processAlive(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header);

    // Group Management
    void processAddGroup(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header);
    void processRemoveGroup(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header);
    void processJoinGroup(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header);
    void processLeaveGroup(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header);

    void sendResponse(const std::string& ip, int port, const std::string& msg);

    // Resource Management
    void loadConfig();
    void initResourcePool();
    PRtpTrans* allocResource(std::string& rtpIp, int& rtpPort, int& videoPort);
    void freeResource(PRtpTrans* rtp);

private:
    int _udpFd;
    bool _running;
    
    std::map<std::string, PRtpTrans*> _sessions;
    std::map<std::string, McpttGroup*> _groups;
    PMutex _mutex;

    // Resource Pool
    int _rtpStartPort;
    int _rtpPoolSize;
    std::string _rtpIp;
    
    // Server Config
    std::string _serverIp;
    int _serverPort;
    std::string _configFile;

    std::vector<PRtpTrans*> _resourcePool; // Pre-allocated list
    std::vector<PRtpTrans*> _freeResources; // Available for use
};

#endif // __CMP_SERVER_H__
