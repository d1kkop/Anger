#include "GameNode.h"
#include "GameConnection.h"
#include "Socket.h"
#include "EndPoint.h"
#include "RpcMacros.h"


namespace Zeroone
{
	GameNode::GameNode(int connectTimeoutSeconds, int sendThreadSleepTimeMs, int keepAliveIntervalSeconds, bool captureSocketErrors):
		RecvPoint(captureSocketErrors, sendThreadSleepTimeMs),
		m_SocketIsOpened(false),
		m_SocketIsBound(false),
		m_ConnectTimeoutMs(connectTimeoutSeconds*1000),
		m_KeepAliveIntervalSeconds(keepAliveIntervalSeconds),
		m_IsServer(false)
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

	EConnectCallResult GameNode::connect(const EndPoint& endPoint, const std::string& pw)
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
				if ( conn->isPendingDelete() ) // dont allow a pending for delete connection to work on
				{
					conn = nullptr;
				}
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
		m_TempConnections2.clear();
		copyConnectionsTo( m_TempConnections2 );
		for ( auto* conn : m_TempConnections2 )
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

	void GameNode::removeConnection(const class GameConnection* g, const char* fmt, ...)
	{
		m_DeadConnections.emplace_back( const_cast<GameConnection*>(g) );
		if ( fmt )
		{
			char buff[2048];
			va_list myargs;
			va_start(myargs, fmt);
		#if _WIN32
			vsprintf_s(buff, 2048, fmt, myargs);
		#else
			vsprintf(buff, fmt, myargs);
		#endif
			va_end(myargs);

			Platform::log("%s", buff);
		}
	}

	void GameNode::sendRemoteConnected(const GameConnection* g)
	{
		if ( !m_IsServer )
			return; // only if is true server, relay message
		auto& etp = g->getEndPoint();
		char buff[128];
		int offs = etp.write( buff, 128 );
		if ( offs < 0 )
		{
			removeConnection( g, "removing conn, serialization error %s", __FUNCTION__ );
			return;
		}
		beginSend( &etp, true ); // to all except
		send( (unsigned char)EGameNodePacketType::RemoteConnected, buff, offs );
		endSend();
	}

	void GameNode::sendRemoteDisconnected(const GameConnection* g, EDisconnectReason reason)
	{
		if ( !m_IsServer )
			return; // only if is true server, relay message
		auto& etp = g->getEndPoint();
		char buff[128];
		int offs = etp.write( buff, 128 );
		if ( offs >= 0 )
		{
			buff[offs] = (unsigned char)reason;
		}
		else
		{
			removeConnection( g, "removing conn, serialization error %s", __FUNCTION__ );
			return;
		}
		beginSend( &etp, true ); // to all except
		send( (unsigned char)EGameNodePacketType::RemoteDisconnected, buff, offs+1 );
		endSend();
	}

	void GameNode::recvPacket(struct Packet& pack, class GameConnection* g)
	{
		EGameNodePacketType packType = (EGameNodePacketType)pack.data[0];
		const char* payload  = pack.data+1; // first byte is PacketType
		int payloadLen = pack.len-1;  // len includes the packetType byte
		switch (packType)
		{
		case EGameNodePacketType::ConnectRequest:
			recvConnectPacket(payload, payloadLen, g);
			break;
		case EGameNodePacketType::ConnectAccept:
			recvConnectAccept(g);
			break;
		case EGameNodePacketType::Disconnect:
			recvDisconnectPacket(payload, payloadLen, g);
			break;
		case EGameNodePacketType::RemoteConnected:
			recvRemoteConnected(g, payload, payloadLen);
			break;
		case EGameNodePacketType::RemoteDisconnected:
			recvRemoteDisconnected(g, payload, payloadLen);
			break;
		case EGameNodePacketType::KeepAliveAnswer:
			g->onReceiveKeepAliveAnswer();
			break;
		case EGameNodePacketType::IncorrectPassword:
			recvInvalidPassword(g, payload, payloadLen);
			break;
		case EGameNodePacketType::Rpc:
			recvRpcPacket(payload, payloadLen, g);
			break;
		default:
			recvUserPacket(g, payload, payloadLen, pack.channel);
			break;
		}
	}

	void GameNode::recvConnectPacket(const char* payload, int payloadLen, class GameConnection* g)
	{
		static const int kBuffSize=1024;
		char pw[kBuffSize];
		if ( !ISocket::readString( pw, kBuffSize, payload, payloadLen ))
		{
			removeConnection( g, "removing conn, serialization error %s", __FUNCTION__ );
			return; // invalid serialization
		}
		if ( strcmp( m_ServerPassword.c_str(), pw ) != 0 )
		{
			g->sendIncorrectPassword();
			removeConnection( g, "removing conn, invalid pw %s", __FUNCTION__ );
		}
		else
		{
			if (g->sendConnectAccept())
			{
				forEachCallback(m_NewConnectionCallbacks, [&](auto& fcb)
				{
					(fcb)(g->getEndPoint());
				});
				sendRemoteConnected( g );
			}
			else
			{
				removeConnection( g, "removing conn, unexpected state %s", __FUNCTION__ );
			}
		}
	}

