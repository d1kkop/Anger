#include "ConnectionNode.h"
#include "Connection.h"
#include "Socket.h"
#include "EndPoint.h"
#include "RpcMacros.h"


namespace Zerodelay
{
	extern ZEndpoint toZpt( const EndPoint& r );


	ConnectionNode::ConnectionNode(ERoutingMethod routingMethod, i32_t sendThreadSleepTimeMs, i32_t keepAliveIntervalSeconds, bool captureSocketErrors):
		RecvPoint(captureSocketErrors, sendThreadSleepTimeMs),
		m_RoutingMethod(routingMethod),
		m_IsServer(false),
		m_SocketIsOpened(false),
		m_SocketIsBound(false),
		m_KeepAliveIntervalSeconds(keepAliveIntervalSeconds),
		m_MaxIncomingConnections(32)
	{
	}

	ConnectionNode::~ConnectionNode()
	{
	}

	EConnectCallResult ConnectionNode::connect(const std::string& name, i32_t port, const std::string& pw, i32_t timeoutSeconds)
	{
		EndPoint endPoint;
		if ( !endPoint.resolve( name, port ) )
		{
			return EConnectCallResult::CannotResolveHost;
		}
		return connect( endPoint, pw, timeoutSeconds );
	}

	EConnectCallResult ConnectionNode::connect(const EndPoint& endPoint, const std::string& pw, i32_t timeoutSeconds)
	{
		if ( !m_ListenSocket )
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

		Connection* g;
		std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
		{
			if ( m_Connections.find( endPoint ) != m_Connections.end() )
			{
				return EConnectCallResult::AlreadyExists;
			}
			g = new Connection( true, endPoint, timeoutSeconds, m_KeepAliveIntervalSeconds );
			m_Connections.insert( std::make_pair( endPoint, g ) );
		}

		g->sendConnectRequest( pw );
		startThreads();
		return EConnectCallResult::Succes;
	}

	EListenCallResult ConnectionNode::listenOn(i32_t port, const std::string& pw)
	{
		if ( m_RoutingMethod == ERoutingMethod::ClientServer && m_IsServer )
		{
			return EListenCallResult::AlreadyStartedServer;
		}

		if ( !m_ListenSocket )
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

		setPassword( pw );
		startThreads();
		m_IsServer = true;
		return EListenCallResult::Succes;
	}

