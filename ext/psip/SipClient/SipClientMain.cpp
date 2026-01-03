
#include "SipClient.h"
#include "SipClientSetup.h"
#include "Log.h"
#include "SipUtility.h"
#include "RtpThread.h"
#include "MemoryDebug.h"


extern std::string	gstrInviteId;
CSipUserAgent clsUserAgent;



#include <stdarg.h>

class CConsoleLog : public ILogCallBack
{
public:
    void Print( EnumLogLevel eLevel, const char * fmt, ... )
    {
        if( (eLevel & LOG_NETWORK) == 0 ) return;

        va_list ap;
        char    szBuf[LOG_MAX_SIZE];

        va_start( ap, fmt );
        vsnprintf( szBuf, sizeof(szBuf), fmt, ap );
        va_end( ap );

        printf( "%s\n", szBuf );
    }
};

CConsoleLog gclsConsoleLog;


int main( int argc, char * argv[] )
{
	if( argc != 2 )
	{
		printf( "[Usage] %s {xml file}\n", argv[0] );
		return 0;
	}

	char * pszFileName = argv[1];

	if( gclsSetupFile.Read( pszFileName ) == false )
	{
		printf( "Setup file error\n" );
		return 0;
	}

#ifdef WIN32
#ifdef _DEBUG
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF );
	CLog::SetDirectory( "c:\\temp\\sipclient" );
#endif
#else
	// CLog::SetDirectory( "log" );
