#include "GameNode.h"
#include "GameConnection.h"
#include "Socket.h"
#include "EndPoint.h"
#include "RpcMacros.h"


namespace Motor
{
	namespace Anger
	{
		GameNode::GameNode(int connectTimeoutSeconds, int sendThreadSleepTimeMs, bool captureSocketErrors):
			RecvPoint(captureSocketErrors, sendThreadSleepTimeMs),
			m_SocketIsOpened(false),
			m_SocketIsBound(false),
			m_ConnectTimeoutMs(connectTimeoutSeconds*1000)
		{
		}

		GameNode::~GameNode()
		{
		}

		class IConnection* GameNode::createNewConnection(const EndPoint& endPoint) const
		{
			return new GameConnection( endPoint, 12 );
		}

		EConnectCallResult GameNode::connect(const std::string& name, int port)
		{
			EndPoint endPoint;
			if ( !endPoint.resolve( name, port ) )
			{
				return EConnectCallResult::CannotResolveHost;
			}
			return connect( endPoint );
		}

		Motor::Anger::EConnectCallResult GameNode::connect(const EndPoint& endPoint)
		{
			if ( !m_Socket )
			{
				return EConnectCallResult::SocketError;
			}

			if ( !openSocket() )
			{
				return EConnectCallResult::SocketError; 
			}

			// explicitly binding here, because otherwise we cannot start waiting for data on the socket
			if ( !bindSocket(0) )
			{
				return EConnectCallResult::CannotBind;
			}

			GameConnection* g;
			std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
			{
				if ( m_Connections.find( endPoint ) != m_Connections.end() )
				{
					return EConnectCallResult::AlreadyExists;
				}
				g = new GameConnection( endPoint, 10 );
				m_Connections.insert( std::make_pair( endPoint, g ) );
			}

			g->sendConnectRequest();	
			startThreads();
			return EConnectCallResult::Succes;
		}

		EListenCallResult GameNode::listenOn(int port)
		{
			if ( !m_Socket )
			{
				return EListenCallResult::SocketError;
			}

			if ( !openSocket() )
			{
				return EListenCallResult::SocketError;
			}

			if ( !bindSocket(port) )
			{
				return EListenCallResult::CannotBind;
			}

			startThreads();
			return EListenCallResult::Succes;
		}

		void GameNode::update()
		{
			m_TempConnections.clear();
			copyConnectionsTo( m_TempConnections );
			std::for_each(m_TempConnections.begin(), m_TempConnections.end(), [this] (auto& conn)
			{
				GameConnection* gc = dynamic_cast<GameConnection*>(conn);
				if ( gc )
				{
					gc->beginPoll();
					Packet pack;
					while ( gc->poll(pack) )
					{
						handlePacket( pack, gc );
						delete [] pack.data;
					}
					gc->endPoll();
					updateConnecting( gc );
					updateKeepAlive( gc );
				}
			});
			removeConnectionsFrom( m_DeadConnections );
			m_DeadConnections.clear();
		}

		void GameNode::handlePacket(struct Packet& pack, class GameConnection* g)
		{
			EGameNodePacketType packType = (EGameNodePacketType)pack.data[0];
			switch (packType)
			{
				case EGameNodePacketType::ConnectAccept:
				{
					g->onReceiveConnectAccept();
					forEachCallback( m_ConnectResultCallbacks, [g] (auto& fcb) 
					{
						(fcb)( g, EConnectResult::Succes );
					});
				}
				break;
				case EGameNodePacketType::ConnectRequest:
				{
					g->sendConnectAccept();
					forEachCallback( m_NewConnectionCallbacks, [g] (auto& fcb)
					{
						(fcb)( g );
					});
				}
				break;
				case EGameNodePacketType::KeepAliveAnswer:
				{
					g->onReceiveKeepAliveAnswer();
				}
				break;
				case EGameNodePacketType::Rpc:
				{
					handleRpcPacket(pack, g);
				}
				break;
				default:
				{
					forEachCallback( m_CustomDataCallbacks, [g, &pack] (auto& fcb) 
					{ 
						// subtract -1 as id is included i the payload
						(fcb) (g, pack.data[0], pack.data + 1, pack.len-1, pack.channel);
					});
				}
				break;
			}
		}

		void GameNode::handleRpcPacket(struct Packet& pack, class GameConnection* g)
		{
			static const int kBuffSize=RPC_NAME_MAX_LENGTH*4;
			char name[kBuffSize];
			::memcpy( name, pack.data+1, RPC_NAME_MAX_LENGTH );
			char fname[kBuffSize];
			sprintf_s(fname, kBuffSize, "__rpc_deserialize_%s", name);
			void* pf = Platform::getPtrFromName( fname );
			if ( pf )
			{
				// function signature
				void (*pfunc)(const char*, int);
				pfunc = (decltype(pfunc)) pf;
				pfunc( pack.data + (1+RPC_NAME_MAX_LENGTH), pack.len-(1+RPC_NAME_MAX_LENGTH) ); // + 1 for id
			}
		}

		void GameNode::updateConnecting(class GameConnection* g)
		{
			if ( !g->updateConnecting(m_ConnectTimeoutMs) )
				return;
			if ( g->getState() == EConnectionState::ConnectingTimedOut )
			{
				forEachCallback( m_ConnectResultCallbacks, [g] (auto& fcb)
				{
					(fcb)( g, EConnectResult::Timedout );
				});
				// get rid of this connection, connecting timed out
				m_DeadConnections.emplace_back( g );
			}
		}

		void GameNode::updateKeepAlive(class GameConnection* g)
		{
			if ( !g->updateKeepAlive() )
				return;
			if ( g->getState() == EConnectionState::ConnectionTimedOut )
			{
				forEachCallback( m_DisconnectCallbacks, [g] (auto& fcb)
				{
					(fcb)( g, EDisconnectReason::Lost );
				});
				// get rid of this connection, connection timed out
				m_DeadConnections.emplace_back( g );
			}
		}

		bool GameNode::openSocket()
		{
			if ( m_SocketIsOpened )
				return true;
			// A node can connect to multiple addresses, but the socket should only open once
			m_SocketIsOpened = m_Socket->open();
			return m_SocketIsOpened;
		}

		bool GameNode::bindSocket(unsigned short port)
		{
			if ( m_SocketIsBound )
				return true;
			m_SocketIsBound = m_Socket->bind(port);
			return m_SocketIsBound;
		}
	}
}
