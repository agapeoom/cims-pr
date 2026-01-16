/*
 * Group XML Parser Header
 */

#ifndef _XML_GROUP_H_
#define _XML_GROUP_H_

#include <string>
#include <vector>
#include <map>
#include "SipMutex.h"

/**
 * @ingroup CspServer
 * @brief Group Information Class
 */
class CXmlGroup {
public:
    CXmlGroup();
    ~CXmlGroup();

    /** Group ID */
    std::string m_strId;

    /** Group Name */
    std::string m_strName;

    /** Member Structure */
    struct CGroupMember {
        std::string m_strId;
        int m_iPriority;

        CGroupMember() : m_iPriority(0) {}
        CGroupMember(std::string id, int prio) : m_strId(id), m_iPriority(prio) {}
    };

    /** Member List (List of Group Members) */
    std::vector<CGroupMember> m_vecMembers;

    /** Parsing method */
    bool Parse( const char *pszFileName );
    void Clear();
};

#endif
