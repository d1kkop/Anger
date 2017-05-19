#include "UnitTest.h"
#include "Zerodelay.h"
#include "RpcMacros.h"
#include "SyncGroups.h"

#include <windows.h>

#include <vector>
#include <thread>
#include <chrono>

using namespace std::literals::chrono_literals;
using namespace Zerodelay;


namespace UnitTests
{
	BaseTest::BaseTest():
		Result(true) 
	{
	}

	//////////////////////////////////////////////////////////////////////////
	// Connection layer test
	//////////////////////////////////////////////////////////////////////////

	void ConnectionLayerTest::initialize()
	{
		Name = "ConnectionLayer";
	}

	void ConnectionLayerTest::run()
	{
		bool connected = false;
		bool timedOut  = false;
		bool foundNewConn = false;
		ZNode* g1 = new ZNode(8, 2, true);
		ZNode* g2 = new ZNode(8, 2, true);
		g1->bindOnConnectResult( [&] (auto etp, auto res) 
		{
			std::string resStr;
			switch ( res )
			{
			case EConnectResult::Succes:
				resStr = "succes";
			//	std::this_thread::sleep_for(1000ms);
			//	g1->disconnectAll();
				break;

			case EConnectResult::Timedout:
				resStr = "timed out";
				break;
			case EConnectResult::InvalidPassword:
				resStr = "invalid password";
				break;
			case EConnectResult::MaxConnectionsReached:
				resStr = "max connections reached";
				break;
			}
			printf( "connect result: %s %s\n", etp.asString().c_str(), resStr.c_str() );
		});
		g2->bindOnNewConnection( [&] (auto& etp)
		{ 
			printf("new connection: %s\n", etp.asString().c_str());
			foundNewConn = true;
		});
		auto discLamda = [] (bool isThisConnection, auto& etp, auto eReason)
		{
			printf("disconnected: %s, reason: %d\n", etp.asString().c_str(), (int)eReason);
		};
		g1->bindOnDisconnect( discLamda );
		g2->bindOnDisconnect( discLamda );
		g1->connect("localhost",27000,"lala");
		g2->setMaxIncomingConnections(1);
		g2->listenOn(27000, "lala");

		volatile bool bClose = false;
		std::thread t( [&] () {
			while  ( !bClose )
			{
				g1->update();
				g2->update();
				std::this_thread::sleep_for(10ms);
			}
		});

		std::this_thread::sleep_for(10000ms);
		bClose = true;
		if ( t.joinable() )
			t.join();
		Result = (foundNewConn);

		delete g1;
		delete g2;
	}

	//////////////////////////////////////////////////////////////////////////
	// Mass Connect test
	//////////////////////////////////////////////////////////////////////////

	void MassConnectTest::initialize()
	{
		Name = "MassConnectTest";
	}

