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
		ZNode* g1 = new ZNode(8, 2);
		ZNode* g2 = new ZNode(8, 2);
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
		g2->setIsNetworkIdProvider(true);
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
		listener->setIsNetworkIdProvider(true);
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

		int kTicks = 0;
		while ( g1->getNumOpenConnections() == 0 )
		{
			g2->update();
			g1->update();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			if ( kTicks++ == 100 )
			{
				printf("FAILED connecting in %s\n", this->Name.c_str());
				this->Result = false;
				return;
			}
		}

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
					g1->sendReliableOrdered( (unsigned char)100, (const char*)&seq, sizeof(int), nullptr, false, channel );
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
		g2->setIsNetworkIdProvider(true);

		volatile bool bThreadClose = false;
		std::thread t( [&] () {
			while ( !bThreadClose ) {
				g1->update();
				g2->update();
			}
		});

		int k = 0; 
		int p = 1000;
		while ( k++ < p )
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

		//	std::this_thread::sleep_for(100ms);
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
			assert(sgt->m_unitsRemote.count(groupIdx)==0);
			sgt->m_unitsRemote.insert(std::make_pair(groupIdx, u));
		}
		else
		{
			printf("group %d created locally\n", groupIdx);
			assert(sgt->m_unitsSelf.count(groupIdx)==0);
			sgt->m_unitsSelf.insert(std::make_pair(groupIdx, u));
		}
	}

	DECL_VAR_GROUP_0( myGroup0, zn )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();
		Unit* u = new Unit();
		groupCreateFeedback( u->c.getNetworkGroupId(), u, sgt );
	}

	DECL_VAR_GROUP_1( myGroup1, zn, char, c )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;

		groupCreateFeedback( u->c.getNetworkGroupId(), u, sgt );
	}

	DECL_VAR_GROUP_2( myGroup2, zn, char, c, short, s )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();
		
		Unit* u = new Unit();
		u->c = c;
		u->s = s;

		groupCreateFeedback( u->c.getNetworkGroupId(), u, sgt );
	}

	DECL_VAR_GROUP_3( myGroup3, zn, char, c, short, s, int, i )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;

		groupCreateFeedback( u->c.getNetworkGroupId(), u, sgt );
	}

	DECL_VAR_GROUP_4( myGroup4, zn, char, c, short, s, int, i, float, f )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;
		u->f = f;

		groupCreateFeedback( u->c.getNetworkGroupId(), u, sgt );
	}

	DECL_VAR_GROUP_5( myGroup5, zn, char, c, short, s, int, i, float, f, double, d )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;
		u->f = f;
		u->d = d;

		groupCreateFeedback( u->c.getNetworkGroupId(), u, sgt );
	}

	DECL_VAR_GROUP_6( myGroup6, zn, char, c, short, s, int, i, float, f, double, d, Vec3, vec )
	{
		SyncGroupTest* sgt = (SyncGroupTest*) zn->getUserDataPtr();

		Unit* u = new Unit();
		u->c = c;
		u->s = s;
		u->i = i;
		u->f = f;
		u->d = d;
		u->vec = vec;

		groupCreateFeedback( u->c.getNetworkGroupId(), u, sgt );
	}

	DECL_VAR_GROUP_7( myGroup7, zn, char, c, short, s, int, i, float, f, double, d, Vec3, vec, Quat, quat )
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

		groupCreateFeedback( u->c.getNetworkGroupId(), u, sgt );
	}

	DECL_VAR_GROUP_8( myGroup8, zn, char, c, short, s, int, i, float, f, double, d, Vec3, vec, Quat, quat, Mat3x3, m3x3 )
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

		groupCreateFeedback( u->c.getNetworkGroupId(), u, sgt );
	}

	DECL_VAR_GROUP_9( myGroup9, zn, char, c, short, s, int, i, float, f, double, d, Vec3, vec, Quat, quat, Mat3x3, m3x3, Name2, name )
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

		Vec3 v;
		v.x = 991.991f;
		v.y = 881.882f;
		v.z = 771.773f;
		bool bComp = memcmp( &v, &(Vec3&)u->vec, sizeof(v))==0;
		assert( bComp && "lel" );
		strcpy_s( ((Name2&)u->name).m, 64, name.m );

		groupCreateFeedback( u->c.getNetworkGroupId(), u, sgt );
	}

	void SyncGroupTest::initialize()
	{
	}

	void SyncGroupTest::run()
	{
		auto fnGroupUpdated = [](const ZEndpoint* ept, u32_t groupId) 
		{
			printf("group %d updated\n", groupId);
		};

		auto fnGroupDestroyed = [this](const ZEndpoint* ept, u32_t groupId) 
		{
			if (ept)
			{
				assert(this->m_unitsRemote.count(groupId)==1);
				delete this->m_unitsRemote[groupId];
				this->m_unitsRemote.erase(groupId);
			}
			else
			{
				assert(this->m_unitsSelf.count(groupId)==1);
				//delete this->m_unitsSelf[groupId];
				this->m_unitsSelf.erase(groupId);
			}

			printf("group %d removed\n", groupId);
		};

		ZNode* g1 = new ZNode();
		g1->setUserDataPtr( this );

		int kClients = 1;
		std::vector<ZNode*> clients;
		for (int i = 0; i < kClients ; i++)
		{
			clients.emplace_back( new ZNode() );
			clients[i]->setUserDataPtr( this );
			clients[i]->bindOnGroupUpdated( fnGroupUpdated );
			clients[i]->bindOnGroupDestroyed( fnGroupDestroyed );
			clients[i]->connect("localhost", 27000);
		}
		g1->listenOn( 27000 );
		g1->setIsNetworkIdProvider( true );

		g1->bindOnGroupUpdated( fnGroupUpdated );
		g1->bindOnGroupDestroyed( fnGroupDestroyed );

		int kTicks2=0;
		while ( ++kTicks2 < 100 )
		{
			g1->update();
			int jConnected = 0;
			for (int i = 0; i < kClients ; i++)
			{
				clients[i]->update();
				if ( clients[i]->getNumOpenConnections() > 0 )
					jConnected++;
			}
			if ( jConnected == kClients )
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}


		create_myGroup2(clients[0], 'a', 22 );


		Vec3 v;
		v.x = 991.991f;
		v.y = 881.882f;
		v.z = 771.773f;

		Quat q;
		q.x = 0.f;
		q.y = sqrt(2.f) * .5f;
		q.z = sqrt(2.f) * .5f;
		q.w = 1.f;

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

		Name2 name;
		char* c = name.m;
		sprintf_s( c, 32, "a random name" );

		int kTicks = 0;
		while ( true )
		{
			kTicks++;

			g1->update();
			for ( auto* z : clients )
			{
				z->update();
			}

			// create random num groups
			int numGroups = ::rand() % 10;

			// create num groups on random client
			ZNode* curClient = clients[ ::rand() % kClients];
			for (int i = 0; i < numGroups ; i++)
			{
				// create group with random num params
				int kr = ::rand() % 10;
				switch (kr)
				{
				case 0:
				create_myGroup1( curClient, 'E' );
				break;
				case 1:
				create_myGroup2( curClient, 'E', 6533 );
				break;
				case 2:
				create_myGroup3( curClient, 'E', 6533, ~0 );
				break;
				case 9:
				create_myGroup4( curClient, 'E', 6533, ~0, 2*10e20f );
				break;
				case 3:
				create_myGroup5( curClient, 'E', 6533, ~0, 2*10e20f, 7172773.881238 );
				break;
				case 4:
				create_myGroup6( curClient, 'E', 6533, ~0, 2*10e20f, 7172773.881238, v );
				break;
				case 5:
				create_myGroup7( curClient, 'E', 6533, ~0, 2*10e20f, 7172773.881238, v, q );
				break;
				case 6:
				create_myGroup8( curClient, 'E', 6533, ~0, 2*10e20f, 7172773.881238, v, q, mm );
				break;
				case 7:
				create_myGroup9( curClient, 'E', 6533, ~0, 2*10e20f, 7172773.881238, v, q, mm, name );
				break;
				case 8:
				create_myGroup0( curClient );
				break;
				}	
			}

			// delete some unit groups
			numGroups = ::rand()%5;
			for (int i = 0; i < 5 ; i++)
			{
				if ( m_unitsSelf.size() == 0 )
					break;
				int ridx = rand() % m_unitsSelf.size();
				for (auto it = m_unitsSelf.begin(); it != m_unitsSelf.end(); it++)
				{
					if (0 == ridx)
					{
						delete it->second;
						it->second = nullptr;
						break;
					}
					ridx--;
				}
			}

			// assign data
			for (int i = 0; i<100; i++)
			{
				if (m_unitsSelf.empty())
					break;

				int r2 = rand() % m_unitsSelf.size();

				Unit* uu = nullptr;
				for (auto& it = m_unitsSelf.begin(); it != m_unitsSelf.end(); ++it)
				{
					if (0 == r2)
					{
						uu = it->second;
						break;
					}
					r2--;
				}

				if (uu == nullptr)
					continue;

				int r = rand() % 9;
				switch(r)
				{
				case 0:
				uu->c += '8';
				break;

				case 1:
				uu->s = uu->s - 65532;
				break;

				case 2:
				uu->i *= (~0)/2;
				break;

				case 3:
				uu->d -= 2e30;
				break;

				case 4:
				uu->f += 2e4;
				break;

				case 5:
				uu->vec = v;
				break;

				case 6:
				uu->quat = q;
				break;

				case 7:
				{
					Name2 kName;
					strcpy_s(kName.m, 32, "hoi hoi");
					uu->name = kName;
				}
				break;

				case 8:
				uu->mat = mm;
				break;
				}
			}

			if ( kTicks == 300 )
				clients[0]->disconnectAll();

			if ( kTicks > 15000 )
				break;

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		for ( auto& u : m_unitsSelf )
			delete u.second;
		for (auto& u : m_unitsRemote )
			delete u.second;
		delete g1;
		for ( auto z : clients )
			delete z;
		//delete g2;
		Result = true;
	}


	//////////////////////////////////////////////////////////////////////////
	/// NetworkTests
	//////////////////////////////////////////////////////////////////////////

	void NetworkTests::RunAll()
	{
		std::vector<BaseTest*> tests;

		// add tests
		tests.emplace_back( new ConnectionLayerTest );
		tests.emplace_back( new MassConnectTest );
		tests.emplace_back( new ReliableOrderTest );
		tests.emplace_back( new RpcTest );
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
