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
#include <cctype>
#include "PRtpHandler.h"
#include "McpttGroup.h"

CmpServer::CmpServer(const std::string& name, const std::string& configFile)
    : PModule(name), _running(false), _udpFd(-1), _configFile(configFile)
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
        // delete group; // PModule/destructor might be an issue. McpttGroup should be deleted clearly.
        // Actually, _groups stores McpttGroup* which we new'd.
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
    addr.sin_addr.s_addr = inet_addr(_serverIp.c_str());
    addr.sin_port = htons(_serverPort);

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

void CmpServer::handlePacket(char* buf, int len, const std::string& ip, int port) {
    std::string line(buf);
    std::stringstream ss(line);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> token) {
        tokens.push_back(token);
    }

    // Protocol: TRANS_ID CSP_ID CSP_SESS CMP_ID CMP_SESS CMD ...
    // Indices:  0        1      2        3      4        5
    if (tokens.size() < 6) return; // Ignore invalid

    std::string headerString = tokens[0] + " " + tokens[1] + " " + tokens[2] + " " + tokens[3] + " " + tokens[4];
    std::string cmd = tokens[5];
    
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
        [](unsigned char c){ return std::tolower(c); });

    printf("CMD: %s (Header: %s)\n", cmd.c_str(), headerString.c_str());
    fflush(stdout);

    if (cmd == "add") {
        processAdd(tokens, ip, port, headerString);
    } else if (cmd == "remove") {
        processRemove(tokens, ip, port, headerString);
    } else if (cmd == "modify") {
        processModify(tokens, ip, port, headerString);
    } else if (cmd == "alive") {
        processAlive(tokens, ip, port, headerString);
    } else if (cmd == "addgroup") {
        processAddGroup(tokens, ip, port, headerString);
    } else if (cmd == "removegroup") {
        processRemoveGroup(tokens, ip, port, headerString);
    } else if (cmd == "joingroup") {
        processJoinGroup(tokens, ip, port, headerString);
    } else if (cmd == "leavegroup") {
        processLeaveGroup(tokens, ip, port, headerString);
    } else if (cmd == "modifygroup") {
        processModifyGroup(tokens, ip, port, headerString);
    }
}

// NOTE: All process functions now receive full tokens.
// Payload starts at index 6 (CMD is 5).
// Previous index 1 ("id") is now index 6.

void CmpServer::processAdd(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header) {
    // CMD: add <id> <rmt_ip> <rmt_port> [rmt_video_port] [peer_idx]
    // Tokens: [0..4] [5=add] [6=id] [7=ip] [8=port] [9=vport] [10=pidx]
    
    if (tokens.size() < 9) return;

    std::string id = tokens[6];
    std::string rmtIp = tokens[7];
    int rmtPort = std::stoi(tokens[8]);
    
    int rmtVideoPort = 0;
    if (tokens.size() >= 10) {
        rmtVideoPort = std::stoi(tokens[9]);
    }
    
    int peerIdx = -1;
    if (tokens.size() >= 11) {
        peerIdx = std::stoi(tokens[10]);
    }

    PAutoLock lock(_mutex);
    if (_sessions.find(id) != _sessions.end()) {
        PRtpTrans* rtp = _sessions[id];
        rtp->setRmt(rmtIp, rmtPort, rmtVideoPort, peerIdx);
        
        std::string locIp = _rtpIp;
        int locPort = rtp->getLocalPort();
        int locVideoPort = rtp->getLocalVideoPort();
        
        std::string resp = header + " OK " + locIp + " " + std::to_string(locPort) + " " + std::to_string(locVideoPort);
        sendResponse(ip, port, resp);
        printf("[ADD] ID=%s (Updated) Peer=%d Rmt=%s:%d VideoRmt=%d -> Loc=%s:%d\n", 
               id.c_str(), peerIdx, rmtIp.c_str(), rmtPort, rmtVideoPort, locIp.c_str(), locPort);
        return;
    }

    std::string locIp = _rtpIp;
    int locPort = 0;
    int locVideoPort = 0;
    
    PRtpTrans* rtp = allocResource(locIp, locPort, locVideoPort);
    if (!rtp) {
        sendResponse(ip, port, header + " ERROR: No resources");
        return;
    }
    
    rtp->setSessionId(id);
    rtp->setRmt(rmtIp, rmtPort, rmtVideoPort, peerIdx);

    _sessions[id] = rtp;
    
    static int workerIdx = 0;
    std::string wname = formatStr("RtpWorker_%d", workerIdx++ % 4);
    rtp->setWorkerName(wname);
    addHandler(wname, rtp);

    std::string resp = header + " OK " + locIp + " " + std::to_string(locPort) + " " + std::to_string(locVideoPort);
    sendResponse(ip, port, resp);
    
    printf("[ADD] ID=%s Rmt=%s:%d VideoRmt=%d -> Assigned Loc=%s:%d Worker=%s\n", 
           id.c_str(), rmtIp.c_str(), rmtPort, rmtVideoPort, locIp.c_str(), locPort, wname.c_str());
}

