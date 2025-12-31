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

    /** Member List (List of User IDs) */
    std::vector<std::string> m_vecMembers;

    /** Parsing method */
    bool Parse( const char *pszFileName );
    void Clear();
};

#endif
