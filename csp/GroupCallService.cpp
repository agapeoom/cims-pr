/*
 * Group Call Service Source
 */

#include "GroupCallService.h"
#include "GroupMap.h"
#include "SipServer.h"
#include "Log.h"
#include "SipUserAgent.h"
#include "CallMap.h"

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