	void MassConnectTest::run()
	{
		int connSucc = 0;
		int connTimeout = 0;
		int connInvalidPw = 0;
		int connMaxConnReached = 0;
		int connNewIncomingConns = 0;
		int numDisconnects = 0;
		int numLost = 0;

		auto onConnectLamda =  [&] (auto etp, auto res) 
		{
			std::string resStr;
			switch ( res )
			{
			case EConnectResult::Succes:
				connSucc++;
			break;
			case EConnectResult::Timedout:
				connTimeout++;
			break;
			case EConnectResult::InvalidPassword:
				connInvalidPw++;
			break;
			case EConnectResult::MaxConnectionsReached:
				connMaxConnReached++;
			break;
			}
		};

		auto onNewConnLamda = [&] (auto etp)
		{
			connNewIncomingConns++;
		};

		auto onDisconnectLamda = [&] (bool isThisConn, auto etp, auto eReason)
		{
			if ( eReason == EDisconnectReason::Closed )
				numDisconnects++;
			else
				numLost++;
		};

		int connsPerNode = NumConns / NumNodes;
		ZNode** connNodes = new ZNode*[NumNodes];
		std::string pw = "halooooooo123";
		for ( int j=0; j<NumNodes; ++j )
		{
			connNodes[j] = new ZNode();
			auto* n = connNodes[j];
			n->bindOnConnectResult( onConnectLamda );
			n->bindOnDisconnect( onDisconnectLamda );
			n->bindOnNewConnection( onNewConnLamda );
			for ( int i=0; i<connsPerNode; ++i )
			{
				const char* pw_ptr = pw.c_str(); 
				if ( rand() % 10 == 9 )
					pw_ptr = "trala"; // invalid pw
				auto eConnCallRes = n->connect( "localhost", 27001, pw_ptr );
				if ( (int)eConnCallRes != 0 )
				{
					printf("con call res error: %d\n", (int)eConnCallRes);
				}
			}
		}

		ZNode* listener = new ZNode();
		auto eRes = listener->listenOn(27001, pw);
		if ( (int)eRes != 0 )
		{
			printf("listen call res error: %d\n", (int)eRes );
		}

		auto tNow = ::clock();
		while ( true )
		{
			listener->update();

			for (int i = 0; i < NumNodes ; i++)
			{
				auto* n = connNodes[i];
				n->update();
			}

			std::this_thread::sleep_for( 30ms );

			if ( ::GetAsyncKeyState( 'Y' ) )
			{
				break;
			}

			auto newNow = ::clock();
			float t = float(newNow - tNow) / (float)CLOCKS_PER_SEC;
			if ( t > 2.f ) // after 2 sec break;
				break;
		}

		for (int i = 0; i < NumNodes ; i++)
		{
			auto* n = connNodes[i];
			delete n;
		}
		delete [] connNodes;
		delete listener;

		printf("connSucc %d connTimeout %d connInvalidPw %d connMaxConn %d newConns %d numClosed %d numLost %d\n", 
			    connSucc, connTimeout, connInvalidPw, connMaxConnReached, connNewIncomingConns, numDisconnects, numLost );
	}

	//////////////////////////////////////////////////////////////////////////
	// Reliable Order Test
	//////////////////////////////////////////////////////////////////////////

	void ReliableOrderTest::initialize()
	{
		Name = "ReliableOrderTest";
	}

	void ReliableOrderTest::run()
	{
		ZNode* g1 = new ZNode();
		ZNode* g2 = new ZNode();
		g2->simulatePacketLoss(PackLoss);

		g1->connect( "localhost", 27000 );
		g2->listenOn( 27000 );

		int kSends = NumSends;
		static const int nch = 8;

		// Send..
		std::thread tSend( [=] () 
		{
			int sendSeq[nch];
			for (int i=0; i<nch;i++)
				sendSeq[i]=0;
			bool bLoop = true;
			while ( bLoop )
			{
				int channel = ::rand() % nch;
				if ( sendSeq[channel] != kSends )
				{
					int seq = sendSeq[channel];
					g1->beginSend();
					g1->send( (unsigned char)100, (const char*)&seq, sizeof(int), EPacketType::Reliable_Ordered, channel );
					g1->endSend();
					sendSeq[channel]++;
				}
				// see if all is transmitted
				bLoop = false;
				for (int i=0; i<nch; i++)
				{
					if ( sendSeq[i] != kSends )
					{
						bLoop = true;
						break;
					}
				}
			}
		});

		volatile bool bDoneRecv = false;
		int expSeq[8];
		for ( int i=0; i<8; i++)
			expSeq[i]=0;

		g2->bindOnCustomData( [&] (auto& etp, auto id, auto* data, int len, unsigned char channel)
		{
			switch ( id )
			{
				case 100:
				{
					int seq = *(int*)data;
			//		printf( "seq %d, channel %d \n", seq, channel );
					if ( seq != expSeq[channel] )
					{
						printf( "%s invalid seq found\n", Name.c_str() );
						Result = false;
						bDoneRecv = true;
					}
					else
					{
						expSeq[channel]++;
						int i;
						for (i=0; i<nch; i++)
						{
							if ( expSeq[i] != kSends ) 
								break;
						}
						if ( i == nch )
						{
							bDoneRecv = (i == nch);
						}
					}
				}
				break;
			}
		});

		// Recv..
		std::thread tRecv( [&] () 
		{
			while ( !bDoneRecv )
			{	
				g2->update();
				std::this_thread::sleep_for(5ms);
			}
		});

		std::this_thread::sleep_for(1000ms);

		if ( tSend.joinable() )
			tSend.join();
		if ( tRecv.joinable() )
			tRecv.join();

		delete g1;
		delete g2;
	}

