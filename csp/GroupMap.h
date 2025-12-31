/*
 * Group Map Header
 */

#ifndef _GROUP_MAP_H_
#define _GROUP_MAP_H_

#include "XmlGroup.h"
#include <map>

typedef std::map<std::string, CXmlGroup> GROUP_MAP;

/**
 * @ingroup CspServer
 * @brief Singleton class to manage Group definitions
 */
class CGroupMap {
public:
    CGroupMap();
    ~CGroupMap();

    /** Load all groups from directory */
    bool Load( const char *pszDirName );

    /** Insert a group */
    void Insert( CXmlGroup &clsGroup );

    /** Select a group by ID */
    bool Select( const char *pszGroupId, CXmlGroup &clsGroup );

    /** Clear all groups */
    void Clear();

private:
    GROUP_MAP m_clsMap;
    CSipMutex m_clsMutex;

    bool ReadDir( const char *pszDirName );
};

extern CGroupMap gclsGroupMap;

#endif
