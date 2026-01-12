#include "SimSession.h"
#include "Log.h"

// Forward declaration of log callback (using console for now, defined in CspsimMain or here)
// For simplicity, we can rely on standard stdout or a shared logger.
// SipClientMain had CConsoleLog. We might need something similar if we want logs.

SimSession::SimSession(int id, std::string user, std::string domain, std::string pwd, 
           std::string serverIp, int serverPort, std::string localIp, int localPort,
           bool bPttMode)
    : m_iId(id), m_strUser(user), m_strDomain(domain), m_strPwd(pwd),
      m_strServerIp(serverIp), m_iServerPort(serverPort), 
      m_strLocalIp(localIp), m_iLocalPort(localPort), m_bPttMode(bPttMode),
      m_bRegistered(false), m_bInCall(false)
{
    m_pSipClient = new SessionSipClient(this);
    // Bind SimSession members to SipClient
    m_pSipClient->m_bPttMode = m_bPttMode;
    m_pSipClient->m_pRtpThread = &m_clsRtpThread;
    m_pSipClient->m_pInviteId = &m_strInviteId;
    m_pSipClient->m_pUserAgent = &m_clsUserAgent;

    // Initialize Server Info
    m_clsServerInfo.m_strIp = m_strServerIp;
    m_clsServerInfo.m_strDomain = m_strDomain;
    m_clsServerInfo.m_strUserId = m_strUser;
    m_clsServerInfo.m_strPassWord = m_strPwd;
    m_clsServerInfo.m_eTransport = E_SIP_UDP;
    m_clsServerInfo.m_iPort = m_iServerPort;
    m_clsServerInfo.m_iLoginTimeout = 600;

    // Initialize Setup
    m_clsSetup.m_iLocalUdpPort = m_iLocalPort;
    m_clsSetup.m_strLocalIp = m_strLocalIp;

    m_clsUserAgent.InsertRegisterInfo( m_clsServerInfo );
}

SimSession::~SimSession()
{
    Stop();
    if (m_pSipClient) delete m_pSipClient;
}

bool SimSession::Start()
{
    if( m_clsUserAgent.Start( m_clsSetup, m_pSipClient ) == false )
    {
        printf( "[Session %d] SIP stack start error\n", m_iId );
        return false;
    }

    if( m_clsRtpThread.Create() == false )
    {
        printf( "[Session %d] RTP thread create error\n", m_iId );
        return false;
    }

    printf("[Session %d] Started on %s:%d\n", m_iId, m_strLocalIp.c_str(), m_iLocalPort);
    return true;
}

void SimSession::Stop()
{
    if (m_strInviteId.empty() == false)
    {
        m_clsUserAgent.StopCall( m_strInviteId.c_str() );
    }

    m_clsRtpThread.Stop();
    m_clsUserAgent.Stop();
    m_clsUserAgent.Final();
}

void SimSession::Update()
{
    // Placeholder for tick logic if needed
}

void SimSession::StartCall(const std::string& strTargetUser)
{
    if (!m_strInviteId.empty()) return;

    CSipCallRtp clsRtp;
    CSipCallRoute clsRoute;

    clsRtp.m_strIp = m_clsSetup.m_strLocalIp;
    clsRtp.m_iPort = m_clsRtpThread.m_iPort;
    clsRtp.m_iCodec = 0;

    clsRoute.m_strDestIp = m_strServerIp;
    clsRoute.m_iDestPort = m_iServerPort;
    clsRoute.m_eTransport = E_SIP_UDP;

    std::string strDestObj;
    if (strTargetUser.empty()) {
        strDestObj = m_strServerIp; // As before, or maybe a default target
    } else {
        strDestObj = strTargetUser;
    }

    printf("[Session %d] Starting Call to %s...\n", m_iId, strDestObj.c_str());
    m_clsUserAgent.StartCall( m_strUser.c_str(), strDestObj.c_str(), &clsRtp, &clsRoute, m_strInviteId );
}

void SimSession::StopCall()
{
    if (!m_strInviteId.empty()) {
         m_clsUserAgent.StopCall( m_strInviteId.c_str() );
         m_clsRtpThread.Stop();
         m_strInviteId.clear();
    }
}

void SimSession::SendPttRequest()
{
     if (m_bPttMode) {
         printf("[Session %d] Sending Floor Request\n", m_iId);
         m_clsRtpThread.SendFloorControl(1); 
     }
}

void SimSession::SendPttRelease()
{
    if (m_bPttMode) {
         printf("[Session %d] Sending Floor Release\n", m_iId);
         m_clsRtpThread.SendFloorControl(4); 
     }
}

// SessionSipClient Callbacks (Delegate to base CSipClient or handle custom)
void SessionSipClient::EventRegister( CSipServerInfo * pclsInfo, int iStatus )
{
    printf( "[Session %d] EventRegister(%s) : %d\n", m_pOwner->m_iId, pclsInfo->m_strUserId.c_str(), iStatus );
    if (iStatus == 200) m_pOwner->m_bRegistered = true;
}
void SessionSipClient::EventCallStart( const char * pszCallId, CSipCallRtp * pclsRtp )
{
    // Base implementation handles RTP start, but we can add log
    CSipClient::EventCallStart(pszCallId, pclsRtp);
    m_pOwner->m_bInCall = true;
}
void SessionSipClient::EventCallEnd( const char * pszCallId, int iSipStatus )
{
    CSipClient::EventCallEnd(pszCallId, iSipStatus);
    m_pOwner->m_bInCall = false;
    m_pOwner->m_strInviteId.clear();
}
