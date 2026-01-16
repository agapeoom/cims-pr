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
                // Check if it has child elements (New Format) or text only (Legacy)
                // Assuming GetData() returns text content.
                // If text content exists directly, it's legacy: <Member>1001</Member>
                // If not, look for children: <Member><Id>1001</Id><Priority>0</Priority></Member>
                
                std::string strId;
                int iPriority = 0; // Default priority

                CXmlElement* pId = it->SelectElement("Id");
                if ( pId ) {
                    // New Format
                    strId = pId->GetData();
                    CXmlElement* pPrio = it->SelectElement("Priority");
                    if (pPrio) {
                        iPriority = std::stoi(pPrio->GetData());
                    }
                } else {
                    // Legacy Format
                    strId = it->GetData();
                }

                if ( !strId.empty() ) {
                    m_vecMembers.push_back( CGroupMember(strId, iPriority) );
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
