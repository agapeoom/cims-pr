#include "CmpClient.h"
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>
#include "Log.h"

CCmpClient::CCmpClient() : m_iCmpPort(0), m_hSocket(-1), m_bKeepAliveRunning(false), m_bRecvRunning(false), m_iNextTransId(1), m_bConnected(false) {
}

CCmpClient::~CCmpClient() {
    m_bKeepAliveRunning = false;
    if (m_threadKeepAlive.joinable()) {
        m_threadKeepAlive.join();
    }
    
    m_bRecvRunning = false;
    if (m_hSocket != -1) {
        shutdown(m_hSocket, SHUT_RDWR);
        close(m_hSocket);
        m_hSocket = -1;
    }

    if (m_threadRecv.joinable()) {
        m_threadRecv.join();
    }
}

CCmpClient& CCmpClient::GetInstance() {
    static CCmpClient instance;
    return instance;
}

bool CCmpClient::Init(const std::string& strCmpIp, int iCmpPort, int iLocalPort) {
    // std::lock_guard<std::mutex> lock(m_mutex); // No global mutex needed for init if called once
    m_strCmpIp = strCmpIp;
    m_iCmpPort = iCmpPort;
    m_iLocalCmpPort = iLocalPort;

    if (m_hSocket == -1) {
        m_hSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_hSocket < 0) {
            CLog::Print(LOG_ERROR, "CmpClient::Init socket error");
            return false;
        }

        struct sockaddr_in localAddr;
        memset(&localAddr, 0, sizeof(localAddr));
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddr.sin_port = htons(m_iLocalCmpPort);

        if (bind(m_hSocket, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
            CLog::Print(LOG_ERROR, "CmpClient::Init bind error port=%d", m_iLocalCmpPort);
            close(m_hSocket);
            m_hSocket = -1;
            return false;
        }
        
        // Start Recv Thread
        m_bRecvRunning = true;
        m_threadRecv = std::thread(&CCmpClient::RecvLoop, this);
    }

    if (!m_bKeepAliveRunning) {
        m_bKeepAliveRunning = true;
        m_threadKeepAlive = std::thread(&CCmpClient::KeepAliveLoop, this);
    }

    return true;
}

// Protocol: TRANS_ID CSP_ID CSP_SESS_ID CMP_ID CMP_SESS_ID CMD PAYLOAD...
// Example: 1001 CSP_MAIN sess_1 CMP_MAIN 0 add 1.2.3.4 1000 0 0
// Response: 1001 CSP_MAIN sess_1 CMP_MAIN 0 OK ...
bool CCmpClient::SendRequestAndWait(const std::string& strPayload, std::string& strResponse) {
    if (m_hSocket == -1) return false;

    unsigned int transId;
    Transaction* pTrans = new Transaction();
    {
        std::lock_guard<std::mutex> lock(m_mutexTrans);
        transId = m_iNextTransId++;
        pTrans->id = transId;
        m_mapTransactions[transId] = pTrans;
    }

    // Construct Packet
    // Header: TRANS_ID
    std::string strPacket = std::to_string(transId) + " " + strPayload;

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(m_iCmpPort);
    servaddr.sin_addr.s_addr = inet_addr(m_strCmpIp.c_str());

    ssize_t n = sendto(m_hSocket, strPacket.c_str(), strPacket.length(), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr));
    if (n < 0) {
        CLog::Print(LOG_ERROR, "CmpClient::SendRequest command sendto error");
        std::lock_guard<std::mutex> lock(m_mutexTrans);
        m_mapTransactions.erase(transId);
        delete pTrans;
        return false;
    }

    // Wait
    // Wait
    bool bResult = false;
    {
        std::unique_lock<std::mutex> lock(pTrans->mutex);
        if (pTrans->cv.wait_for(lock, std::chrono::seconds(3), [&]{ return pTrans->bCompleted; })) {
            strResponse = pTrans->strResponse;
            bResult = true;
        } else {
            CLog::Print(LOG_ERROR, "CmpClient::SendRequest timeout");
            bResult = false;
        }
    } // lock unlocked here (Crucial for Deadlock prevention and preventing SegFault on delete)

    {
        std::lock_guard<std::mutex> mapLock(m_mutexTrans);
        m_mapTransactions.erase(transId);
    }
    delete pTrans;
    return bResult;
}

