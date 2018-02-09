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
		int numTimedout  = 0;
		int numConnected = 0;
		int numInvalidPw = 0;
		int numMaxConnsReached = 0;
		int numDisconnected = 0;
		int numNewConnections = 0;
		ZNode* g1 = new ZNode();
		const u32_t kConnections = 256;
		ZNode* gs[kConnections];
		for (auto & g : gs)
		{
			g = new ZNode();
		}
		auto discLamda = [&] (bool isThisConnection, auto& etp, auto eReason)
		{
			printf("disconnected: %s, reason: %d\n", etp.toIpAndPort().c_str(), (int)eReason);
			numDisconnected++;
		};
		auto newConnLamda = [&] (bool directLink, auto& etp, auto& additionalData)
		{ 
			printf("new connection: %s\n", etp.toIpAndPort().c_str());
			if ( additionalData.size() )
			{
				printf("additional data:\n");
				for (auto& kvp : additionalData)
				{
					printf("Key: %s value: %s\n", kvp.first.c_str(), kvp.second.c_str());
				}
			}
			numNewConnections++;
		};
		for (auto g : gs)
		{
			g->bindOnConnectResult( [&] (auto etp, auto res) 
			{
				std::string resStr;
				switch ( res )
				{
				case EConnectResult::Succes:
					resStr = "succes";
					numConnected++;
					//	std::this_thread::sleep_for(1000ms);
					//	g1->disconnectAll();
					break;

				case EConnectResult::Timedout:
					resStr = "timed out";
					numTimedout++;
					break;
				case EConnectResult::InvalidPassword:
					resStr = "invalid password";
					numInvalidPw++;
					break;
				case EConnectResult::MaxConnectionsReached:
					resStr = "max connections reached";
					numMaxConnsReached++;
					break;
				}
				printf( "connect result: %s %s\n", etp.toIpAndPort().c_str(), resStr.c_str() );
			});
			g->bindOnNewConnection( newConnLamda );
			g->bindOnDisconnect( discLamda );
		}
		g1->bindOnNewConnection( newConnLamda );
		g1->bindOnDisconnect( discLamda );
		ZEndpoint ztp;
		std::map<std::string, std::string> values;
		values["value1"]  = "hello world";
		values["my name"] = "bart";
		values["key3"]    = "value3";
		bool bResolve = ztp.resolve("localhost", 27001);
		assert(bResolve);
		EConnectCallResult res;
		//printf("deliberately connecting with wrong pw..\n");
		//res = gs[0]->connect( ztp, "lala2", 8, values );
		//assert(res == EConnectCallResult::Succes);
		//res = gs[0]->connect("localhost",27001,"lala", 8, values);
		//assert(res == EConnectCallResult::AlreadyExists);
	//	g2->setMaxIncomingConnections(-1);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		g1->listen(27001, "lala", 1024);

		ZEndpoint ztp2("127.0.0.1", 27001);
		ZEndpoint ztp3("localhost", 27001);
		assert(ztp2 == ztp3);
		volatile bool bClose = false;
		std::thread t( [&] () 
		{
			while  ( !bClose )
			{
				g1->update();
				for (auto g : gs)
				{
					g->update();
					std::this_thread::sleep_for(10ms);
					if ( !g->isConnectionKnown(ztp2) )
					{
						res = g->connect("127.0.0.1", 27001, "lala", 8, values);
					}
				}
			}
		});

		std::this_thread::sleep_for(5000ms);
		bClose = true;
		if ( t.joinable() )
			t.join();

		g1->disconnect();
	//	g2->disconnect();


		auto fnAnyConnectionHasOpenLinks = [&]()
		{
			for ( auto g : gs )
			{
				if ( g->getNumOpenLinks() > 0 ) return true;
			}
			return false;
		};

		int k = 0;
		while (/*++k < 20 &&*/ (g1->getNumOpenLinks() != 0 || fnAnyConnectionHasOpenLinks()) )
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			g1->update();
			for (auto g : gs) g->update();
		}

		delete g1;
		for (auto g : gs) delete g;

		int kTargetNumConns = (int) roundf(float(kConnections+1)*((float)kConnections/2));
		Result = ( numConnected == kConnections ) && ( kTargetNumConns  == numNewConnections ); // numInvalidPw == 1 && numTimedout == 0 );

		printf("Results: \nNumConnected %d numRemoteConnected %d NumTimedOut %d NumInvalidPw %d NumMaxConnectionReached %d NumDisconnected %d\n", 
			   numConnected, numNewConnections, numTimedout, numInvalidPw, numMaxConnsReached, numDisconnected);
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

		auto onNewConnLamda = [&] (bool directLink, auto etp, auto& additionalData)
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
		auto eRes = listener->listen(27001, pw);
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
		g2->listen( 27000 );

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

		if ( tSend.joinable() )
			tSend.join();
		if ( tRecv.joinable() )
			tRecv.join();

		g1->disconnect();
		g2->disconnect();

		// Wait until done
		std::thread tWait( [&] () 
		{
			while ( g1->hasPendingData() || g2->hasPendingData() )
			{	
				g2->update();
				g1->update();
				std::this_thread::sleep_for(5ms);
			}
		});

		std::this_thread::sleep_for(1000ms);

		if ( tWait.joinable() )
			tWait.join();

		delete g1;
		delete g2;
	}

	//////////////////////////////////////////////////////////////////////////
	/// RPC
	//////////////////////////////////////////////////////////////////////////