	//////////////////////////////////////////////////////////////////////////
	/// RPC
	//////////////////////////////////////////////////////////////////////////

	int __high = ~0;
	float __fltMax  = FLT_MAX;
	float __fltMin  = FLT_MIN;
	double __dblMax = DBL_MAX;
	double __dblMin = DBL_MIN;
	long __llong = LONG_MAX;
	char __bb  = 127;
	short __sk = 31535;

	RPC_FUNC_0( unitRpcTest0 )
	{
//		printf(" unitRpcTest0 \n" );
	}
	RPC_FUNC_1( unitRpcTest1, int, a )
	{
//		printf(" unitRpcTest1 %d \n", a );
		assert( a == __high );
	}
	RPC_FUNC_2( unitRpcTest2, int, a, float, b )
	{
//		printf(" unitRpcTest1 %d %.3f \n", a, b );
		assert( a == __high && b==__fltMax );
	}
	RPC_FUNC_3( unitRpcTest3, int, a, float, b, double, c )
	{
//		printf(" unitRpcTest %d %.3f %.f \n", a, b, c );
		assert( a == __high && b==__fltMax && c==__dblMax );
	}
	RPC_FUNC_4( unitRpcTest4, int, a, float, b, double, c, long, k )
	{
//		printf(" unitRpcTest %d %.3f %.f %d \n", a, b, c, k );
		assert( a == __high && b==__fltMax && c==__dblMax && k == __llong );
	}
	RPC_FUNC_5( unitRpcTest5, int, a, float, b, double, c, long, k, char, bb )
	{
//		printf(" unitRpcTest %d %.3f %.f %d %d\n", a, b, c, k, bb );
		assert( a == __high && b==__fltMax && c==__dblMax && k == __llong && bb==__bb );
	}
	RPC_FUNC_6( unitRpcTest6, int, a, float, b, double, c, long, k, char, bb, double, fj )
	{
//		printf(" unitRpcTest %d %.3f %.f %d %d %f\n", a, b, c, k, bb, fj );
		assert( a == __high && b==__fltMax && c==__dblMax && k == __llong && bb==__bb && fj==__dblMin );
	}
	RPC_FUNC_7( unitRpcTest7, int, a, float, b, double, c, long, k, char, bb, double, fj, short, s )
	{
//		printf(" unitRpcTest %d %.3f %.f %d %d %f, %d\n", a, b, c, k, bb, fj, s );
		assert( a == __high && b==__fltMax && c==__dblMax && k == __llong && bb==__bb && fj==__dblMin && s==__sk );
	}
	RPC_FUNC_8( unitRpcTest8, int, a, float, b, double, c, long, k, char, bb, double, fj, short, s, float, kt )
	{
//		printf(" unitRpcTest %d %.3f %.f %d %d %f, %d %.5f\n", a, b, c, k, bb, fj, s, kt );
		assert( a == __high && b==__fltMax && c==__dblMax && k == __llong && bb==__bb && fj==__dblMin && s==__sk && kt==__fltMin );
	}
	RPC_FUNC_9( unitRpcTest9, int, a, float, b, double, c, long, k, char, bb, double, fj, short, s, float, kt, double, dt )
	{
//		printf(" unitRpcTest %d %.3f %.f %d %d %f, %d %.5f %f\n", a, b, c, k, bb, fj, s, kt, dt );
		assert( a == __high && b==__fltMax && c==__dblMax && k == __llong && bb==__bb && fj==__dblMin && s==__sk && kt==__fltMin && dt == __dblMax );
	}


	void RpcTest::initialize()
	{
		Name = "RpcTest";
	}

