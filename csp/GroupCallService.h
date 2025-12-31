/*
 * Group Call Service Header
 */

#ifndef _GROUP_CALL_SERVICE_H_
#define _GROUP_CALL_SERVICE_H_

#include <string>

class CSipCallRtp;
class CSipCallRoute;

/**
 * @ingroup CspServer
 * @brief Service class to handle Group Calls
 */
class CGroupCallService {
public:
    CGroupCallService();
    ~CGroupCallService();

    /**
     * @brief Process a call to a group
     * @param pszGroupId The group ID being called
     * @param pszCallerInfo Caller information (From)
     * @param pszCallId Incoming Call-ID
     * @param pclsRtp RTP info of caller
     * @param pclsRoute Route info
     * @return true if group call initiated, false if group not found or error
     */
    bool ProcessGroupCall( const char *pszGroupId, const char *pszCallerInfo, const char *pszCallId,
                           CSipCallRtp *pclsRtp, CSipCallRoute *pclsRoute );
};

extern CGroupCallService gclsGroupCallService;

#endif