#if _DEBUG
	constexpr int nRpcs=10000;
#else
	constexpr int nRpcs=1000000;
#endif
	int __high[nRpcs];				int hR=0, hS=0;
	float __fltMax[nRpcs];			int hR1=0, hS1=0;
	float __fltMin[nRpcs];			int hR2=0, hS2=0;
	double __dblMax[nRpcs];			int hR3=0, hS3=0;
	double __dblMin[nRpcs];			int hR4=0, hS4=0;
	long __llong[nRpcs];			int hR5=0, hS5=0;
	char __bb[nRpcs];				int hR6=0, hS6=0;
	short __sk[nRpcs];				int hR7=0, hS7=0;
	int hR8=0, hS8=0;

	RPC_FUNC_0( unitRpcTest0 )
	{
//		printf(" unitRpcTest0 \n" );
	}
	RPC_FUNC_1( unitRpcTest1, int, a )
	{
//		printf(" unitRpcTest1 %d \n", a );
		RpcTest* rt = (RpcTest*)userData;
		if ( rt->_zrecv.isZero() )
			rt->_zrecv = *etp;
		if ( rt->_zrecv == *etp )
		{
			assert( a == __high[hR++] );
		}
		else
		{
			printf("received rpc1 data from unknown endpoint \n");
		}
	}
	RPC_FUNC_2( unitRpcTest2, int, a, float, b )
	{
//		printf(" unitRpcTest1 %d %.3f \n", a, b );
		RpcTest* rt = (RpcTest*)userData;
		if ( rt->_zrecv.isZero() )
			rt->_zrecv = *etp;
		if ( rt->_zrecv == *etp )
		{
			assert( a == __high[hR1] && b==__fltMax[hR1] );
			hR1++;
		}
		else
		{
			printf("received rpc2 data from unknown endpoint \n");
		}
	}
	RPC_FUNC_3( unitRpcTest3, int, a, float, b, double, c )
	{
//		printf(" unitRpcTest %d %.3f %.f \n", a, b, c );
		RpcTest* rt = (RpcTest*)userData;
		if ( rt->_zrecv.isZero() )
			rt->_zrecv = *etp;
		if ( rt->_zrecv == *etp )
		{
			assert( a == __high[hR2] && b==__fltMax[hR2] && c==__dblMax[hR2] );
			hR2++;
		}
		else
		{
			printf("received rpc3 data from unknown endpoint \n");
		}
	}
	RPC_FUNC_4( unitRpcTest4, int, a, float, b, double, c, long, k )
	{
//		printf(" unitRpcTest %d %.3f %.f %d \n", a, b, c, k );
		RpcTest* rt = (RpcTest*)userData;
		if ( rt->_zrecv.isZero() )
			rt->_zrecv = *etp;
		if ( rt->_zrecv == *etp )
		{
			assert( a == __high[hR3] && b==__fltMax[hR3] && c==__dblMax[hR3] && k == __llong[hR3] );
			hR3++;
		}
		else
		{
			printf("received rpc4 data from unknown endpoint \n");
		}
	}
	RPC_FUNC_5( unitRpcTest5, int, a, float, b, double, c, long, k, char, bb )
	{
//		printf(" unitRpcTest %d %.3f %.f %d %d\n", a, b, c, k, bb );
		RpcTest* rt = (RpcTest*)userData;
		if ( rt->_zrecv.isZero() )
			rt->_zrecv = *etp;
		if ( rt->_zrecv == *etp )
		{
			assert( a == __high[hR4] && b==__fltMax[hR4] && c==__dblMax[hR4] && k == __llong[hR4] && bb==__bb[hR4] );
			hR4++;
		}
		else
		{
			printf("received rpc5 data from unknown endpoint \n");
		}
	}
	RPC_FUNC_6( unitRpcTest6, int, a, float, b, double, c, long, k, char, bb, double, fj )
	{
//		printf(" unitRpcTest %d %.3f %.f %d %d %f\n", a, b, c, k, bb, fj );
		RpcTest* rt = (RpcTest*)userData;
		if ( rt->_zrecv.isZero() )
			rt->_zrecv = *etp;
		if ( rt->_zrecv == *etp )
		{
			assert( a == __high[hR5] && b==__fltMax[hR5] && c==__dblMax[hR5] && k == __llong[hR5] && bb==__bb[hR5] && fj==__dblMin[hR5] );
			hR5++;
		}
		else
		{
			printf("received rpc6 data from unknown endpoint \n");
		}
	}
	RPC_FUNC_7( unitRpcTest7, int, a, float, b, double, c, long, k, char, bb, double, fj, short, s )
	{
//		printf(" unitRpcTest %d %.3f %.f %d %d %f, %d\n", a, b, c, k, bb, fj, s );
		RpcTest* rt = (RpcTest*)userData;
		if ( rt->_zrecv.isZero() )
			rt->_zrecv = *etp;
		if ( rt->_zrecv == *etp )
		{
			assert( a == __high[hR6] && b==__fltMax[hR6] && c==__dblMax[hR6] && k == __llong[hR6] && bb==__bb[hR6] && fj==__dblMin[hR6] && s==__sk[hR6] );
			hR6++;
		}
		else
		{
			printf("received rpc7 data from unknown endpoint \n");
		}
	}
	RPC_FUNC_8( unitRpcTest8, int, a, float, b, double, c, long, k, char, bb, double, fj, short, s, float, kt )
	{
//		printf(" unitRpcTest %d %.3f %.f %d %d %f, %d %.5f\n", a, b, c, k, bb, fj, s, kt );
		RpcTest* rt = (RpcTest*)userData;
		if ( rt->_zrecv.isZero() )
			rt->_zrecv = *etp;
		if ( rt->_zrecv == *etp )
		{
			assert( a == __high[hR7] && b==__fltMax[hR7] && c==__dblMax[hR7] && k == __llong[hR7] && bb==__bb[hR7] && fj==__dblMin[hR7] && s==__sk[hR7] && kt==__fltMin[hR7] );
			hR7++;
		}
		else
		{
			printf("received rpc8 data from unknown endpoint \n");
		}
	}
	RPC_FUNC_9( unitRpcTest9, int, a, float, b, double, c, long, k, char, bb, double, fj, short, s, float, kt, double, dt )
	{
//		printf(" unitRpcTest %d %.3f %.f %d %d %f, %d %.5f %f\n", a, b, c, k, bb, fj, s, kt, dt );
		RpcTest* rt = (RpcTest*)userData;
		if ( rt->_zrecv.isZero() )
			rt->_zrecv = *etp;
		if ( rt->_zrecv == *etp )
		{
			assert( a == __high[hR8] && b==__fltMax[hR8] && c==__dblMax[hR8] && k == __llong[hR8] && bb==__bb[hR8] && fj==__dblMin[hR8] && s==__sk[hR8] && kt==__fltMin[hR8] && dt == __dblMax[hR8] );
			hR8++;
		}
		else
		{
			printf("received rpc9 data from unknown endpoint \n");
		}
	}


	void RpcTest::initialize()
	{
		Name = "RpcTest";
	}

	void RpcTest::run()
	{
		for ( int i=0; i<nRpcs; ++i)
		{
			__high[i] = rand();
			__fltMax[i] = rand()%3 == 0? FLT_MIN : rand()%2==0 ? FLT_MAX : 1e20f *(rand()%RAND_MAX);
			__dblMax[i] = rand()%3 == 0? DBL_MIN : rand()%2==0 ? DBL_MAX : 1e20f *(rand()%RAND_MAX);
			__llong[i] = rand();
			__bb[i] = rand();
			__dblMin[i] = rand()%3 == 0? DBL_MIN : rand()%2==0 ? DBL_MAX : 1e20f *(rand()%RAND_MAX);
			__sk[i] = rand()%3;
			__fltMin[i] = rand()%3 == 0? FLT_MIN : rand()%2==0 ? FLT_MAX : 1e20f *(rand()%RAND_MAX);
		}

		ZNode* g1 = new ZNode();
		ZNode* g2 = new ZNode();

		_ztp.resolve("localhost", 27000);
		
		g1->connect( _ztp, "", 8, std::map<std::string, std::string>(), true );
		
		g2->listen( 27000 );
		g2->setUserDataPtr( this );

		volatile bool bThreadClose = false;
		std::thread t( [&] () 
		{
			while ( !bThreadClose ) 
			{
				g1->update();
				g2->update();
				//std::this_thread::sleep_for(2ms);
			}
		});

		// wait to be connected
		std::this_thread::sleep_for(1000ms);

		int k = 0; 
		int p = nRpcs;
		int rpcChan = 0;
		while ( k++ < p )
		{
			rpc_unitRpcTest0( g1, false, nullptr, false, rpcChan );
			rpc_unitRpcTest1( g1, __high[hS++], false, nullptr, false, rpcChan );
			rpc_unitRpcTest2( g1, __high[hS1], __fltMax[hS1], false, nullptr, false, rpcChan ); hS1++;
			rpc_unitRpcTest3( g1, __high[hS2], __fltMax[hS2], __dblMax[hS2], false, nullptr, false, rpcChan ); hS2++;
			rpc_unitRpcTest4( g1, __high[hS3], __fltMax[hS3], __dblMax[hS3], __llong[hS3], false, nullptr, false, rpcChan ); hS3++;
			rpc_unitRpcTest5( g1, __high[hS4], __fltMax[hS4], __dblMax[hS4], __llong[hS4], __bb[hS4], false, nullptr, false, rpcChan ); hS4++;
			rpc_unitRpcTest6( g1, __high[hS5], __fltMax[hS5], __dblMax[hS5], __llong[hS5], __bb[hS5], __dblMin[hS5], false, nullptr, false, rpcChan ); hS5++;
			rpc_unitRpcTest7( g1, __high[hS6], __fltMax[hS6], __dblMax[hS6], __llong[hS6], __bb[hS6], __dblMin[hS6], __sk[hS6], false, nullptr, false, rpcChan ); hS6++;
			rpc_unitRpcTest8( g1, __high[hS7], __fltMax[hS7], __dblMax[hS7], __llong[hS7], __bb[hS7], __dblMin[hS7], __sk[hS7], __fltMin[hS7], false, nullptr, false, rpcChan ); hS7++;
			rpc_unitRpcTest9( g1, __high[hS8], __fltMax[hS8], __dblMax[hS8], __llong[hS8], __bb[hS8], __dblMin[hS8], __sk[hS8], __fltMin[hS8], __dblMax[hS8], false, nullptr, false, rpcChan ); hS8++;
		//	std::this_thread::sleep_for(1ms);
		}

		while (g1->hasPendingData() || g2->hasPendingData())
		{
			std::this_thread::sleep_for(2ms);
		}

		bThreadClose = true;

		if ( t.joinable() )
			t.join();

		g1->disconnect();
		g2->disconnect();
		

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
		g1->listen( 27000 );
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
				clients[0]->disconnect();

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
		u32_t nSuccesful = 0;
		for ( auto* t : tests )
		{
			t->initialize();
			printf("Running test %s\n", t->Name.c_str());
			t->run();
			if ( t->Result )
			{
				nSuccesful++;
				printf("%s succesful\n", t->Name.c_str());
			}
			else
				printf("%s FAILED\n", t->Name.c_str());
		}


		printf("\n\n----------- RESULTS ----------------\n");
		printf("Num Tests %d | Succesful %d | Failed %d\n", (u32_t)tests.size(), nSuccesful,(u32_t)tests.size()-nSuccesful);
		for (auto t : tests)
		{
			if ( !t->Result ) printf("\t%s FAILED\n", t->Name.c_str());
		}


		for ( auto* t : tests )
		{
			delete t;
		}

		printf("\nTests END\n\n");
	}
}
