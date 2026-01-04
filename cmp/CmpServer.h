#ifndef __CMP_SERVER_H__
#define __CMP_SERVER_H__

#include <string>
#include <map>
#include <iostream>
#include "pbase.h"
#include "pmodule.h"
#include "PRtpHandler.h"
#include "McpttGroup.h"

class CmpServer : public PModule {
public:
    CmpServer(const std::string& name, int port);
    virtual ~CmpServer();

    bool startServer();
    void stopServer();

    void runControlLoop(); // Main loop for UDP control

protected:
    void handlePacket(char* buf, int len, const std::string& ip, int port);
    void processAdd(const std::vector<std::string>& tokens, const std::string& ip, int port);
    void processRemove(const std::vector<std::string>& tokens, const std::string& ip, int port);
    void processModify(const std::vector<std::string>& tokens, const std::string& ip, int port);
    void processAlive(const std::vector<std::string>& tokens, const std::string& ip, int port);

    // Group Management
    void processAddGroup(const std::vector<std::string>& tokens, const std::string& ip, int port);
    void processRemoveGroup(const std::vector<std::string>& tokens, const std::string& ip, int port);
    void processJoinGroup(const std::vector<std::string>& tokens, const std::string& ip, int port);
    void processLeaveGroup(const std::vector<std::string>& tokens, const std::string& ip, int port);

    void sendResponse(const std::string& ip, int port, const std::string& msg);

    // Resource Management
    void loadConfig();
    void initResourcePool();
    PRtpTrans* allocResource(std::string& rtpIp, int& rtpPort, int& videoPort);
    void freeResource(PRtpTrans* rtp);

private:
    int _udpFd;
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

    std::vector<PRtpTrans*> _resourcePool; // Pre-allocated list
    std::vector<PRtpTrans*> _freeResources; // Available for use
};

#endif // __CMP_SERVER_H__
