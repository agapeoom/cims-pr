#include "CmpServer.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <cstring>
#include <thread>
#include <algorithm>

CmpServer::CmpServer(const std::string& name, int port) 
    : PModule(name), _port(port), _running(false), _udpFd(-1), _rtpStartPort(50000), _rtpPoolSize(100), _rtpIp("127.0.0.1")
{
    loadConfig();
    initResourcePool();

    // Initialize a few workers for RTP processing
    for(int i=0; i<4; ++i) {
        std::string wname = formatStr("RtpWorker_%d", i);
        addWorker(wname, 1, 2048, true);
    }
}

CmpServer::~CmpServer() {
    stopServer();
    for(auto const& [name, group] : _groups) {
        delete group;
    }
    _groups.clear();

    for(auto* rtp : _resourcePool) {
        delete rtp;
    }
    _resourcePool.clear();
    _freeResources.clear();
}

bool CmpServer::startServer() {
    _udpFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (_udpFd < 0) {
        perror("socket");
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);

    if (bind(_udpFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(_udpFd);
        return false;
    }

    _running = true;
    std::thread([this]() {
        this->runControlLoop();
    }).detach();

    return true;
}

void CmpServer::stopServer() {
    _running = false;
    if (_udpFd >= 0) {
        // close(_udpFd);
        ::close(_udpFd);
        _udpFd = -1;
    }
}

void CmpServer::runControlLoop() {
    char buf[4096];
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    while (_running) {
        int len = recvfrom(_udpFd, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&clientAddr, &addrLen);
        if (len > 0) {
            buf[len] = '\0';
            std::string ip = inet_ntoa(clientAddr.sin_addr);
            int port = ntohs(clientAddr.sin_port);
            handlePacket(buf, len, ip, port);
        }
    }
}

#include <cctype>
// ...

void CmpServer::handlePacket(char* buf, int len, const std::string& ip, int port) {
    std::string line(buf);
    // printf("RX: %s\n", line.c_str());
    std::stringstream ss(line);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) return;

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
        [](unsigned char c){ return std::tolower(c); });

    printf("CMD: %s\n", cmd.c_str());
    fflush(stdout);

    if (cmd == "add") {
        processAdd(tokens, ip, port);
    } else if (cmd == "remove") {
        processRemove(tokens, ip, port);
    } else if (cmd == "modify") {
        processModify(tokens, ip, port);
    } else if (cmd == "alive") {
        processAlive(tokens, ip, port);
    } else if (cmd == "addgroup") {
        processAddGroup(tokens, ip, port);
    } else if (cmd == "removegroup") {
        processRemoveGroup(tokens, ip, port);
    } else if (cmd == "joingroup") {
        processJoinGroup(tokens, ip, port);
    } else if (cmd == "leavegroup") {
        processLeaveGroup(tokens, ip, port);
    }
}

void CmpServer::processAdd(const std::vector<std::string>& tokens, const std::string& ip, int port) {
    // add <id> <rmt_ip> <rmt_port> [rmt_video_port]
    if (tokens.size() < 4) return;

    std::string id = tokens[1];
    std::string rmtIp = tokens[2];
    int rmtPort = std::stoi(tokens[3]);
    
    int rmtVideoPort = 0;
    if (tokens.size() >= 5) {
        rmtVideoPort = std::stoi(tokens[4]);
    }

    PAutoLock lock(_mutex);
    if (_sessions.find(id) != _sessions.end()) {
        PRtpTrans* rtp = _sessions[id];
        rtp->setRmt(rmtIp, rmtPort, rmtVideoPort);
        
        // Get existing local ports
        std::string locIp = _rtpIp;
        int locPort = rtp->getLocalPort();
        int locVideoPort = rtp->getLocalVideoPort();
        
        std::string resp = "OK " + locIp + " " + std::to_string(locPort) + " " + std::to_string(locVideoPort);
        sendResponse(ip, port, resp);
        printf("[ADD] ID=%s (Updated) Rmt=%s:%d VideoRmt=%d -> Existing Loc=%s:%d VideoLoc=%d\n", 
               id.c_str(), rmtIp.c_str(), rmtPort, rmtVideoPort, locIp.c_str(), locPort, locVideoPort);
        return;
    }

    // Allocate Resource
    std::string locIp = _rtpIp;
    int locPort = 0;
    int locVideoPort = 0;
    
    PRtpTrans* rtp = allocResource(locIp, locPort, locVideoPort);
    if (!rtp) {
        sendResponse(ip, port, "ERROR: No resources");
        return;
    }
    
    // Set Session Name/ID
    rtp->setSessionId(id);
    
    rtp->setRmt(rmtIp, rmtPort, rmtVideoPort);

    _sessions[id] = rtp;
    
    // Distribute to workers
    static int workerIdx = 0;
    std::string wname = formatStr("RtpWorker_%d", workerIdx++ % 4);
    addHandler(wname, rtp);

    // Response: OK <loc_ip> <loc_port> <loc_video_port>
    std::string resp = "OK " + locIp + " " + std::to_string(locPort) + " " + std::to_string(locVideoPort);
    sendResponse(ip, port, resp);
    
    printf("[ADD] ID=%s Rmt=%s:%d VideoRmt=%d -> Assigned Loc=%s:%d VideoLoc=%d\n", 
           id.c_str(), rmtIp.c_str(), rmtPort, rmtVideoPort, locIp.c_str(), locPort, locVideoPort);
}