	EDisconnectCallResult ConnectionNode::disconnect(const EndPoint& endPoint)
	{
		Connection* conn = nullptr;
		{
			std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
			auto& it = m_Connections.find( endPoint );
			if ( it != m_Connections.end() )
			{
				conn = dynamic_cast<Connection*>(it->second);
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

	void ConnectionNode::disconnectAll()
	{
		// Copy to tempConnections2, as disconnectAll may be called from a callback function
		// in which case TempConnections is already in use.
		m_TempConnections2.clear();
		copyConnectionsTo( m_TempConnections2 );
		for ( auto* conn : m_TempConnections2 )
		{
			// Connection* gc = dynamic_cast<Connection*>(conn);
			Connection* gc = static_cast<Connection*>(conn); // TODO change to dynamic when more connection types are there
			if ( gc )
			{
				gc->disconnect();
			}
		}
	}

	i32_t ConnectionNode::getNumOpenConnections()
	{
		i32_t num = 0;
		m_TempConnections.clear();
		copyConnectionsTo( m_TempConnections );
		for ( auto* conn : m_TempConnections )
		{
			if ( conn->isConnected() )
			{
				num++;
			}
		}
		return num;
	}

	void ConnectionNode::update(std::function<void(const Packet&, IConnection*)> unhandledCb)
	{
		// Called from game thread
		m_TempConnections.clear();
		copyConnectionsTo( m_TempConnections );
		for ( auto* conn : m_TempConnections )
		{
			// Connection* gc = dynamic_cast<Connection*>(conn);
			Connection* gc = static_cast<Connection*>(conn); // TODO change to dynamic when more Connection types are there
			if ( gc )
			{
				gc->beginPoll();
				Packet pack;
				// connection can have become pending delete after it has processed the packet, in that case do no longer update states or call callbacks
				while ( !gc->isPendingDelete() && gc->poll(pack) ) 
				{
					// returns false if packet is not handled.
					if ( !recvPacket( pack, gc ) )
					{
						// pass unhandled packets through to othe Node systems
						unhandledCb( pack, gc );
					}
					delete [] pack.data;
				}
				gc->endPoll();
				updateConnecting( gc );				// implicitely checks if connection is not a pendingDelete, no callbacks called then
				updateKeepAlive( gc );				// same
				updateDisconnecting( gc );			// same
			}
		}
	#ifdef _DEBUG
		for ( auto& dc : m_DeadConnections )
		{
			assert( dc->isPendingDelete() );
		}
	#endif
		m_DeadConnections.clear();
	}

	void ConnectionNode::setPassword(const std::string& pw)
	{
		m_Password = pw;
	}

	void ConnectionNode::setMaxIncomingConnections(i32_t maxNumConnections)
	{
		m_MaxIncomingConnections = maxNumConnections;
	}

	void ConnectionNode::getConnectionListCopy(std::vector<ZEndpoint>& endpoints)
	{
		// Called from game thread
		m_TempConnections.clear();
		copyConnectionsTo( m_TempConnections );
		for ( auto& tc : m_TempConnections )
		{
			endpoints.emplace_back( toZpt( tc->getEndPoint() ) );
		}
	}

	ERoutingMethod ConnectionNode::getRoutingMethod() const
	{
		return m_RoutingMethod;
	}

	class IConnection* ConnectionNode::createNewConnection(const EndPoint& endPoint) const
	{
		return new Connection( false, endPoint, m_KeepAliveIntervalSeconds );
	}

	void ConnectionNode::removeConnection(const class Connection* g, const i8_t* fmt, ...)
	{
		auto* gg = const_cast<Connection*>(g);
		gg->setIsPendingDelete();
		m_DeadConnections.emplace_back(gg);
		if ( fmt )
		{
			i8_t buff[2048];
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

	void ConnectionNode::sendRemoteConnected(const Connection* g)
	{
		if ( m_RoutingMethod != ERoutingMethod::ClientServer || !isServer() )
			return; 
		auto& etp = g->getEndPoint();
		i8_t buff[128];
		i32_t offs = etp.write( buff, 128 );
		if ( offs < 0 )
		{
			removeConnection( g, "removing conn, serialization error %s", __FUNCTION__ );
			return;
		}
		// to all except
		send( (u8_t)EDataPacketType::RemoteConnected, buff, offs, &etp, true );
	}

	void ConnectionNode::sendRemoteDisconnected(const Connection* g, EDisconnectReason reason)
	{
		if ( m_RoutingMethod != ERoutingMethod::ClientServer || !isServer() )
			return; // relay message if wanted
		auto& etp = g->getEndPoint();
		i8_t buff[128];
		i32_t offs = etp.write( buff, 128 );
		if ( offs >= 0 )
		{
			buff[offs] = (u8_t)reason;
		}
		else
		{
			removeConnection( g, "removing conn, serialization error %s", __FUNCTION__ );
			return;
		}
		// to all except
		send( (u8_t)EDataPacketType::RemoteDisconnected, buff, offs+1, &etp, true );
	}

	bool ConnectionNode::recvPacket(struct Packet& pack, class Connection* g)
	{
		EDataPacketType packType = (EDataPacketType)pack.data[0];
		const i8_t* payload  = pack.data+1; // first byte is PacketType
		i32_t payloadLen = pack.len-1;  // len includes the packetType byte
		switch (packType)
		{
		case EDataPacketType::ConnectRequest:
			recvConnectPacket(payload, payloadLen, g);
			break;
		case EDataPacketType::ConnectAccept:
			recvConnectAccept(g);
			break;
		case EDataPacketType::Disconnect:
			recvDisconnectPacket(payload, payloadLen, g);
			break;
		case EDataPacketType::RemoteConnected:
			recvRemoteConnected(g, payload, payloadLen);
			break;
		case EDataPacketType::RemoteDisconnected:
			recvRemoteDisconnected(g, payload, payloadLen);
			break;
		case EDataPacketType::KeepAliveRequest:
			g->onReceiveKeepAliveRequest();
			break;
		case EDataPacketType::KeepAliveAnswer:
			g->onReceiveKeepAliveAnswer();
			break;
		case EDataPacketType::IncorrectPassword:
			recvInvalidPassword(g, payload, payloadLen);
			break;
		case EDataPacketType::MaxConnectionsReached:
			recvMaxConnectionsReached(g, payload, payloadLen);
			break;
		case EDataPacketType::Rpc:
			recvRpcPacket(payload, payloadLen, g);
			break;
		default:
			if ( (u8_t)packType >= USER_ID_OFFSET )
			{
				recvUserPacket(g, pack );
			}
			else
			{
				// unhandled packet
				return false;
			}
			break;
		}
		return true;
	}

	void ConnectionNode::recvConnectPacket(const i8_t* payload, i32_t payloadLen, class Connection* g)
	{
		static const i32_t kBuffSize=1024;
		i8_t pw[kBuffSize];
		if ( !ISocket::readString( pw, kBuffSize, payload, payloadLen ))
		{
			removeConnection( g, "removing conn, serialization error %s", __FUNCTION__ );
			return; // invalid serialization
		}
		if ( strcmp( m_Password.c_str(), pw ) != 0 )
		{
			g->sendIncorrectPassword();
			removeConnection( g, "removing conn, invalid pw %s", g->getEndPoint().asString().c_str() );
		}
		else
		{
			i32_t numConnections;
			{
				std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
				numConnections = (i32_t)m_Connections.size();
			}
			if ( numConnections >= m_MaxIncomingConnections+1 )
			{
				g->sendMaxConnectionsReached();
				removeConnection( g, "removing conn, max number of connections reached %s", g->getEndPoint().asString().c_str() );
				return;
			}
			// All fine..
			if (g->sendConnectAccept())
			{
				forEachCallback(m_NewConnectionCallbacks, [&](auto& fcb)
				{
					(fcb)(g->getEndPoint());
				});
				sendRemoteConnected( g );
				Platform::log( "%s connected", g->getEndPoint().asString().c_str() );
			}
			else
			{
				removeConnection( g, "removing conn, unexpected state %s", __FUNCTION__ );
			}
		}
	}

	void ConnectionNode::recvConnectAccept(class Connection* g)
	{
		if (g->onReceiveConnectAccept())
		{
			forEachCallback(m_ConnectResultCallbacks, [&](auto& fcb)
			{
				(fcb)(g->getEndPoint(), EConnectResult::Succes);
			});
			Platform::log( "accepted connection %s", g->getEndPoint().asString().c_str() );
		}
		else
		{
			removeConnection ( g, "removing conn, unexpected state %s", __FUNCTION__ );
		}
	}

	void ConnectionNode::recvDisconnectPacket(const i8_t* payload, i32_t len, class Connection* g)
	{
		if ( g->acceptDisconnect() )
		{
			sendRemoteDisconnected( g, EDisconnectReason::Closed );
			forEachCallback(m_DisconnectCallbacks, [&](auto& fcb)
			{
				(fcb)(true, g->getEndPoint(), EDisconnectReason::Closed);
			});
			removeConnection(g, "disconnect received, removing connecting..%s", g->getEndPoint().asString().c_str());
		}
		else
		{
			removeConnection( g, "removing conn, unexpected state %s", __FUNCTION__ );
		}
	}

	void ConnectionNode::recvRemoteConnected(class Connection* g, const i8_t* payload, i32_t payloadLen)
	{
		EndPoint etp;
		if (g->onReceiveRemoteConnected(payload, payloadLen, etp))
		{
			forEachCallback(m_NewConnectionCallbacks, [&](auto& fcb)
			{
				(fcb)(etp);
			});
			Platform::log( "remote %s connected", etp.asString().c_str() );
		}
		else
		{
			removeConnection( g, "removing conn, serialization fail %s", __FUNCTION__ );
		}
	}

	void ConnectionNode::recvRemoteDisconnected(class Connection* g, const i8_t* payload, i32_t payloadLen)
	{
		EndPoint etp;
		EDisconnectReason reason;
		if (g->onReceiveRemoteDisconnected(payload, payloadLen, etp, reason))
		{
			forEachCallback(m_DisconnectCallbacks, [&](auto& fcb)
			{
				assert ( etp != g->getEndPoint() && "received remote disc for ourselves..");
				(fcb)(false, etp, reason);
			});
			Platform::log( "remote %s disconnected", etp.asString().c_str() );
		}
		else
		{
			removeConnection( g, "removing conn, serialization fail or unexpected state %s", __FUNCTION__ );
		}
	}

	void ConnectionNode::recvInvalidPassword(class Connection* g, const i8_t* payload, i32_t payloadLen)
	{
		if ( g->setInvalidPassword() )
		{
			forEachCallback(m_ConnectResultCallbacks, [&](auto& fcb)
			{
				(fcb)(g->getEndPoint(), EConnectResult::InvalidPassword);
			});
			removeConnection( g, "removing conn %s, invalid pw", g->getEndPoint().asString().c_str() );
		}
		else
		{
			removeConnection( g, "removing conn, unexpected state %s", __FUNCTION__ );
		}
	}

	void ConnectionNode::recvMaxConnectionsReached(class Connection* g, const i8_t* payload, i32_t payloadLen)
	{
		if ( g->setMaxConnectionsReached() )
		{
			forEachCallback(m_ConnectResultCallbacks, [&](auto& fcb)
			{
				(fcb)(g->getEndPoint(), EConnectResult::MaxConnectionsReached);
			});
			removeConnection( g, "removing conn %s, max connections reached", g->getEndPoint().asString().c_str() );
		}
		else
		{
			removeConnection( g, "removing conn, unexpected state %s", __FUNCTION__ );
		}
	}

	void ConnectionNode::recvRpcPacket(const i8_t* payload, i32_t len, class Connection* g)
	{
		i8_t name[RPC_NAME_MAX_LENGTH];
		if ( !ISocket::readFixed( name, RPC_NAME_MAX_LENGTH, payload, (RPC_NAME_MAX_LENGTH<len?RPC_NAME_MAX_LENGTH:len)) )
		{
			removeConnection( g, "rpc serialiation error %s", __FUNCTION__ );
			return;
		}
		i8_t fname[RPC_NAME_MAX_LENGTH*2];
	#if _WIN32
		sprintf_s(fname, RPC_NAME_MAX_LENGTH*2, "__rpc_deserialize_%s", name);
	#else	
		sprintf(fname, "__rpc_deserialize_%s", name);
	#endif
		void* pf = Platform::getPtrFromName( fname );
		if ( pf )
		{
			// function signature
			void (*pfunc)(const i8_t*, i32_t);
			pfunc = (decltype(pfunc)) pf;
			pfunc( payload, len );
		}
		else
		{
			removeConnection( g, "removing conn, rpc function %s not found %s", fname, __FUNCTION__ );
		}
	}

	void ConnectionNode::recvUserPacket(class Connection* g, const Packet& pack)
	{
		if ( pack.relay && m_RoutingMethod == ERoutingMethod::ClientServer && isServer() ) // send through to others
		{
			// except self
			send( pack.data[0], pack.data+1, pack.len-1, &g->getEndPoint(), true, pack.type, pack.channel, false /* relay only once */ );
		}
		forEachCallback(m_CustomDataCallbacks, [&](auto& fcb)
		{
			(fcb)(g->getEndPoint(), pack.data[0], pack.data+1, pack.len-1, pack.channel);
		});
	}

	void ConnectionNode::updateConnecting(class Connection* g)
	{
		if ( g->updateConnecting() ) // Else, already connected or timed out
		{
			if ( g->getState() == EConnectionState::InitiateTimedOut )
			{
				if ( !g->isPendingDelete() ) // other updates such as updateKeepAlive or updateDisconnecting may set it to pendingDelete, no longer invoke callbacks then
				{
					forEachCallback( m_ConnectResultCallbacks, [g] (auto& fcb)
					{
						(fcb)( g->getEndPoint(), EConnectResult::Timedout );
					});
				}
				removeConnection( g, "removing conn, connecting timed out %s", g->getEndPoint().asString().c_str() );
			}
		}
	}

	void ConnectionNode::updateKeepAlive(class Connection* g)
	{
		if ( g->updateKeepAlive() ) // Else arleady timed out, just not timed out, or no keep alive is managed
		{
			if ( g->getState() == EConnectionState::ConnectionTimedOut )
			{
				sendRemoteDisconnected( g, EDisconnectReason::Lost );
				if ( !g->isPendingDelete() ) // other updates such as updateConnecting or updateDisconnecting may set it to pendingDelete, no longer invoke callbacks then
				{
					forEachCallback( m_DisconnectCallbacks, [g] (auto& fcb)
					{
						(fcb)( true, g->getEndPoint(), EDisconnectReason::Lost );
					});
				}
				removeConnection( g, "removing conn, connection was lost %s", g->getEndPoint().asString().c_str() );
			}
		}
	}

	void ConnectionNode::updateDisconnecting(class Connection* g)
	{
		if ( g->updateDisconnecting() ) // Else, state already handled or not in disconnecting state
		{
			if ( g->getState() == EConnectionState::Disconnected ) // if linger state is passed, state is set to disconnected
			{
				if ( !g->isPendingDelete() ) // other updates such as updateConnecting or updateKeepAlive may set it to pendingDelete, no longer invoke callbacks then
				{
					forEachCallback( m_DisconnectCallbacks, [g] (auto& fcb)
					{
						(fcb)( true, g->getEndPoint(), EDisconnectReason::Closed );
					});
				}
				removeConnection( g, "removing conn %s, disonnected gracefully", g->getEndPoint().asString().c_str() );
			}
		}
	}

	bool ConnectionNode::openSocket()
	{
		if ( m_SocketIsOpened )
			return true;
		// A node can connect to multiple addresses, but the socket should only open once
		m_SocketIsOpened = m_ListenSocket->open();
		return m_SocketIsOpened;
	}

	bool ConnectionNode::bindSocket(u16_t port)
	{
		if ( m_SocketIsBound )
			return true;
		m_SocketIsBound = m_ListenSocket->bind(port);
		return m_SocketIsBound;
	}
}
