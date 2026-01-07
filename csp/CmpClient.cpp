#include "CmpClient.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include "Log.h"

CCmpClient::CCmpClient() : m_iSocket(-1), m_iCmpPort(0) {
}

CCmpClient::~CCmpClient() {
    if (m_iSocket != -1) {
        close(m_iSocket);
    }
}

CCmpClient& CCmpClient::GetInstance() {
    static CCmpClient instance;
    return instance;
}

bool CCmpClient::Init(const std::string& strCmpIp, int iCmpPort) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_strCmpIp = strCmpIp;
    m_iCmpPort = iCmpPort;

    if (m_iSocket == -1) {
        m_iSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_iSocket < 0) {
            CLog::Print(LOG_ERROR, "CmpClient::Init socket error");
            return false;
        }
        
        // Set timeout
        struct timeval tv;
        tv.tv_sec = 1; // 1 second timeout
        tv.tv_usec = 0;
        setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    }
    return true;
}

bool CCmpClient::SendCommand(const std::string& strCmd, std::string& strResponse) {
    if (m_iSocket == -1) return false;

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(m_iCmpPort);
    servaddr.sin_addr.s_addr = inet_addr(m_strCmpIp.c_str());

    ssize_t n = sendto(m_iSocket, strCmd.c_str(), strCmd.length(), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr));
    if (n < 0) {
        CLog::Print(LOG_ERROR, "CmpClient::SendCommand sendto error: %s", strCmd.c_str());
        return false;
    }

    char buffer[1024];
    socklen_t len = sizeof(servaddr);
    n = recvfrom(m_iSocket, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&servaddr, &len);
    if (n < 0) {
        CLog::Print(LOG_ERROR, "CmpClient::SendCommand recvfrom timeout/error: %s", strCmd.c_str());
        return false;
    }
    buffer[n] = '\0';
    strResponse = buffer;
    
    // Check for OK
    if (strResponse.find("OK") != 0) {
        CLog::Print(LOG_ERROR, "CmpClient::SendCommand Error Response: %s -> %s", strCmd.c_str(), strResponse.c_str());
        return false;
    }

    return true;
}

bool CCmpClient::AddSession(const std::string& strSessionId, std::string& strLocalIp, int& iLocalPort, int& iLocalVideoPort) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Initial ADD without remote info (placeholders)
    // ADD <id> <rmt_ip> <rmt_port> [rmt_video_port]
    // Use 0.0.0.0 and 0 for placeholders
    std::string strCmd = "add " + strSessionId + " 0.0.0.0 0 0";
    std::string strResp;
    
    if (!SendCommand(strCmd, strResp)) return false;

    // Parse OK <loc_ip> <loc_port> <loc_video_port>
    std::stringstream ss(strResp);
    std::string ok;
    ss >> ok >> strLocalIp >> iLocalPort >> iLocalVideoPort;
    return true;
}

bool CCmpClient::UpdateSession(const std::string& strSessionId, const std::string& strRmtIp, int iRmtPort, int iRmtVideoPort, std::string& strLocalIp, int& iLocalPort) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string strCmd = "add " + strSessionId + " " + strRmtIp + " " + std::to_string(iRmtPort) + " " + std::to_string(iRmtVideoPort);
    std::string strResp;
    
    if (!SendCommand(strCmd, strResp)) return false;
    
    int iLocalVideoPort;
    std::stringstream ss(strResp);
    std::string ok;
    ss >> ok >> strLocalIp >> iLocalPort >> iLocalVideoPort;
    return true;
}

bool CCmpClient::RemoveSession(const std::string& strSessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string strCmd = "remove " + strSessionId;
    std::string strResp;
    return SendCommand(strCmd, strResp);
}

bool CCmpClient::AddGroup(const std::string& strGroupId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string strCmd = "addgroup " + strGroupId;
    std::string strResp;
    return SendCommand(strCmd, strResp);
}

bool CCmpClient::JoinGroup(const std::string& strGroupId, const std::string& strSessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string strCmd = "joingroup " + strGroupId + " " + strSessionId;
    std::string strResp;
    return SendCommand(strCmd, strResp);
}

bool CCmpClient::LeaveGroup(const std::string& strGroupId, const std::string& strSessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string strCmd = "leavegroup " + strGroupId + " " + strSessionId;
    std::string strResp;
    return SendCommand(strCmd, strResp);
}
