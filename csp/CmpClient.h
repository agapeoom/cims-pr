#ifndef __CMP_CLIENT_H__
#define __CMP_CLIENT_H__

#include <string>
#include <vector>
#include <mutex>
#include "SipStackDefine.h" // For Socket types if needed, or standard includes

class CCmpClient {
public:
    static CCmpClient& GetInstance();

    bool Init(const std::string& strCmpIp, int iCmpPort, int iLocalPort);
    
    // Returns assigned local IP/Port from CMP
    bool AddSession(const std::string& strSessionId, std::string& strLocalIp, int& iLocalPort, int& iLocalVideoPort);
    bool UpdateSession(const std::string& strSessionId, const std::string& strRmtIp, int iRmtPort, int iRmtVideoPort, std::string& strLocalIp, int& iLocalPort);
    bool RemoveSession(const std::string& strSessionId);

    bool AddGroup(const std::string& strGroupId);
    bool JoinGroup(const std::string& strGroupId, const std::string& strSessionId);
    bool LeaveGroup(const std::string& strGroupId, const std::string& strSessionId);

private:
    CCmpClient();
    ~CCmpClient();

    bool SendCommand(const std::string& strCmd, std::string& strResponse);

    std::string m_strCmpIp;
    int m_iCmpPort;
    int m_iSocket;
    std::mutex m_mutex;
};

#define gclsCmpClient CCmpClient::GetInstance()

#endif