	void RpcTest::run()
	{
		ZNode* g1 = new ZNode();
		ZNode* g2 = new ZNode();

		g1->connect( "localhost", 27000 );
		g2->listenOn( 27000 );

		volatile bool bThreadClose = false;
		std::thread t( [&] () {
			while ( !bThreadClose ) {
				g1->update();
				g2->update();
			}
		});

		int k = 0; 
		while ( k++ < 10 )
		{

			rpc_unitRpcTest0( g1 );
			rpc_unitRpcTest1( g1, __high );
			rpc_unitRpcTest2( g1, __high, __fltMax );
			rpc_unitRpcTest3( g1, __high, __fltMax, __dblMax );
			rpc_unitRpcTest4( g1, __high, __fltMax, __dblMax, __llong );
			rpc_unitRpcTest5( g1, __high, __fltMax, __dblMax, __llong, __bb );
			rpc_unitRpcTest6( g1, __high, __fltMax, __dblMax, __llong, __bb, __dblMin );
			rpc_unitRpcTest7( g1, __high, __fltMax, __dblMax, __llong, __bb, __dblMin, __sk );
			rpc_unitRpcTest8( g1, __high, __fltMax, __dblMax, __llong, __bb, __dblMin, __sk, __fltMin );
			rpc_unitRpcTest9( g1, __high, __fltMax, __dblMax, __llong, __bb, __dblMin, __sk, __fltMin, __dblMax );

			std::this_thread::sleep_for(100ms);
		}

		std::this_thread::sleep_for(300ms);
		bThreadClose = true;

		if ( t.joinable() )
			t.join();

		delete g1;
		delete g2;
		Result = true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Sync group test layer test
	//////////////////////////////////////////////////////////////////////////


	SYNC_GROUP_2( myGroup, zn, double, d, int, i )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();
		
		Unit* u = new Unit();
		sprintf_s( u->playerName, 64, "bart knuiman" ); 
//		u->nDouble = d;
		u->nInt = -1;
	//	u->nInt = i;
	//	u->nFloat  = 817.92f;
	//	u->nFloat2 = 83.76f;
	
		if ( u->nInt.getVarConrol() == EVarControl::Remote )
		{
			sgt->m_unitsRemote.emplace_back( u );
		}
		else
		{
			sgt->m_unitsSelf.emplace_back( u );
		}
	}

	void SyncGroupTest::initialize()
	{
	}

	void SyncGroupTest::run()
	{
		ZNode* g1 = new ZNode();
		ZNode* g2 = new ZNode();

		g1->setUserDataPtr( this );
		g2->setUserDataPtr( this );
		create_myGroup( g1, 8.723, 88 );

		g1->connect( "localhost", 27000 );
		g2->listenOn( 27000 );
		g2->setIsNetworkIdProvider( true );

		int kTicks = 0;
		while ( true )
		{
			g1->update();
			g2->update();
			std::this_thread::sleep_for(20ms);

			kTicks++;

			// 50 is a sec
			if ( kTicks == 50 )
			{
				if ( m_unitsSelf.size() )
				{
					Unit* u =  m_unitsSelf[0];
					assert( u->nInt.getVarConrol() == EVarControl::Full );
					u->nInt = 8819;
					sprintf_s( u->playerName, 64, "super raket change");
					printf("have control\n");
				}

				if ( m_unitsRemote.size() )
				{
					Unit* u =  m_unitsRemote[0];
					assert( u->nInt.getVarConrol() == EVarControl::Remote );
					printf("have NO control\n");
				}
			}

			if ( kTicks > 200 )
				break;
		}

		for ( auto* u : m_unitsSelf )
			delete u;
		for (auto * u : m_unitsRemote )
			delete u;
		delete g1;
		delete g2;
		Result = true;
	}


	//////////////////////////////////////////////////////////////////////////
	/// NetworkTests
	//////////////////////////////////////////////////////////////////////////

	void NetworkTests::RunAll()
	{
		std::vector<BaseTest*> tests;

		// add tests
//		tests.emplace_back( new ConnectionLayerTest );
//		tests.emplace_back( new MassConnectTest );
//		tests.emplace_back( new ReliableOrderTest );
//		tests.emplace_back( new RpcTest );
		tests.emplace_back( new SyncGroupTest );
			
		// run them
		for ( auto* t : tests )
		{
			t->initialize();
			printf("Running test %s\n", t->Name.c_str());
			t->run();
			if ( t->Result )
				printf("%s succesful\n", t->Name.c_str());
			else
				printf("%s FAILED\n", t->Name.c_str());
		}

		for ( auto* t : tests )
		{
			delete t;
		}
	}
}