void CCmpClient::RecvLoop() {
    char buffer[4096];
    struct sockaddr_in cliaddr;
    socklen_t len;

    while (m_bRecvRunning) {
        len = sizeof(cliaddr);
        int n = recvfrom(m_hSocket, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&cliaddr, &len);
        if (n > 0) {
            buffer[n] = '\0';
            std::string strPacket = buffer;
            std::string strIp = inet_ntoa(cliaddr.sin_addr);
            int iPort = ntohs(cliaddr.sin_port);
            OnPacketReceived(strPacket, strIp, iPort);
        }
    }
}

void CCmpClient::OnPacketReceived(const std::string& strPacket, const std::string& strIp, int iPort) {
    // Parse TRANS_ID logic
    std::stringstream ss(strPacket);
    unsigned int transId;
    if (!(ss >> transId)) {
        CLog::Print(LOG_ERROR, "CmpClient::OnPacketReceived Invalid Packet: %s", strPacket.c_str());
        return;
    }
    
    // Remaining is Body
    // Reconstruct body strictly? Or just take what's left?
    // getline gets rest of line but leading space issue.
    // Let's assume response starts after TransID space.
    size_t pos = strPacket.find(' ');
    std::string strBody = (pos != std::string::npos) ? strPacket.substr(pos + 1) : "";

    std::lock_guard<std::mutex> lock(m_mutexTrans);
    auto it = m_mapTransactions.find(transId);
    if (it != m_mapTransactions.end()) {
        Transaction* pTrans = it->second;
        std::lock_guard<std::mutex> transLock(pTrans->mutex);
        pTrans->strResponse = strBody;
        pTrans->bCompleted = true;
        pTrans->cv.notify_one();
    } else {
        // Async Notification or unknown TransID
        // Just log for now as we don't handle async callbacks yet
        printf("CmpClient Async RX: %s\n", strPacket.c_str());
    }
}

// FORMAT: CSP_ID CSP_SESS_ID CMP_ID CMP_SESS_ID CMD ...
// Using defaults: CSP_MAIN, <sessId>, CMP_MAIN, 0

bool CCmpClient::AddSession(const std::string& strSessionId, std::string& strLocalIp, int& iLocalPort, int& iLocalVideoPort) {
    // OLD: add <id> ...
    // NEW: CSP_MAIN <sessId> CMP_MAIN 0 add ... (TransID added by SendRequest)
    
    std::string strPayload = "CSP_MAIN " + strSessionId + " CMP_MAIN 0 add " + strSessionId + " 0.0.0.0 0 0 0";
    std::string strResp;
    
    printf("CmpClient::AddSession: %s\n", strPayload.c_str()); // Debug

    if (!SendRequestAndWait(strPayload, strResp)) return false;

    // Response Body: CSP_MAIN <sessId> CMP_MAIN <cmpSess> OK <ip> <port> <vport>
    // Need to parse header first?
    // The response body handled by OnPacketReceived is everything after TransId.
    // So strResp = "CSP_MAIN sess_1 CMP_MAIN 0 OK locIp locPort locVPort"
    
    std::stringstream ss(strResp);
    std::string cspId, cspSess, cmpId, cmpSess, status;
    ss >> cspId >> cspSess >> cmpId >> cmpSess >> status;
    
    if (status != "OK") return false;
    
    ss >> strLocalIp >> iLocalPort >> iLocalVideoPort;
    return true;
}

bool CCmpClient::UpdateSession(const std::string& strSessionId, const std::string& strRmtIp, int iRmtPort, int iRmtVideoPort, int iPeerIdx, std::string& strLocalIp, int& iLocalPort) {
    std::string strPayload = "CSP_MAIN " + strSessionId + " CMP_MAIN 0 add " + strSessionId + " " + strRmtIp + " " + std::to_string(iRmtPort) + " " + std::to_string(iRmtVideoPort) + " " + std::to_string(iPeerIdx);
    std::string strResp;
    
    printf("CmpClient::UpdateSession: %s\n", strPayload.c_str());

    if (!SendRequestAndWait(strPayload, strResp)) return false;
    
    // Parse
    std::stringstream ss(strResp);
    std::string cspId, cspSess, cmpId, cmpSess, status;
    ss >> cspId >> cspSess >> cmpId >> cmpSess >> status;
    
    if (status != "OK") return false;
    // Assuming same return format
    int iLocalVideoPort;
    ss >> strLocalIp >> iLocalPort >> iLocalVideoPort;
    return true;
}