void CmpServer::processRemove(const std::vector<std::string>& tokens, const std::string& ip, int port) {
    // remove <id>
    if (tokens.size() < 2) return;
    std::string id = tokens[1];

    PAutoLock lock(_mutex);
    auto it = _sessions.find(id);
    if (it != _sessions.end()) {
        PRtpTrans* rtp = it->second;
        // Return to pool
        freeResource(rtp);
        
        _sessions.erase(it);
        sendResponse(ip, port, "OK");
        printf("[REMOVE] ID=%s\n", id.c_str());
    } else {
        sendResponse(ip, port, "ERROR: ID not found");
    }
}

void CmpServer::processModify(const std::vector<std::string>& tokens, const std::string& ip, int port) {
    // modify <id> <rmt_ip> <rmt_port>
    if (tokens.size() < 4) return;
    std::string id = tokens[1];
    std::string rmtIp = tokens[2];
    int rmtPort = std::stoi(tokens[3]);

    PAutoLock lock(_mutex);
    auto it = _sessions.find(id);
    if (it != _sessions.end()) {
        it->second->setRmt(rmtIp, rmtPort);
        sendResponse(ip, port, "OK");
        printf("[MODIFY] ID=%s Rmt=%s:%d\n", id.c_str(), rmtIp.c_str(), rmtPort);
    } else {
        sendResponse(ip, port, "ERROR: ID not found");
    }
}

void CmpServer::processAlive(const std::vector<std::string>& tokens, const std::string& ip, int port) {
    sendResponse(ip, port, "OK");
}

void CmpServer::processAddGroup(const std::vector<std::string>& tokens, const std::string& ip, int port) {
    // addGroup <groupId>
    if (tokens.size() < 2) return;
    std::string groupId = tokens[1];
    
    PAutoLock lock(_mutex);
    if (_groups.find(groupId) != _groups.end()) {
        sendResponse(ip, port, "OK");
        printf("[ADD_GROUP] ID=%s (Already exists)\n", groupId.c_str());
        fflush(stdout);
        return;
    }
    
    McpttGroup* group = new McpttGroup(groupId);
    _groups[groupId] = group;
    sendResponse(ip, port, "OK");
    printf("[ADD_GROUP] ID=%s\n", groupId.c_str());
    fflush(stdout);
}

void CmpServer::processRemoveGroup(const std::vector<std::string>& tokens, const std::string& ip, int port) {
    // removeGroup <groupId>
    if (tokens.size() < 2) return;
    std::string groupId = tokens[1];
    
    PAutoLock lock(_mutex);
    auto it = _groups.find(groupId);
    if (it != _groups.end()) {
        delete it->second;
        _groups.erase(it);
        sendResponse(ip, port, "OK");
        printf("[REMOVE_GROUP] ID=%s\n", groupId.c_str());
    } else {
        sendResponse(ip, port, "ERROR: Group not found");
    }
}

void CmpServer::processJoinGroup(const std::vector<std::string>& tokens, const std::string& ip, int port) {
    // joinGroup <groupId> <sessionId>
    if (tokens.size() < 3) return;
    std::string groupId = tokens[1];
    std::string sessionId = tokens[2];
    
    PAutoLock lock(_mutex);
    auto groupIt = _groups.find(groupId);
    auto sessionIt = _sessions.find(sessionId);
    
    // Idempotency: If already joined, return OK
    if (groupIt != _groups.end() && groupIt->second->hasMember(sessionId)) {
        sendResponse(ip, port, "OK");
        printf("[JOIN_GROUP] Group=%s Session=%s (Already joined)\n", groupId.c_str(), sessionId.c_str());
        return;
    }

    if (groupIt == _groups.end()) {
        sendResponse(ip, port, "ERROR: Group not found");
        return;
    }
    if (sessionIt == _sessions.end()) {
        sendResponse(ip, port, "ERROR: Session not found");
        return;
    }
    
    groupIt->second->addMember(sessionId, sessionIt->second);
    sessionIt->second->setGroup(groupIt->second);
    
    sendResponse(ip, port, "OK");
    printf("[JOIN_GROUP] Group=%s Session=%s\n", groupId.c_str(), sessionId.c_str());
}

