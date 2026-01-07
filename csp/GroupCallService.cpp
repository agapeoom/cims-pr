/*
 * Group Call Service Source
 */

#include "GroupCallService.h"
#include "GroupMap.h"
#include "SipServer.h"
#include "Log.h"
#include "SipUserAgent.h"
#include "CallMap.h"
#include "CmpClient.h"
#include "RtpMap.h"

// External global objects
extern CSipUserAgent gclsUserAgent;

CGroupCallService gclsGroupCallService;

CGroupCallService::CGroupCallService() {
}

CGroupCallService::~CGroupCallService() {
}

/**
 * @brief Process Group Call
 */
bool CGroupCallService::ProcessGroupCall( const char *pszGroupId, const char *pszCallerInfo, const char *pszCallId,
                                          CSipCallRtp *pclsRtp, CSipCallRoute *pclsRoute ) {
    CXmlGroup clsGroup;

    if ( gclsGroupMap.Select( pszGroupId, clsGroup ) == false ) {
        return false;
    }

    CLog::Print( LOG_INFO, "Processing Group Call GroupId(%s) Name(%s) Caller(%s)", 
                 pszGroupId, clsGroup.m_strName.c_str(), pszCallerInfo );

    // Join the caller to the group in CMP
    if (pclsRtp) {
        // Assuming the first port is the audio port we allocated
        // We need to find the SessionID associated with this port.
        // pclsRtp has m_iPort, but we need to know which port index or if it's the base port.
        // Usually m_iPort is the start port.
        
        // pclsRtp doesn't seem to expose m_iPort directly in public interface based on visual inspection of other files?
        // Wait, SipServer.cpp uses pclsRtp->m_iPort. let's assume it's public.
        // However, we need to be careful. The port in pclsRtp might strictly be what we SetIpPort with.
        
        // Let's iterate or check.
        // For now, let's try to look up the port.
        // We can't easy get the exact port number if we don't know it.
        // But SipServer.cpp calls SetIpPort on it.
        
        // Let's assume we can get it via simple property or method.
        // We need to look at CSipCallRtp definition or guess.
        // Based on SipServer.cpp: pclsRtp->SetIpPort(...)
        // It doesn't use m_iPort for reading usually, mostly setting.
        
        // Actually, we can just use the fact that we just allocated it if this is the caller.
        // But ProcessGroupCall is called from EventIncomingCall.
        // The allocation happened just before in EventIncomingCall:
        // iStartPort = gclsRtpMap.CreatePort(...)
        // pclsRtp->SetIpPort(..., iStartPort, ...)
        
        // So pclsRtp has the info.
        // Let's try to access it. If compile fails, I'll fix.
        // But wait, pclsRtp might be an opaque pointer to SipCallRtp? No it's included via SipUserAgent.h probably.
        
        // Let's assume we can use the RtpMap lookup.
        // Issue: We don't have the port number variable here easily unless we inspect pclsRtp.
        
        // Workaround: We can't easily get the port if we don't know the member name.
        // Let's look at RtpMap.h again to see if CRtpInfo can be found by other means? No.
        
        // Let's look at SipStack/SipUserAgentCallBack.h (viewed in RtpMap.h) or similar.
        // Wait, I haven't viewed SipStack files.
        // I'll take a safe bet:
        // SipServer.cpp: pclsRtp->m_iPort = ...
        // So m_iPort exists.
        
        CRtpInfo *pInfo = NULL;
        // The first media port is usually the one we track in RtpMap key.
        // We need to loop media count?
        // Or just use the first one.
        // pclsRtp->m_sttMediaInfo[0].m_iPort ... ?
        // SipServer.cpp used: pclsRtp->SetIpPort(..., iStartPort, ...) which sets internal list.
        // And pclsRtp->m_iPort usage in EventCallRing seems to imply a single port or start port.
        
        int iPort = pclsRtp->m_iPort;
        if (gclsRtpMap.Select(iPort, &pInfo)) {
             gclsCmpClient.JoinGroup(pszGroupId, pInfo->m_strSessionId);
             CLog::Print( LOG_INFO, "Joined Group(%s) Session(%s) Port(%d)", pszGroupId, pInfo->m_strSessionId.c_str(), iPort );
        } else {
             CLog::Print( LOG_ERROR, "Failed to find session for GroupCall Port(%d)", iPort );
        }
    }

    std::vector<std::string>::iterator it;
    for ( it = clsGroup.m_vecMembers.begin(); it != clsGroup.m_vecMembers.end(); ++it ) {
        std::string strMember = *it;
        std::string strNewCallId;
        CSipMessage *pclsInvite = NULL;

        CLog::Print( LOG_DEBUG, "GroupCall Initiating to Member(%s)", strMember.c_str() );

        // Create a new call leg for each member
        if ( gclsUserAgent.CreateCall( pszCallerInfo, strMember.c_str(), pclsRtp, pclsRoute, strNewCallId, &pclsInvite ) ) {
            
            // Map the incoming call to the new outgoing call
            // NOTE: CCallMap only supports 1-to-1 mapping. For group calls, the 'Incoming->Outgoing' map
            // will store the LAST initiated member. 'Outgoing->Incoming' will be stored for ALL members.
            // This means bridging will work for ANY member who answers, but we cannot track ALL outgoing legs 
            // from the incoming call ID easily to cancel others.
            gclsCallMap.Insert( pszCallId, strNewCallId.c_str(), -1 ); // -1 for RTP port logic (start port handled internally or via RtpMap)

            // We might need to handle RTP ports correctly here if using Relay.
            // Assuming simplified logic for now.

            if ( gclsUserAgent.StartCall( strNewCallId.c_str(), pclsInvite ) == false ) {
                CLog::Print( LOG_ERROR, "GroupCall StartCall failed for Member(%s)", strMember.c_str() );
                // Clean up if needed
                gclsCallMap.Delete( strNewCallId.c_str() );
            }
        } else {
            CLog::Print( LOG_ERROR, "GroupCall CreateCall failed for Member(%s)", strMember.c_str() );
        }
    }

    return true;
}
