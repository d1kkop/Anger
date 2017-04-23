#include "GameNode.h"
#include "GameConnection.h"
#include "Socket.h"
#include "EndPoint.h"
#include "RpcMacros.h"


namespace Motor
{
	namespace Anger
	{
		GameNode::GameNode(int connectTimeoutSeconds, int sendThreadSleepTimeMs, int keepAliveIntervalSeconds, bool captureSocketErrors):
			RecvPoint(captureSocketErrors, sendThreadSleepTimeMs),
			m_SocketIsOpened(false),
			m_SocketIsBound(false),
			m_ConnectTimeoutMs(connectTimeoutSeconds*1000),
			m_KeepAliveIntervalSeconds(keepAliveIntervalSeconds),
			m_IsServer(false),
			m_InternalError(EInternalError::Succes)
		{
		}

		GameNode::~GameNode()
		{
		}

		EConnectCallResult GameNode::connect(const std::string& name, int port, const std::string& pw)
		{
			EndPoint endPoint;
			if ( !endPoint.resolve( name, port ) )
			{
				return EConnectCallResult::CannotResolveHost;
			}
			return connect( endPoint );
		}

		Motor::Anger::EConnectCallResult GameNode::connect(const EndPoint& endPoint, const std::string& pw)
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
				g = new GameConnection( endPoint, m_KeepAliveIntervalSeconds );
				m_Connections.insert( std::make_pair( endPoint, g ) );
			}

