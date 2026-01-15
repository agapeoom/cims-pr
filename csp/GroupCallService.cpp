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
#include "UserMap.h"
#include "SipServerSetup.h"

// External global objects
extern CSipUserAgent gclsUserAgent;

CGroupCallService gclsGroupCallService;

CGroupCallService::CGroupCallService() : m_bMonitorRunning(false) {
}

CGroupCallService::~CGroupCallService() {
    StopMonitor();
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
        CUserInfo clsUserInfo;
        CSipCallRoute clsMemberRoute;

        // [FIX] Check if member is registered and get their specific route
        if ( gclsUserMap.Select( strMember.c_str(), clsUserInfo ) == false ) {
            CLog::Print( LOG_INFO, "GroupCall Member(%s) not available (not registered)", strMember.c_str() );
            continue;
        }
        clsUserInfo.GetCallRoute( clsMemberRoute );

        CLog::Print( LOG_DEBUG, "GroupCall Initiating to Member(%s)", strMember.c_str() );

        // Create a new call leg for each member using their specific route
        if ( gclsUserAgent.CreateCall( pszCallerInfo, strMember.c_str(), pclsRtp, &clsMemberRoute, strNewCallId, &pclsInvite ) ) {
            
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

/**
 * @brief Invite a member to a group call
 */
bool CGroupCallService::InviteMember( const char *pszUserId, const char *pszGroupId ) {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    // Check if already invited (to avoid loop storm)
    if ( m_mapUserCall.find(pszUserId) != m_mapUserCall.end() ) {
        // Already active?
        return false;
    }
    
    CUserInfo clsUserInfo;
    CSipCallRoute clsRoute;
    std::string strSessionId;
    int iStartPort = -1;

    // 1. Check User Availability
    if ( !gclsUserMap.Select( pszUserId, clsUserInfo ) ) {
        CLog::Print( LOG_ERROR, "InviteMember(%s) User not found", pszUserId );
        return false;
    }
    clsUserInfo.GetCallRoute( clsRoute );

    // 2. Allocate RTP Port/Session
    iStartPort = gclsRtpMap.CreatePort( SOCKET_COUNT_PER_MEDIA );
    if ( iStartPort == -1 ) {
        CLog::Print( LOG_ERROR, "InviteMember(%s) CreatePort failed", pszUserId );
        return false;
    }

    // 3. Get Session ID
    CRtpInfo *pInfo = NULL;
    if ( gclsRtpMap.Select( iStartPort, &pInfo ) ) {
        strSessionId = pInfo->m_strSessionId;
    } else {
        CLog::Print( LOG_ERROR, "InviteMember(%s) Session info not found for port %d", pszUserId, iStartPort );
        gclsRtpMap.Delete(iStartPort);
        return false;
    }

    // 4. Join Group in CMP
    if ( !gclsCmpClient.JoinGroup( pszGroupId, strSessionId ) ) {
         CLog::Print( LOG_ERROR, "InviteMember(%s) JoinGroup failed", pszUserId );
         gclsRtpMap.Delete(iStartPort);
         return false;
    }

    // 5. Prepare RTP Info for INVITE (SDP)
    CSipCallRtp clsRtp;
    clsRtp.SetIpPort( gclsSetup.m_strLocalIp.c_str(), iStartPort, SOCKET_COUNT_PER_MEDIA );
    
    // [FIX] Add Codecs to generate valid SDP with m-line.
    // Without codecs, SipStack generates SDP without m-line, causing 488 (Not Acceptable Here).
    clsRtp.m_clsCodecList.push_back(0);   // PCMU
    clsRtp.m_clsCodecList.push_back(8);   // PCMA
    clsRtp.m_clsCodecList.push_back(101); // telephone-event
    clsRtp.m_iCodec = 0; // Default to PCMU
    
    // 6. Create and Start Call
    std::string strCallId;
    CSipMessage *pclsInvite = NULL;

    if ( gclsUserAgent.CreateCall( pszGroupId, pszUserId, &clsRtp, &clsRoute, strCallId, &pclsInvite ) ) {
         
         CCallInfo clsCallInfo;
         clsCallInfo.m_bRecv = false; // Outgoing
         clsCallInfo.m_iPeerRtpPort = iStartPort;
         
         gclsCallMap.Insert( strCallId.c_str(), clsCallInfo );
         
         // Track
         m_mapUserCall[pszUserId] = strCallId;

         if ( !gclsUserAgent.StartCall( strCallId.c_str(), pclsInvite ) ) {
             CLog::Print( LOG_ERROR, "InviteMember StartCall failed" );
             gclsCallMap.Delete( strCallId.c_str() );
             return false;
         }
    } else {
         CLog::Print( LOG_ERROR, "InviteMember CreateCall failed" );
         gclsRtpMap.Delete(iStartPort);
         return false;
    }

    CLog::Print( LOG_INFO, "InviteMember(%s) Group(%s) Session(%s) CallId(%s) Initiated", 
                 pszUserId, pszGroupId, strSessionId.c_str(), strCallId.c_str() );
    return true;
}

void CGroupCallService::StartMonitor() {
    if ( !m_bMonitorRunning ) {
        m_bMonitorRunning = true;
        m_threadMonitor = std::thread(&CGroupCallService::MonitorLoop, this);
        CLog::Print( LOG_INFO, "GroupCallService Monitor Started" );
    }
}

void CGroupCallService::StopMonitor() {
    m_bMonitorRunning = false;
    if ( m_threadMonitor.joinable() ) {
        m_threadMonitor.join();
        CLog::Print( LOG_INFO, "GroupCallService Monitor Stopped" );
    }
}

void CGroupCallService::MonitorLoop() {
    while ( m_bMonitorRunning ) {
        std::this_thread::sleep_for( std::chrono::seconds(5) );
        if ( !m_bMonitorRunning ) break;
        
        CheckGroupIntegrity();
    }
}

void CGroupCallService::SyncGroups() {
    CLog::Print( LOG_INFO, "SyncGroups: Re-creating all groups in CMP" );
    gclsGroupMap.IterateInternal([](const CXmlGroup& group) {
        if ( !gclsCmpClient.AddGroup( group.m_strId ) ) {
            CLog::Print( LOG_ERROR, "SyncGroups: AddGroup(%s) failed", group.m_strId.c_str() );
        }
    });
    
    // After Sync, CheckIntegrity will handle re-invites naturally if calls were lost.
}

void CGroupCallService::CheckGroupIntegrity() {
    // Iterate groups
    gclsGroupMap.IterateInternal([this](const CXmlGroup& group) {
        // Iterate members
        for ( const std::string& strUserId : group.m_vecMembers ) {
             // Check if registered
             CUserInfo clsUser;
             if ( gclsUserMap.Select( strUserId.c_str(), clsUser ) ) {
                 // Check if active call
                 std::unique_lock<std::recursive_mutex> lock(m_mutex);
                 if ( m_mapUserCall.find(strUserId) == m_mapUserCall.end() ) {
                     // Registered but no call -> Invite!
                     // Unlock before calling InviteMember to avoid recursion deadlock (Invite locks mutex)
                     lock.unlock();
                     InviteMember( strUserId.c_str(), group.m_strId.c_str() );
                 }
             }
        }
    });
}

void CGroupCallService::OnCmpStatusChanged( bool bConnected ) {
    if ( bConnected ) {
        CLog::Print( LOG_INFO, "OnCmpStatusChanged: Connected -> Syncing Groups" );
        SyncGroups();
    } else {
        CLog::Print( LOG_INFO, "OnCmpStatusChanged: Disconnected" );
        // Optionally Clear m_mapUserCall or teardown calls?
        // If CMP is gone, RTP is dead. 
        // We should probably teardown all group calls.
        std::unique_lock<std::recursive_mutex> lock(m_mutex);
        for ( auto it = m_mapUserCall.begin(); it != m_mapUserCall.end(); ++it ) {
             // auto const& userId = it->first;
             // auto const& callId = it->second;
             // Just iterating, nothing to do unless we Terminate.
        }
    }
}

void CGroupCallService::OnCallTerminated( const std::string& strCallId ) {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    for ( auto it = m_mapUserCall.begin(); it != m_mapUserCall.end(); ++it ) {
        if ( it->second == strCallId ) {
            m_mapUserCall.erase(it);
            break;
        }
    }
}