	void GameNode::recvConnectAccept(class GameConnection* g)
	{
		if (g->onReceiveConnectAccept())
		{
			forEachCallback(m_ConnectResultCallbacks, [&](auto& fcb)
			{
				(fcb)(g->getEndPoint(), EConnectResult::Succes);
			});
		}
		else
		{
			removeConnection ( g, "removing conn, unexpected state %s", __FUNCTION__ );
		}
	}

	void GameNode::recvDisconnectPacket(const char* payload, int len, class GameConnection* g)
	{
		if ( g->acceptDisconnect() )
		{
			if ( m_IsServer )
			{	// if true server, relay message to all
				sendRemoteDisconnected( g, EDisconnectReason::Closed );
			}
			removeConnection(g, nullptr);
		}
		else
		{
			removeConnection( g, "removing conn, unexpected state %s", __FUNCTION__ );
		}
	}

	void GameNode::recvRemoteConnected(class GameConnection* g, const char* payload, int payloadLen)
	{
		EndPoint etp;
		if (g->onReceiveRemoteConnected(payload, payloadLen, etp))
		{
			forEachCallback(m_NewConnectionCallbacks, [&](auto& fcb)
			{
				(fcb)(etp);
			});
		}
		else
		{
			removeConnection( g, "removing conn, serialization fail %s", __FUNCTION__);
		}
	}

	void GameNode::recvRemoteDisconnected(class GameConnection* g, const char* payload, int payloadLen)
	{
		EndPoint etp;
		EDisconnectReason reason;
		if (g->onReceiveRemoteDisconnected(payload, payloadLen, etp, reason))
		{
			forEachCallback(m_DisconnectCallbacks, [&](auto& fcb)
			{
				(fcb)(g->getEndPoint() == etp, etp, reason);
			});
		}
		else
		{
			removeConnection( g, "removing conn, serialization fail or unexpected state %s", __FUNCTION__ );
		}
	}

	void GameNode::recvInvalidPassword(class GameConnection* g, const char* payload, int payloadLen)
	{
		if ( g->setInvalidPassword() )
		{
			forEachCallback(m_ConnectResultCallbacks, [&](auto& fcb)
			{
				(fcb)(g->getEndPoint(), EConnectResult::InvalidPassword);
			});
		}
		else
		{
			removeConnection( g, "removing conn, unexpected state %s", __FUNCTION__ );
		}
	}

	void GameNode::recvRpcPacket(const char* payload, int len, class GameConnection* g)
	{
		char name[RPC_NAME_MAX_LENGTH];
		if ( !ISocket::readFixed( name, RPC_NAME_MAX_LENGTH, payload, (RPC_NAME_MAX_LENGTH<len?RPC_NAME_MAX_LENGTH:len)) )
		{
			removeConnection( g, "rpc serialiation error %s", __FUNCTION__ );
			return;
		}
		char fname[RPC_NAME_MAX_LENGTH*2];
	#if _WIN32
		sprintf_s(fname, RPC_NAME_MAX_LENGTH*2, "__rpc_deserialize_%s", name);
	#else	
		sprintf(fname, "__rpc_deserialize_%s", name);
	#endif
		void* pf = Platform::getPtrFromName( fname );
		if ( pf )
		{
			// function signature
			void (*pfunc)(const char*, int);
			pfunc = (decltype(pfunc)) pf;
			pfunc( payload + RPC_NAME_MAX_LENGTH, len - RPC_NAME_MAX_LENGTH );
		}
		else
		{
			removeConnection( g, "removing conn, rpc function %s not found %s", fname, __FUNCTION__ );
		}
	}

	void GameNode::recvUserPacket(class GameConnection* g, const char* payload, int payloadLen, unsigned char channel)
	{
		forEachCallback(m_CustomDataCallbacks, [&](auto& fcb)
		{
			(fcb)(g->getEndPoint(), *(payload-1), payload, payloadLen, channel);
		});
	}

	void GameNode::updateConnecting(class GameConnection* g)
	{
		g->updateConnecting(m_ConnectTimeoutMs);
		if ( g->getState() == EConnectionState::InitiateTimedOut )
		{
			forEachCallback( m_ConnectResultCallbacks, [g] (auto& fcb)
			{
				(fcb)( g->getEndPoint(), EConnectResult::Timedout );
			});
			removeConnection( g, "removing conn, connecting timed out %s", __FUNCTION__ );
		}
	}

	void GameNode::updateKeepAlive(class GameConnection* g)
	{
		g->updateKeepAlive();
		if ( g->getState() == EConnectionState::ConnectionTimedOut )
		{
			sendRemoteDisconnected( g, EDisconnectReason::Lost );
			forEachCallback( m_DisconnectCallbacks, [g] (auto& fcb)
			{
				(fcb)( true, g->getEndPoint(), EDisconnectReason::Lost );
			});
			removeConnection( g, "removing conn, connection was lost %s", __FUNCTION__ );
		}
	}

	void GameNode::updateDisconnecting(class GameConnection* g)
	{
		g->updateDisconnecting();
		if ( g->getState() == EConnectionState::Disconnected ) // if linger state is passed, state is set to disconnected
		{
			removeConnection( g, "removing conn, disonnected gracefully" );
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