void CmpServer::processRemove(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header) {
    if (tokens.size() < 7) return;
    std::string id = tokens[6];

    PAutoLock lock(_mutex);
    auto it = _sessions.find(id);
    if (it != _sessions.end()) {
        PRtpTrans* rtp = it->second;
        
        // Clean up handler from worker
        delHandler(rtp->getWorkerName(), rtp);
        rtp->reset();
        
        freeResource(rtp);
        _sessions.erase(it);
        sendResponse(ip, port, header + " OK");
        printf("[REMOVE] ID=%s\n", id.c_str());
    } else {
        sendResponse(ip, port, header + " ERROR: ID not found");
    }
}

void CmpServer::processModify(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header) {
    if (tokens.size() < 9) return;
    std::string id = tokens[6];
    std::string rmtIp = tokens[7];
    int rmtPort = std::stoi(tokens[8]);

    PAutoLock lock(_mutex);
    auto it = _sessions.find(id);
    if (it != _sessions.end()) {
        it->second->setRmt(rmtIp, rmtPort);
        sendResponse(ip, port, header + " OK");
        printf("[MODIFY] ID=%s Rmt=%s:%d\n", id.c_str(), rmtIp.c_str(), rmtPort);
    } else {
        sendResponse(ip, port, header + " ERROR: ID not found");
    }
}

void CmpServer::processAlive(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header) {
    sendResponse(ip, port, header + " OK");
}

void CmpServer::processAddGroup(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header) {
    // CMD: addgroup 0 <groupId> <count> <mem1:prio1> ...
    if (tokens.size() < 9) return;
    std::string groupId = tokens[6];
    int count = std::stoi(tokens[7]);
    
    std::map<std::string, int> priorities;
    for (int i=0; i<count; ++i) {
        if (8+i < tokens.size()) {
            std::string token = tokens[8+i];
            size_t pos = token.find(':');
            if (pos != std::string::npos) {
                std::string id = token.substr(0, pos);
                int prio = std::stoi(token.substr(pos+1));
                priorities[id] = prio;
            }
        }
    }

    PAutoLock lock(_mutex);
    if (_groups.find(groupId) != _groups.end()) {
        PRtpTrans* rtp = _sessions[groupId]; 
        McpttGroup* group = _groups[groupId];
        if (group) {
             group->updatePriorities(priorities);
        }
        
        if (rtp) {
             std::string locIp = _rtpIp;
             int locPort = rtp->getLocalPort();
             std::string resp = header + " OK " + locIp + " " + std::to_string(locPort);
             sendResponse(ip, port, resp);
             return;
        }
    }
    
    std::string locIp = _rtpIp;
    int locPort = 0;
    int locVideoPort = 0;
    
    PRtpTrans* rtp = allocResource(locIp, locPort, locVideoPort);
    if (!rtp) {
        sendResponse(ip, port, header + " ERROR: No resources");
        return;
    }
    rtp->setSessionId(groupId);
    _sessions[groupId] = rtp;

    static int workerIdx = 0;
    std::string wname = formatStr("RtpWorker_%d", workerIdx++ % 4);
    rtp->setWorkerName(wname);
    
    McpttGroup* group = new McpttGroup(groupId);
    group->setSharedSession(rtp);
    group->updatePriorities(priorities); // Set priorities
    
    // [DTMF PTT]
    if (_enableDtmfPtt) {
        group->setDtmfConfig(true, _dtmfPushDigit, _dtmfReleaseDigit);
    }
    
    rtp->setGroup(group);
    
    addHandler(wname, rtp);
    
    _groups[groupId] = group;
    
    std::string resp = header + " OK " + locIp + " " + std::to_string(locPort);
    sendResponse(ip, port, resp);
    printf("[ADD_GROUP] ID=%s SharedPort=%d Worker=%s Members=%d\n", groupId.c_str(), locPort, wname.c_str(), count);
}

void CmpServer::processModifyGroup(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header) {
    if (tokens.size() < 9) return;
    std::string groupId = tokens[6];
    int count = std::stoi(tokens[7]);
    
    std::map<std::string, int> priorities;
    for (int i=0; i<count; ++i) {
        if (8+i < tokens.size()) {
            std::string token = tokens[8+i];
            size_t pos = token.find(':');
            if (pos != std::string::npos) {
                std::string id = token.substr(0, pos);
                int prio = std::stoi(token.substr(pos+1));
                priorities[id] = prio;
            }
        }
    }
    
    PAutoLock lock(_mutex);
    auto it = _groups.find(groupId);
    if (it != _groups.end()) {
        it->second->updatePriorities(priorities);
        sendResponse(ip, port, header + " OK");
        printf("[MODIFY_GROUP] ID=%s Members=%d\n", groupId.c_str(), count);
    } else {
        sendResponse(ip, port, header + " ERROR: Group not found");
    }
}

