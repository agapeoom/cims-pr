/*
 * Group Map Source
 */

#include "GroupMap.h"
#include "Directory.h"
#include "Log.h"
#include "CmpClient.h"

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
    Clear();
    return ReadDir( pszDirName );
}

/**
 * @brief Read directory and parse XML files
 */
bool CGroupMap::ReadDir( const char *pszDirName ) {
    FILE_LIST clsFileList;
    FILE_LIST::iterator itFL;
    std::string strFileName;

    if ( CDirectory::FileList( pszDirName, clsFileList ) == false ) {
        CLog::Print( LOG_ERROR, "GroupMap ReadDir(%s) failed", pszDirName );
        return false;
    }

    for ( itFL = clsFileList.begin(); itFL != clsFileList.end(); ++itFL ) {
        CXmlGroup clsXml;

        strFileName = pszDirName;
        CDirectory::AppendName( strFileName, itFL->c_str() );

        if ( clsXml.Parse( strFileName.c_str() ) ) {
            Insert( clsXml );
            CLog::Print( LOG_INFO, "GroupMap Loaded Group(%s: %s)", clsXml.m_strId.c_str(), clsXml.m_strName.c_str() );
        }
    }

    return true;
}

/**
 * @brief Insert group into map
 */
void CGroupMap::Insert( CXmlGroup &clsGroup ) {
    m_clsMutex.acquire();
    m_clsMap[clsGroup.m_strId] = clsGroup;
    m_clsMutex.release();

    gclsCmpClient.AddGroup(clsGroup.m_strId);
}

/**
 * @brief Select group by ID
 */
bool CGroupMap::Select( const char *pszGroupId, CXmlGroup &clsGroup ) {
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