			g->sendConnectRequest( pw );
			startThreads();
			return EConnectCallResult::Succes;
		}

		EListenCallResult GameNode::listenOn(int port, const std::string& pw)
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

			setServerPassword( pw );
			startThreads();
			return EListenCallResult::Succes;
		}

		EDisconnectCallResult GameNode::disconnect(const EndPoint& endPoint)
		{
			GameConnection* conn = nullptr;
			{
				std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
				auto& it = m_Connections.find( endPoint );
				if ( it != m_Connections.end() )
				{
					conn = dynamic_cast<GameConnection*>(it->second);
				}
			}
			if ( conn )
			{
				if ( conn->disconnect() )
				{
					return EDisconnectCallResult::Succes;
				}
				return EDisconnectCallResult::AlreadyCalled;
			}
			return EDisconnectCallResult::UnknownEndpoint;
		}

		void GameNode::disconnectAll()
		{
			m_TempConnections.clear();
			copyConnectionsTo( m_TempConnections );
			for ( auto* conn : m_TempConnections )
			{
				GameConnection* gc = dynamic_cast<GameConnection*>(conn);
				if ( gc )
				{
					gc->disconnect();
				}
			}
		}

		void GameNode::update()
		{
			m_TempConnections.clear();
			copyConnectionsTo( m_TempConnections );
			for ( auto* conn : m_TempConnections )
			{
				GameConnection* gc = dynamic_cast<GameConnection*>(conn);
				if ( gc )
				{
					gc->beginPoll();
					Packet pack;
					while ( gc->poll(pack) )
					{
						recvPacket( pack, gc );
						delete [] pack.data;
					}
					gc->endPoll();
					updateConnecting( gc );
					updateKeepAlive( gc );
					updateDisconnecting( gc );
				}
			}
			markIsPendingDelete( m_DeadConnections );
			m_DeadConnections.clear();
		}

		void GameNode::setIsTrueServer(bool is)
		{
			m_IsServer = is;
		}

		void GameNode::setServerPassword(const std::string& pw)
		{
			m_ServerPassword = pw;
		}

		class IConnection* GameNode::createNewConnection(const EndPoint& endPoint) const
		{
			return new GameConnection( endPoint, m_KeepAliveIntervalSeconds );
		}

		void GameNode::sendRemoteConnected(const GameConnection* g)
		{
			auto& etp = g->getEndPoint();
			char buff[128];
			int offs = etp.write( buff, 128 );
			if ( offs < 0 )
			{
				m_InternalError = EInternalError::BufferTooShort;
				return;
			}
			beginSend( &etp, true );
			send( (unsigned char)EGameNodePacketType::RemoteConnected, buff, offs );
			endSend();
		}

		void GameNode::sendRemoteDisconnected(const GameConnection* g, EDisconnectReason reason)
		{
			auto& etp = g->getEndPoint();
			char buff[128];
			int offs = etp.write( buff, 128 );
			if ( offs >= 0 )
			{
				buff[offs] = (unsigned char)reason;
			}
			else
			{
				m_InternalError = EInternalError::BufferTooShort;
				return;
			}
			beginSend( &etp, true );
			send( (unsigned char)EGameNodePacketType::RemoteDisconnected, buff, offs+1 );
			endSend();
		}

		void GameNode::recvPacket(struct Packet& pack, class GameConnection* g)
		{
			EGameNodePacketType packType = (EGameNodePacketType)pack.data[0];
			char* payload  = pack.data+1; // first byte is PacketType
			int payloadLen = pack.len-1;  // len includes the packetType byte
			switch (packType)
			{
				case EGameNodePacketType::ConnectAccept:
				{
					if ( g->onReceiveConnectAccept() )
					{
						forEachCallback( m_ConnectResultCallbacks, [&] (auto& fcb)
						{
							(fcb)( g, EConnectResult::Succes );
						});
					}
				}
				break;
				case EGameNodePacketType::RemoteConnected:
				{
					EndPoint etp;
					if ( g->onReceiveRemoteConnected( payload, payloadLen, etp ) )
					{
						forEachCallback( m_NewConnectionCallbacks, [&] (auto& fcb)
						{
							(fcb)( g, etp );
						});
					}
				}
				break;
				case EGameNodePacketType::RemoteDisconnected:
				{
					EndPoint etp;
					EDisconnectReason reason;
					if ( g->onReceiveRemoteDisconnected( payload, payloadLen, etp, reason ) )
					{
						forEachCallback( m_DisconnectCallbacks, [&] (auto& fcb)
						{
							(fcb)( g, etp, reason );
						});
					}
				}
				break;
				case EGameNodePacketType::ConnectRequest:
				{
					recvConnectPacket(payload, payloadLen, g);
				}
				break;
				case EGameNodePacketType::Disconnect:
				{
					recvDisconnectPacket(payload, payloadLen, g);
				}
				break;
				case EGameNodePacketType::KeepAliveAnswer:
				{
					g->onReceiveKeepAliveAnswer();
				}
				break;
				case EGameNodePacketType::Rpc:
				{
					recvRpcPacket(payload, payloadLen, g);
				}
				break;
				default: // custom user data
				{
					forEachCallback( m_CustomDataCallbacks, [&] (auto& fcb) 
					{
						(fcb) (g, pack.data[0], payload, payloadLen, pack.channel);
					});
				}
				break;
			}
		}

		void GameNode::recvConnectPacket(const char* payload, int payloadLen, class GameConnection* g)
		{
			static const int kBuffSize=1024;
			char pw[kBuffSize];
			if ( !ISocket::readString( pw, kBuffSize, payload, payloadLen ))
			{
				m_InternalError = EInternalError::BufferTooShort;
				return; // invalid serialization
			}
			if ( strcmp( m_ServerPassword.c_str(), pw ) != 0 )
			{
				g->sendIncorrectPassword();
			}
			else
			{
				if (g->sendConnectAccept() && m_IsServer )
				{
					sendRemoteConnected( g );
				}
			}
		}

		void GameNode::recvDisconnectPacket(const char* payload, int len, class GameConnection* g)
		{
			if ( !g->acceptDisconnect() )
				return; // invalid state
			sendRemoteDisconnected( g, EDisconnectReason::Closed );
		}

		void GameNode::recvRpcPacket(const char* payload, int len, class GameConnection* g)
		{
			char name[RPC_NAME_MAX_LENGTH];
			if ( !ISocket::readFixed( name, RPC_NAME_MAX_LENGTH, payload, (RPC_NAME_MAX_LENGTH<len?RPC_NAME_MAX_LENGTH:len)) )
			{
				m_InternalError = EInternalError::BufferTooShort;
				return;
			}
			char fname[RPC_NAME_MAX_LENGTH*2];
			sprintf_s(fname, RPC_NAME_MAX_LENGTH*2, "__rpc_deserialize_%s", name);
			void* pf = Platform::getPtrFromName( fname );
			if ( pf )
			{
				// function signature
				void (*pfunc)(const char*, int);
				pfunc = (decltype(pfunc)) pf;
				pfunc( payload + RPC_NAME_MAX_LENGTH, len - RPC_NAME_MAX_LENGTH );
			}
		}

		void GameNode::updateConnecting(class GameConnection* g)
		{
			if ( !g->updateConnecting(m_ConnectTimeoutMs) ) // invalid state
				return;
			if ( g->getState() == EConnectionState::InitiateTimedOut )
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
			if ( !g->updateKeepAlive() ) // invalid state
				return;
			if ( g->getState() == EConnectionState::ConnectionTimedOut )
			{
				sendRemoteDisconnected( g, EDisconnectReason::Lost );
				forEachCallback( m_DisconnectCallbacks, [g] (auto& fcb)
				{
					(fcb)( g, g->getEndPoint(), EDisconnectReason::Lost );
				});
				// get rid of this connection, connection timed out
				m_DeadConnections.emplace_back( g );
			}
		}

		void GameNode::updateDisconnecting(class GameConnection* g)
		{
			if ( !g->updateDisconnecting() ) // invalid state
				return;
			// If linger time is passed, state is changed to disconnected
			if ( g->getState() == EConnectionState::Disconnected )
			{
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
