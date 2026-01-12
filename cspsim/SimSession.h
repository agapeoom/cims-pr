#ifndef _SIM_SESSION_H_
#define _SIM_SESSION_H_

#include "SipClient.h"
#include "SipClientSetup.h"
#include "RtpThread.h"

// Forward declaration
class SimSession;

// Customized SipClient for session
class SessionSipClient : public CSipClient {
public:
    SessionSipClient(SimSession* owner) : m_pOwner(owner) {}
    virtual ~SessionSipClient() {}
    
    // Override callbacks to delegate to SimSession if needed,
    // or just use basic SipClient functionality but update SimSession state
    virtual void EventRegister( CSipServerInfo * pclsInfo, int iStatus );
    virtual void EventCallStart( const char * pszCallId, CSipCallRtp * pclsRtp );
    virtual void EventCallEnd( const char * pszCallId, int iSipStatus );

    SimSession* m_pOwner;
};

class SimSession {
public:
    SimSession(int id, std::string user, std::string domain, std::string pwd, 
               std::string serverIp, int serverPort, std::string localIp, int localPort,
               bool bPttMode);
    ~SimSession();

    bool Start();
    void Stop();
    void Update(); // Main loop tick if needed

    // Actions
    void StartCall(const std::string& strTargetUser = ""); 
    void StopCall();
    void SendPttRequest();
    void SendPttRelease();

public:
    int m_iId;
    std::string m_strUser;
    std::string m_strDomain;
    std::string m_strPwd;
    std::string m_strServerIp;
    int m_iServerPort;
    std::string m_strLocalIp;
    int m_iLocalPort;
    bool m_bPttMode;

    CSipUserAgent m_clsUserAgent;
    CSipStackSetup m_clsSetup;
    CSipServerInfo m_clsServerInfo;
    SessionSipClient* m_pSipClient; // Derived class to handle callbacks
    
    // Each session needs its own RTP thread?
    // RtpThread in original code was a singleton 'gclsRtpThread'. 
    // We need to instantiate it per session.
    CRtpThread m_clsRtpThread; 
    
    std::string m_strInviteId; // Active Call ID
    bool m_bRegistered;
    bool m_bInCall;
};

#endif