void CmpServer::processRemoveGroup(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header) {
    if (tokens.size() < 7) return;
    std::string groupId = tokens[6];
    
    PAutoLock lock(_mutex);
    auto it = _groups.find(groupId);
    if (it != _groups.end()) {
        McpttGroup* group = it->second;
        delete group;
        _groups.erase(it);
        
        // Remove Shared Session
        auto itSess = _sessions.find(groupId);
        if (itSess != _sessions.end()) {
            PRtpTrans* rtp = itSess->second;
            delHandler(rtp->getWorkerName(), rtp);
            rtp->reset();
            freeResource(rtp);
            _sessions.erase(itSess);
        }

        sendResponse(ip, port, header + " OK");
        printf("[REMOVE_GROUP] ID=%s\n", groupId.c_str());
    } else {
        sendResponse(ip, port, header + " ERROR: Group not found");
    }
}



void CmpServer::processLeaveGroup(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header) {
    if (tokens.size() < 8) return;
    std::string groupId = tokens[6];
    std::string sessionId = tokens[7];
    
    PAutoLock lock(_mutex);
    auto groupIt = _groups.find(groupId);
    if (groupIt != _groups.end()) {
        groupIt->second->removeMember(sessionId);
        sendResponse(ip, port, header + " OK");
        printf("[LEAVE_GROUP] Group=%s Session=%s\n", groupId.c_str(), sessionId.c_str());
    } else {
        sendResponse(ip, port, header + " ERROR: Group not found");
    }
}
    


void CmpServer::processJoinGroup(const std::vector<std::string>& tokens, const std::string& ip, int port, const std::string& header) {
    // Debug: Print all tokens
    std::string allTokens;
    for (auto& t : tokens) allTokens += "[" + t + "] ";
    printf("[CMD_DEBUG] JoinGroup Tokens: %s\n", allTokens.c_str());

    // Expected format: CSP_MAIN <callId> CMP_MAIN 0 joingroup <groupId> <sessionId> <userIp> <userPort>
    if (tokens.size() < 10) {
        printf("[CMD_ERROR] JoinGroup Token Count %d < 10. Ignoring.\n", (int)tokens.size());
        sendResponse(ip, port, header + " ERROR: Invalid Args");
        return;
    }
    std::string groupId = tokens[6];
    std::string sessionId = tokens[7];
    std::string userIp = tokens[8];
    int userPort = std::stoi(tokens[9]);

    PAutoLock lock(_mutex);
    auto groupIt = _groups.find(groupId);
    if (groupIt == _groups.end()) {
        sendResponse(ip, port, header + " ERROR: Group not found");
        return;
    }
    printf("0. [JOIN_GROUP] Group=%s Session=%s Peer=%s:%d\n", groupId.c_str(), sessionId.c_str(), userIp.c_str(), userPort);

    groupIt->second->addMember(sessionId, userIp, userPort);

    sendResponse(ip, port, header + " OK");
    printf("1. [JOIN_GROUP] Group=%s Session=%s Peer=%s:%d\n", groupId.c_str(), sessionId.c_str(), userIp.c_str(), userPort);
}    


void CmpServer::sendResponse(const std::string& ip, int port, const std::string& msg) {
    // Send back to origin (which is CSP Port)
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);

    sendto(_udpFd, msg.c_str(), msg.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
}



// ... (skip to loadConfig)

void CmpServer::loadConfig() {
    FILE* fp = fopen(_configFile.c_str(), "r");
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
                if (strcmp(key, "EnableDtmfPtt") == 0) _enableDtmfPtt = atoi(val);
                if (strcmp(key, "DtmfPushDigit") == 0) _dtmfPushDigit = val;
                if (strcmp(key, "DtmfReleaseDigit") == 0) _dtmfReleaseDigit = val;
                // CspIp/Port removed from config by user request
            }
        }
        fclose(fp);
    }
    printf("Config: RtpStartPort=%d, RtpPoolSize=%d, RtpIp=%s, ServerIp=%s, ServerPort=%d, DtmfPtt=%d Push=%s Rel=%s\n", 
           _rtpStartPort, _rtpPoolSize, _rtpIp.c_str(), _serverIp.c_str(), _serverPort, 
           _enableDtmfPtt, _dtmfPushDigit.c_str(), _dtmfReleaseDigit.c_str());
}

void CmpServer::initResourcePool() {
    int currentPort = _rtpStartPort;
    for (int i = 0; i < _rtpPoolSize; ++i) {
        std::string name = formatStr("InActiveRtp_%d", i);
        PRtpTrans* rtp = new PRtpTrans(name);
        
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

PRtpTrans* CmpServer::allocResource(std::string& rtpIp, int& rtpPort, int& videoPort) {
    if (_freeResources.empty()) return NULL;
    
    PRtpTrans* rtp = _freeResources.back();
    _freeResources.pop_back();
    
    rtpIp = _rtpIp; 
    rtpPort = rtp->getLocalPort(); 
    videoPort = rtp->getLocalVideoPort();
    
    return rtp;
}

void CmpServer::freeResource(PRtpTrans* rtp) {
    _freeResources.push_back(rtp);
}
