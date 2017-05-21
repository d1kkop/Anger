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

	void groupCreateFeedback(int groupIdx, Unit* u, SyncGroupTest* sgt )
	{
		if ( u->c.getVarConrol() == EVarControl::Remote )
		{
			printf("group %d created remotely\n", groupIdx);
			sgt->m_unitsRemote.emplace_back( u );
		}
		else
		{
			printf("group %d created locally\n", groupIdx);
			sgt->m_unitsSelf.emplace_back( u );
		}
	}

	SYNC_GROUP_0( myGroup0, zn )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();
		Unit* u = new Unit();
		groupCreateFeedback( 0, u, sgt );
	}

	SYNC_GROUP_1( myGroup1, zn, char, c )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;

		groupCreateFeedback( 1, u, sgt );
	}

	SYNC_GROUP_2( myGroup2, zn, char, c, short, s )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();
		
		Unit* u = new Unit();
		u->c = c;
		u->s = s;

		groupCreateFeedback(2, u, sgt );
	}

	SYNC_GROUP_3( myGroup3, zn, char, c, short, s, int, i )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;

		groupCreateFeedback( 3, u, sgt );
	}

	SYNC_GROUP_4( myGroup4, zn, char, c, short, s, int, i, float, f )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;
		u->f = f;

		groupCreateFeedback( 4, u, sgt );
	}

	SYNC_GROUP_5( myGroup5, zn, char, c, short, s, int, i, float, f, double, d )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;
		u->f = f;
		u->d = d;

		groupCreateFeedback( 5, u, sgt );
	}

	SYNC_GROUP_6( myGroup6, zn, char, c, short, s, int, i, float, f, double, d, Vec3, vec )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;
		u->f = f;
		u->d = d;
		u->vec = vec;

		groupCreateFeedback( 6, u, sgt );
	}

	SYNC_GROUP_7( myGroup7, zn, char, c, short, s, int, i, float, f, double, d, Vec3, vec, Quat, quat )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;
		u->f = f;
		u->d = d;
		u->vec  = vec;
		u->quat = quat;

		groupCreateFeedback( 7, u, sgt );
	}

	SYNC_GROUP_8( myGroup8, zn, char, c, short, s, int, i, float, f, double, d, Vec3, vec, Quat, quat, Mat3x3, m3x3 )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;
		u->f = f;
		u->d = d;
		u->vec  = vec;
		u->quat = quat;
		u->mat  = m3x3;

		groupCreateFeedback( 8, u, sgt );
	}

	SYNC_GROUP_9( myGroup9, zn, char, c, short, s, int, i, float, f, double, d, Vec3, vec, Quat, quat, Mat3x3, m3x3, Name2, name )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;
		u->f = f;
		u->d = d;
		u->vec  = vec;
		u->quat = quat;
		u->mat  = m3x3;
		strcpy_s( ((Name2&)u->name).m, 64, name.m );

		groupCreateFeedback( 9, u, sgt );
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

		//create_myGroup1( g1, 'E' );
		//create_myGroup2( g1, 'E', 6533 );
		//create_myGroup3( g1, 'E', 6533, ~0 );
		//create_myGroup4( g1, 'E', 6533, ~0, 2*10e20f );
		//create_myGroup5( g1, 'E', 6533, ~0, 2*10e20f, 7172773.881238 );

		Vec3 v;
		v.x = 991.991f;
		v.y = 881.882f;
		v.z = 771.773f;
	//	create_myGroup6( g1, 'E', 6533, ~0, 2*10e20f, 7172773.881238, v );

		Quat q;
		q.x = 0.f;
		q.y = sqrt(2.f) * .5f;
		q.z = sqrt(2.f) * .5f;
		q.w = 1.f;
	//	create_myGroup7( g1, 'E', 6533, ~0, 2*10e20f, 7172773.881238, v, q );

		Mat3x3 mm; // set identity
		for (int i = 0; i < 3 ; i++)
		{
			for (int j = 0; j < 3 ; j++)
			{
				if ( i == j )
					mm.m[i][j] = 1.f;
				else
					mm.m[i][j] = 0.f;
			}
		}
	//	create_myGroup8( g1, 'E', 6533, ~0, 2*10e20f, 7172773.881238, v, q, mm );

		Name2 name;
		char* c = name.m;
		sprintf_s( c, 32, "a random name" );
		create_myGroup9( g1, 'E', 6533, ~0, 2*10e20f, 7172773.881238, v, q, mm, name );

		// -------------------------------------------------------------------------------------------------------------------------------

	//	g1->connect( "localhost", 27000 );
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