void CmpServer::processLeaveGroup(const std::vector<std::string>& tokens, const std::string& ip, int port) {
     // leaveGroup <groupId> <sessionId>
    if (tokens.size() < 3) return;
    std::string groupId = tokens[1];
    std::string sessionId = tokens[2];
    
    PAutoLock lock(_mutex);
    auto groupIt = _groups.find(groupId);
    auto sessionIt = _sessions.find(sessionId);
    
    if (groupIt == _groups.end()) {
        sendResponse(ip, port, "ERROR: Group not found");
        return;
    }
    
    groupIt->second->removeMember(sessionId);
    if (sessionIt != _sessions.end()) {
        sessionIt->second->setGroup(NULL);
    }
    
    sendResponse(ip, port, "OK");
    printf("[LEAVE_GROUP] Group=%s Session=%s\n", groupId.c_str(), sessionId.c_str());
}

void CmpServer::sendResponse(const std::string& ip, int port, const std::string& msg) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);

    sendto(_udpFd, msg.c_str(), msg.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
}

void CmpServer::loadConfig() {
    FILE* fp = fopen("cmp.conf", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char key[128], val[128];
            if (sscanf(line, "%[^=]=%s", key, val) == 2) {
                if (strcmp(key, "RtpStartPort") == 0) _rtpStartPort = atoi(val);
                if (strcmp(key, "RtpPoolSize") == 0) _rtpPoolSize = atoi(val);
                if (strcmp(key, "RtpIp") == 0) _rtpIp = val;
                if (strcmp(key, "ServerIp") == 0) _serverIp = val;
                if (strcmp(key, "ServerPort") == 0) _serverPort = atoi(val);
            }
        }
        fclose(fp);
    }
    printf("Config: RtpStartPort=%d, RtpPoolSize=%d, RtpIp=%s, ServerIp=%s, ServerPort=%d\n", 
           _rtpStartPort, _rtpPoolSize, _rtpIp.c_str(), _serverIp.c_str(), _serverPort);
}

void CmpServer::initResourcePool() {
    // Each resource needs Audio RTP, RTCP (Port+1), Video RTP (Port+2), Video RTCP (Port+3) -> Skip 4
    int currentPort = _rtpStartPort;
    for (int i = 0; i < _rtpPoolSize; ++i) {
        std::string name = formatStr("InActiveRtp_%d", i);
        PRtpTrans* rtp = new PRtpTrans(name);
        
        // Pre-bind sockets
        // Using _rtpIp for bind
        if (rtp->init(_rtpIp, currentPort, currentPort + 2)) {
             _resourcePool.push_back(rtp);
             _freeResources.push_back(rtp);
        } else {
             printf("Failed to init resource on port %d\n", currentPort);
             delete rtp;
        }
        currentPort += 4;
    }
    printf("Initialized %lu resources\n", _resourcePool.size());
}

#include "PRtpHandler.h" // Need access to CRtpSocket methods if we were to inspect ports, but PRtpTrans encapsulates it.
// To get ports from PRtpTrans, we might need a getter. 
// Ah, PRtpTrans::init binds the ports. We know the ports because we assigned them.
// But we need to return them in allocResource.
// Let's modify PRtpTrans to store its bound ports or just calculate them here?
// We can calculate them based on index or store in map?
// Actually PRtpTrans stores _portLoc. We can add a getter or just trust our assignment.
// We assigned currentPort and currentPort + 2.
// Wait, we need to map PRtpTrans* back to ports or store ports in PRtpTrans.
// Let's assume we can add getLocalPort() to PRtpTrans.

PRtpTrans* CmpServer::allocResource(std::string& rtpIp, int& rtpPort, int& videoPort) {
    // Lock is held by caller (processAdd)
    if (_freeResources.empty()) return NULL;
    
    PRtpTrans* rtp = _freeResources.back();
    _freeResources.pop_back();
    
    // We need to retrieve the ports this RTP was initialized with.
    // For now, let's add a getter to PRtpTrans or just hack it
    // Better to add getLocalPort() to PRtpTrans.h
    // Assuming we will add it.
    rtpIp = _rtpIp; 
    rtpPort = rtp->getLocalPort(); 
    videoPort = rtp->getLocalVideoPort();
    
    return rtp;
}

void CmpServer::freeResource(PRtpTrans* rtp) {
    // Lock is held by caller
    // Reset remote?
    // rtp->final()? No, we want to keep it bound.
    // Maybe rtp->reset()?
    // We just setRmt to 0/empty?
    // For now just put back to pool.
    _freeResources.push_back(rtp);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    CmpServer server("CmpServer", port);

    if (!server.startServer()) {
        return 1;
    }

    printf("CmpServer started on port %d\n", port);
    
    // Keep main thread alive
    while(true) {
        msleep(1000);
    }
    return 0;
}
