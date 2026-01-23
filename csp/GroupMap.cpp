/*
 * Group Map Source
 */

#include "GroupMap.h"
#include "Directory.h"
#include "Log.h"
#include "CmpClient.h"
#include <set>

CGroupMap gclsGroupMap;

CGroupMap::CGroupMap() {
}

CGroupMap::~CGroupMap() {
    Clear();
}

/**
 * @brief Load groups from a directory
 */
bool CGroupMap::Load( const char *pszDirName ) {
    return ReadDir( pszDirName );
}

/**
 * @brief Read directory and parse JSON files
 */
bool CGroupMap::ReadDir( const char *pszDirName ) {
    FILE_LIST clsFileList;
    FILE_LIST::iterator itFL;
    std::string strFileName;
    std::set<std::string> setFoundIds;

    if ( CDirectory::FileList( pszDirName, clsFileList ) == false ) {
        CLog::Print( LOG_ERROR, "GroupMap ReadDir(%s) failed", pszDirName );
        return false;
    }

    for ( itFL = clsFileList.begin(); itFL != clsFileList.end(); ++itFL ) {
        CspPttGroup clsGroup;

        strFileName = pszDirName;
        CDirectory::AppendName( strFileName, itFL->c_str() );

        if ( clsGroup.load( strFileName.c_str() ) ) {
            Insert( clsGroup );
            setFoundIds.insert(clsGroup._id);
            CLog::Print( LOG_INFO, "GroupMap Loaded Group(%s: %s)", clsGroup._id.c_str(), clsGroup._name.c_str() );
        }
    }
    
    // Sync Logic: Remove groups not found in current directory scan
    m_clsMutex.acquire();
    for (auto it = m_clsMap.begin(); it != m_clsMap.end(); ) {
        if (setFoundIds.find(it->first) == setFoundIds.end()) {
            CLog::Print(LOG_INFO, "GroupMap Removed Group(%s)", it->first.c_str());
            m_clsMap.erase(it++);
        } else {
            ++it;
        }
    }
    m_clsMutex.release();

    return true;
}

/**
 * @brief Insert group into map
 */
void CGroupMap::Insert( CspPttGroup &clsGroup ) {
    m_clsMutex.acquire();
    m_clsMap[clsGroup._id] = clsGroup;
    m_clsMutex.release();
}


bool CGroupMap::Select(const char *pszGroupId, CspPttGroup &clsGroup ) {
    bool bRes = false;
    GROUP_MAP::iterator it;

    m_clsMutex.acquire();
    it = m_clsMap.find( pszGroupId );
    if ( it != m_clsMap.end() ) {
        clsGroup = it->second;
        bRes = true;
    }
    m_clsMutex.release();

    return bRes;
}

/**
 * @brief Clear map
 */
void CGroupMap::Clear() {
    m_clsMutex.acquire();
    m_clsMap.clear();
    m_clsMutex.release();
}

void CGroupMap::IterateInternal( std::function<void(const CspPttGroup&)> fnCallback ) {
    m_clsMutex.acquire();
    for ( auto const& [key, val] : m_clsMap ) {
        fnCallback(val);
    }
    m_clsMutex.release();
}

bool CGroupMap::FindGroupsByUser(std::string strUserId) {
    bool bRes = false;
    
    m_clsMutex.acquire();
    for (auto const& [key, group] : m_clsMap) {
        for (auto const& user : group._pusers) {
            if (user->_id == strUserId) {
                bRes = true; // Found at least one group
                // If we need to return the groups, we should change signature.
                // For now, matching the bool return type (just checks existence?).
                // User code snippet didn't specify behavior, but signature returns bool.
                break;
            }
        }
        if (bRes) break;
    }
    m_clsMutex.release();

    return bRes;
}