#endif
	CLog::SetLevel( LOG_NETWORK | LOG_DEBUG | LOG_INFO );
    CLog::SetCallBack( &gclsConsoleLog );

	
	// CSipUserAgent clsUserAgent; // Moved to global

	CSipServerInfo clsServerInfo;
	CSipStackSetup clsSetup;
	CSipClient clsSipClient;

	clsServerInfo.m_strIp = gclsSetupFile.m_strSipServerIp;
	clsServerInfo.m_strDomain = gclsSetupFile.m_strSipDomain;
	clsServerInfo.m_strUserId = gclsSetupFile.m_strSipUserId;
	clsServerInfo.m_strPassWord = gclsSetupFile.m_strSipPassWord;
	clsServerInfo.m_eTransport = gclsSetupFile.m_eSipTransport;
	clsServerInfo.m_iPort = gclsSetupFile.m_iSipServerPort;
	clsServerInfo.m_iLoginTimeout = 600;
	//clsServerInfo.m_iNatTimeout = 10;

	// �Ｚ 070 ���� �α��ν� User-Agent ����� Ư���� ���ڿ��� ���Ե��� ������ 480 ������ ���ŵȴ�.
	// �Ʒ��� ���� Acrobits ���� User-Agent ����� �����ϸ� ���������� 401 ������ �����Ѵ�.
	//clsSetup.m_strUserAgent = "Acrobits";

	// Expires ����� 300 �� �Է��ϰ� ������ �Ʒ��� ���� �����ϸ� �ȴ�.
	// clsServerInfo.m_iLoginTimeout = 300;

	clsSetup.m_iLocalUdpPort = gclsSetupFile.m_iUdpPort;
	clsSetup.m_strLocalIp = gclsSetupFile.m_strLocalIp;

	if( gclsSetupFile.m_eSipTransport == E_SIP_TCP )
	{
		clsSetup.m_iLocalTcpPort = gclsSetupFile.m_iUdpPort;
		clsSetup.m_iTcpCallBackThreadCount = 1;
	}
	else if( gclsSetupFile.m_eSipTransport == E_SIP_TLS )
	{
		clsSetup.m_iLocalTlsPort = gclsSetupFile.m_iUdpPort;
		clsSetup.m_strCertFile = gclsSetupFile.m_strPemFile;
	}

	// Via ��� �� Contact ����� ���� ���� ��Ʈ ��ȣ�� �����ϰ� ������ �Ʒ��� ���� �����ϸ� �ȴ�.
	// clsSetup.m_bUseContactListenPort = true;

	// UDP ���� �������� �⺻ ������ 1���̴�. �̸� �����Ϸ��� CSipStackSetup.m_iUdpThreadCount �� �����ϸ� �ȴ�.

	clsUserAgent.InsertRegisterInfo( clsServerInfo );

	if( clsUserAgent.Start( clsSetup, &clsSipClient ) == false )
	{
		printf( "sip stack start error\n" );
		return 0;
	}

	if( gclsRtpThread.Create() == false )
	{
		printf( "rtp thread create error\n" );
		return 0;
	}

	char	szCommand[1024];
	int		iLen;

	memset( szCommand, 0, sizeof(szCommand) );
	while( fgets( szCommand, sizeof(szCommand), stdin ) )
	{
		if( szCommand[0] == 'Q' || szCommand[0] == 'q' ) break;

		iLen = (int)strlen(szCommand);
		if( iLen >= 2 && szCommand[iLen-2] == '\r' )
		{
			szCommand[iLen-2] = '\0';
		}
		else if( iLen >= 1 && szCommand[iLen-1] == '\n' )
		{
			szCommand[iLen-1] = '\0';
		}

		if( szCommand[0] == 'C' || szCommand[0] == 'c' )
		{
			CSipCallRtp clsRtp;
			CSipCallRoute	clsRoute;

			clsRtp.m_strIp = clsSetup.m_strLocalIp;
			clsRtp.m_iPort = gclsRtpThread.m_iPort;
			clsRtp.m_iCodec = 0;

			clsRoute.m_strDestIp = gclsSetupFile.m_strSipServerIp;
			clsRoute.m_iDestPort = gclsSetupFile.m_iSipServerPort;
			clsRoute.m_eTransport = gclsSetupFile.m_eSipTransport;

			clsUserAgent.StartCall( gclsSetupFile.m_strSipUserId.c_str(), szCommand + 2, &clsRtp, &clsRoute, gstrInviteId );
		}
		else if( szCommand[0] == 'e' || szCommand[0] == 's' )
		{
			clsUserAgent.StopCall( gstrInviteId.c_str() );
			gclsRtpThread.Stop( );
			gstrInviteId.clear();
		}
		else if( szCommand[0] == 'a' )
		{
			CSipCallRtp clsRtp;

			clsRtp.m_strIp = clsSetup.m_strLocalIp;
			clsRtp.m_iPort = gclsRtpThread.m_iPort;
			clsRtp.m_iCodec = 0;

			clsUserAgent.AcceptCall( gstrInviteId.c_str(), &clsRtp );
			gclsRtpThread.Start( clsSipClient.m_clsDestRtp.m_strIp.c_str(), clsSipClient.m_clsDestRtp.m_iPort );
		}
		else if( szCommand[0] == 'm' )
		{
			CSipCallRoute	clsRoute;

			clsRoute.m_strDestIp = gclsSetupFile.m_strSipServerIp;
			clsRoute.m_iDestPort = gclsSetupFile.m_iSipServerPort;
			clsRoute.m_eTransport = gclsSetupFile.m_eSipTransport;

			clsUserAgent.SendSms( gclsSetupFile.m_strSipUserId.c_str(), szCommand + 2, "hello", &clsRoute );
		}
		else if( szCommand[0] == 'i' )
		{
			CMonitorString strBuf;

			clsUserAgent.m_clsSipStack.GetString( strBuf );

			printf( "%s", strBuf.GetString() );
		}
		else if( szCommand[0] == 't' )
		{
			// OPTION �޽��� ���� ����
			CSipMessage * pclsMessage = new CSipMessage();
			if( pclsMessage )
			{
				char	szTag[SIP_TAG_MAX_SIZE], szCallIdName[SIP_CALL_ID_NAME_MAX_SIZE];

				SipMakeTag( szTag, sizeof(szTag) );
				SipMakeCallIdName( szCallIdName, sizeof(szCallIdName) );

				// Call-ID �� �����Ѵ�.
				pclsMessage->m_clsCallId.m_strName = szCallIdName;
				pclsMessage->m_clsCallId.m_strHost = clsUserAgent.m_clsSipStack.m_clsSetup.m_strLocalIp;

				pclsMessage->m_eTransport = E_SIP_UDP;

				// SIP method �� �����Ѵ�.
				pclsMessage->m_strSipMethod = SIP_METHOD_OPTIONS;

				// Request Uri �� �����Ѵ�.
				pclsMessage->m_clsReqUri.Set( SIP_PROTOCOL, "1000", "192.168.0.1", 5060 );

				// CSeq �� �����Ѵ�.
				pclsMessage->m_clsCSeq.Set( 1, SIP_METHOD_OPTIONS );

				// From ����� �����Ѵ�.
				pclsMessage->m_clsFrom.m_clsUri.Set( SIP_PROTOCOL, "2000", "192.168.0.200", 5060 );
				pclsMessage->m_clsFrom.InsertParam( SIP_TAG, szTag );

				// To ����� �����Ѵ�.
				pclsMessage->m_clsTo.m_clsUri.Set( SIP_PROTOCOL, "1000", "192.168.0.1", 5060 );

				// SIP �޽����� ������ ��� IP �ּҿ� ��Ʈ ��ȣ �� ���������� �����Ѵ�.
				pclsMessage->AddRoute( "192.168.0.1", 5060, E_SIP_UDP );

				clsUserAgent.m_clsSipStack.SendSipMessage( pclsMessage );
			}
		}
	
		memset( szCommand, 0, sizeof(szCommand) );
	}

	if( gstrInviteId.empty() == false )
	{
		clsUserAgent.StopCall( gstrInviteId.c_str() );
	}

	gclsRtpThread.Stop( );

	sleep(2);

	clsUserAgent.Stop();
	clsUserAgent.Final();

	return 0;
}
