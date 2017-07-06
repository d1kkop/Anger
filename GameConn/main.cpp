
#include "../vld/include/vld.h"

#include "RUDPConnection.h"
#include "Socket.h"
#include "EndPoint.h"
#include "RecvPoint.h"
#include "ConnectionNode.h"
#include "Connection.h"
#include "Platform.h"
#include "UnitTest.h"
#include "RpcMacros.h"

#include <cassert>
#include <chrono>

using namespace Zerodelay;
using namespace std::chrono_literals;


struct myGameStruct
{
	i8_t namePlayer1[64];
	i8_t namePlayer2[64];
	i8_t namePlayer3[64];
	i8_t namePlayer4[64];
	i8_t namePlayer5[64];
};


RPC_FUNC_3( myRcpTest, i32_t, tank, i8_t, henk, bool, bert )
{
	printf("tank = %d, henk = %c, bert = %d\n", tank, henk, bert );
}

RPC_FUNC_3( myRcpTest2, myGameStruct, gs, i32_t, d1, i32_t, d2 )
{
	printf( "%s %s %s %s %s\n", gs.namePlayer1, gs.namePlayer2, gs.namePlayer3, gs.namePlayer4, gs.namePlayer5 );
}


class MyGameNetwork
{
public:
	MyGameNetwork(const std::string& name):
		m_Name(name),
		m_GameNode(new ConnectionNode())
	{	
		m_GameNode->bindOnConnectResult( [this] (const auto& etp, auto e) { onConnectectResult(etp, e); } );
//		m_GameNode->bindOnDisconnect( [this] (const auto* c, const auto& etp, auto e) { onDisconnect(c, etp, e); } );
		m_GameNode->bindOnNewConnection( [this] (const auto& etp) { onNewConnection( etp); } );
	}

	~MyGameNetwork()
	{
		delete m_GameNode;
	}

	void doConnect( const EndPoint& ept )
	{
		//EndPoint ept;
		//if ( ept.resolve( "localhost", 27000 ) )
		for ( i32_t i =0; i < 2; i++ )
		{
			auto eRes = m_GameNode->connect( ept );
			switch ( eRes )
			{
			case EConnectCallResult::CannotResolveHost:
				printf( "%s cannot resolve host \n", m_Name.c_str() );
				break;
			case EConnectCallResult::SocketError:
				printf( "%s socket error \n", m_Name.c_str() );
				break;
			case EConnectCallResult::Succes:
				printf( "%s connect call result succesful \n", m_Name.c_str() );
				break;
			case EConnectCallResult::AlreadyExists:
				printf( "%s connect to %s already exists\n", m_Name.c_str(), ept.asString().c_str() );
			}
		}
	}

	void doListen()
	{
		auto eRes = m_GameNode->listenOn( 27000 );

		switch( eRes )
		{
		case EListenCallResult::CannotBind:
			printf( "%s cannot bind \n", m_Name.c_str() );
			break;

		case EListenCallResult::SocketError:
			printf( "%s listen socket error\n", m_Name.c_str() );
			break;

		case EListenCallResult::Succes:
			printf( "%s listen succes \n", m_Name.c_str() );
			break;
		}
	}

	void update()
	{
		m_GameNode->update();
	}

	void onConnectectResult( const EndPoint& etp, EConnectResult connResult )
	{
		switch ( connResult )
		{
			case EConnectResult::Succes:
				printf ("%s conn %s connected succesfully\n", m_Name.c_str(), etp.asString().c_str());
				this->m_RemoteEpt =etp;
				break;

			case EConnectResult::Timedout:
				printf ("%s conn %s could not connect (timed out)\n", m_Name.c_str(), etp.asString().c_str());
				break;
		}
	}

	void onDisconnect( bool isThisConnection, const EndPoint& etp, EDisconnectReason reason )
	{
		switch ( reason )
		{
			case EDisconnectReason::Closed:
				printf ("%s conn %s closed\n", m_Name.c_str(), etp.asString().c_str());
				break;

			case EDisconnectReason::Lost:
				printf ("%s conn %s lost us... :-( \n", m_Name.c_str(), etp.asString().c_str());
				break;
		};
	}

	void onNewConnection( const EndPoint& etp )
	{
		printf("%s new connection %s\n", m_Name.c_str(), etp.asString().c_str());
	}

public:
	std::string m_Name;
	EndPoint m_RemoteEpt;
	ConnectionNode* m_GameNode;
};


i32_t main(i32_t argc, i8_t** argv)
{
	//if ( 0 == Platform::initialize() )
	{
	//	NetworkTests::RunAll();
		system("pause");
		Platform::shutdown();
	}
	return 0;
}