bool CCmpClient::RemoveSession(const std::string& strSessionId) {
    std::string strPayload = "CSP_MAIN " + strSessionId + " CMP_MAIN 0 remove " + strSessionId;
    std::string strResp;
    printf("CmpClient::RemoveSession: %s\n", strPayload.c_str());
    return SendRequestAndWait(strPayload, strResp);
}

bool CCmpClient::AddGroup(const std::string& strGroupId, const std::vector<CXmlGroup::CGroupMember>& vecMembers, std::string& strIp, int& iPort) {
    // Send 7 tokens header + GroupID + Members
    // Format: addgroup 0 <groupId> <count> <mem1:prio1> <mem2:prio2> ...
    
    std::string strPayload = "CSP_MAIN 0 CMP_MAIN 0 addgroup 0 " + strGroupId + " " + std::to_string(vecMembers.size());
    for (const auto& mem : vecMembers) {
        strPayload += " " + mem.m_strId + ":" + std::to_string(mem.m_iPriority);
    }
    
    std::string strResp;
    
    if (SendRequestAndWait(strPayload, strResp)) {
        // Parse: Scan for "OK"
        std::stringstream ss(strResp);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ' ')) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }

        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "OK") {
                if (i + 2 < tokens.size()) {
                    strIp = tokens[i+1];
                    iPort = std::stoi(tokens[i+2]);
                    CLog::Print(LOG_INFO, "CmpClient::AddGroup Success: %s:%d Members: %d", strIp.c_str(), iPort, (int)vecMembers.size());
                    return true;
                }
            }
        }
        CLog::Print(LOG_ERROR, "CmpClient::AddGroup Parse Fail: OK not found or args missing. Resp: %s", strResp.c_str());
    } else {
         CLog::Print(LOG_ERROR, "CmpClient::AddGroup SendRequest Failed");
    }
    return false;
}

bool CCmpClient::ModifyGroup(const std::string& strGroupId, const std::vector<CXmlGroup::CGroupMember>& vecMembers) {
    // Format: modifygroup 0 <groupId> <count> <mem1:prio1> <mem2:prio2> ...
    std::string strPayload = "CSP_MAIN 0 CMP_MAIN 0 modifygroup 0 " + strGroupId + " " + std::to_string(vecMembers.size());
    for (const auto& mem : vecMembers) {
        strPayload += " " + mem.m_strId + ":" + std::to_string(mem.m_iPriority);
    }
    
    std::string strResp;
    return SendRequestAndWait(strPayload, strResp);
}

bool CCmpClient::JoinGroup(const std::string& strGroupId, const std::string& strSessionId, const std::string& strIp, int iPort) {
    std::string strPayload = "CSP_MAIN " + strSessionId + " CMP_MAIN 0 joingroup " + strGroupId + " " + strSessionId + " " + strIp + " " + std::to_string(iPort);
    std::string strResp;
    return SendRequestAndWait(strPayload, strResp);
}

bool CCmpClient::LeaveGroup(const std::string& strGroupId, const std::string& strSessionId) {
    std::string strPayload = "CSP_MAIN " + strSessionId + " CMP_MAIN 0 leavegroup " + strGroupId + " " + strSessionId;
    std::string strResp;
    return SendRequestAndWait(strPayload, strResp);
}

bool CCmpClient::RemoveGroup(const std::string& strGroupId) {
    std::string strPayload = "CSP_MAIN 0 CMP_MAIN 0 removegroup " + strGroupId;
    std::string strResp;
    return SendRequestAndWait(strPayload, strResp);
}

void CCmpClient::KeepAliveLoop() {
    while (m_bKeepAliveRunning) {
        std::string strResp;
        bool bSuccess = SendRequestAndWait("CSP_MAIN 0 CMP_MAIN 0 ALIVE", strResp);
        
        // Simple success check. Could parse response for "OK" if needed.
        if (bSuccess) {
            if (!m_bConnected) {
                m_bConnected = true;
                if (m_fnConnectionCallback) {
                    m_fnConnectionCallback(true);
                }
                CLog::Print(LOG_INFO, "CMP Connected");
            }
        } else {
            if (m_bConnected) {
                m_bConnected = false;
                if (m_fnConnectionCallback) {
                    m_fnConnectionCallback(false);
                }
                CLog::Print(LOG_INFO, "CMP Disconnected");
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}
