
#include "../vld/include/vld.h"

#include "RUDPConnection.h"
#include "Socket.h"
#include "EndPoint.h"
#include "RecvPoint.h"
#include "GameNode.h"
#include "GameConnection.h"
#include "Platform.h"
#include "UnitTest.h"

#include <cassert>
#include <chrono>

using namespace Motor;
using namespace Anger;
using namespace std::chrono_literals;


class MyGameNetwork
{
public:
	MyGameNetwork(const std::string& name):
		m_Name(name),
		m_GameNode(new GameNode())
	{
		m_GameNode->bindOnConnectResult( [this] (const auto* c, auto e) { onConnectectResult(c, e); } );
		m_GameNode->bindOnDisconnect( [this] (const auto* c, auto e) { onDisconnect(c, e); } );
		m_GameNode->bindOnNewConnection( [this] (const auto* c) { onNewConnection(c); } );

		//// send value 3 and 2
		//char name [] = "bartje";
		//m_GameNode->rpc( 2, nullptr, false, 2, 
		//				3,  // first arg data
		//				3.23f, 
		//				name ); // send arg data
	}

	~MyGameNetwork()
	{
		delete m_GameNode;
	}

	void doConnect( const EndPoint& ept )
	{
		//EndPoint ept;
		//if ( ept.resolve( "localhost", 27000 ) )
		for ( int i =0; i < 2; i++ )
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

	void onConnectectResult( const GameConnection* conn, EConnectResult connResult )
	{
		switch ( connResult )
		{
			case EConnectResult::Succes:
				printf ("%s conn %s connected succesfully\n", m_Name.c_str(), conn->getEndPoint().asString().c_str());
				this->m_RemoteEpt = conn->getEndPoint();
				break;

			case EConnectResult::Timedout:
				printf ("%s conn %s could not connect (timed out)\n", m_Name.c_str(), conn->getEndPoint().asString().c_str());
				break;
		}
	}

	void onDisconnect( const GameConnection* conn, EDisconnectReason reason )
	{
		switch ( reason )
		{
			case EDisconnectReason::Closed:
				printf ("%s conn %s closed\n", m_Name.c_str(), conn->getEndPoint().asString().c_str());
				break;

			case EDisconnectReason::Lost:
				printf ("%s conn %s lost us... :-( \n", m_Name.c_str(), conn->getEndPoint().asString().c_str());
				break;
		};
	}

	void onNewConnection( const GameConnection* conn )
	{
		printf("%s new connection %s\n", m_Name.c_str(), conn->getEndPoint().asString().c_str());
	}

public:
	std::string m_Name;
	EndPoint m_RemoteEpt;
	GameNode* m_GameNode;
};


void test_fake_connection()
{
}

int main(int argc, char** argv)
{
	if ( 0 == Platform::initialize() )
	{
		NetworkTests::RunAll();
		system("pause");

		Platform::shutdown();
	}
	return 0;
}