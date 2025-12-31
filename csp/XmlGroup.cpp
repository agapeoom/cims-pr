/*
 * Group XML Parser Source
 */

#include "XmlGroup.h"
#include "XmlElement.h"
#include "Log.h"

CXmlGroup::CXmlGroup() {
}

CXmlGroup::~CXmlGroup() {
}

/**
 * @brief Parse Group XML file
 * @param pszFileName XML File Path
 * @return true if success, false otherwise
 */
bool CXmlGroup::Parse( const char *pszFileName ) {
    CXmlElement clsXml;

    Clear();

    if ( clsXml.ParseFile( pszFileName ) == false ) return false;

    clsXml.SelectElementData( "Id", m_strId );
    clsXml.SelectElementData( "Name", m_strName );

    if ( m_strId.empty() ) return false;

    CXmlElement *pclsMemberList = clsXml.SelectElement( "MemberList" );
    if ( pclsMemberList ) {
        XML_ELEMENT_LIST clsList;
        if ( pclsMemberList->SelectElementList( "Member", clsList ) ) {
            XML_ELEMENT_LIST::iterator it;
            for ( it = clsList.begin(); it != clsList.end(); ++it ) {
                std::string strMember = it->GetData();
                if ( !strMember.empty() ) {
                    m_vecMembers.push_back( strMember );
                }
            }
        }
    }

    return true;
}

void CXmlGroup::Clear() {
    m_strId.clear();
    m_strName.clear();
    m_vecMembers.clear();
}
