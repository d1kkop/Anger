#ifdef MOTOR_NETWORK_DEBUG

#include "UnitTest.h"
#include "EndPoint.h"
#include "RUDPConnection.h"
#include "GameNode.h"
#include "GameConnection.h"
#include "Socket.h"

#include <thread>
#include <chrono>

using namespace std::literals::chrono_literals;


namespace Motor
{
	namespace Anger
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
			bool foundNewConn = false;
			GameNode* g1 = new GameNode();
			GameNode* g2 = new GameNode();
			g1->bindOnConnectResult( [&] (auto* c, auto res) {
				printf("connect result: %s %d\n", c->getEndPoint().asString().c_str(), (int)res);
				if ( res == EConnectResult::Succes )
					connected = true;
			});
			g2->bindOnNewConnection( [&] (auto* c) { 
				printf("new connection: %s\n", c->getEndPoint().asString().c_str());
				foundNewConn = true;
			});
			auto discLamda = [] (auto* c, auto res) 
			{
				printf("disconnected: %s, reason: %d\n", c->getEndPoint().asString().c_str(), (int)res);
			};
			g1->bindOnDisconnect( discLamda );
			g2->bindOnDisconnect( discLamda );
			g1->connect("localhost",27000);
			g2->listenOn(27000);

			volatile bool bClose = false;
			std::thread t( [&] () {
				while  ( !bClose )
				{
					g1->update();
					g2->update();
					std::this_thread::sleep_for(10ms);
				}
			});

			std::this_thread::sleep_for(5000ms);
			bClose = true;
			if ( t.joinable() )
				t.join();
			Result = (foundNewConn && connected);
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
			GameNode* g1 = new GameNode();
			GameNode* g2 = new GameNode();
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

			g2->bindOnCustomData( [&] (auto* c, auto id, auto* data, int len, unsigned char channel)
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
		/// Mass Connections
		//////////////////////////////////////////////////////////////////////////

		void RunMassConnections::initialize()
		{
			Name = "MassConnectionTest";
		}

		void RunMassConnections::run()
		{
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
			tests.emplace_back( new ReliableOrderTest );
	//		tests.emplace_back( new RunMassConnections );
			
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
				delete t;
		}
	}
}

#endif