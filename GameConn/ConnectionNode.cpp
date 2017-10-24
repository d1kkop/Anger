#include "ConnectionNode.h"
#include "Connection.h"
#include "Socket.h"
#include "EndPoint.h"
#include "RpcMacros.h"
#include "CoreNode.h"
#include "RecvNode.h"
#include "Util.h"


namespace Zerodelay
{
	extern ZEndpoint toZpt( const EndPoint& r );


	ConnectionNode::ConnectionNode(i32_t keepAliveIntervalSeconds):
		m_DispatchNode(nullptr),
		m_ProcessingConnection(nullptr),
		m_IsServer(false),
		m_KeepAliveIntervalSeconds(keepAliveIntervalSeconds),
		m_MaxIncomingConnections(32)
	{
	}

	ConnectionNode::~ConnectionNode()
	{
	}

	void ConnectionNode::postInitialize(CoreNode* coreNode)
	{
		assert(!m_CoreNode && !m_DispatchNode);
		m_CoreNode = coreNode;
		m_DispatchNode = coreNode->rn();
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
		if ( !m_DispatchNode->openSocketOnPort(0) )
		{
			return EConnectCallResult::SocketError; 
		}

		if ( m_Connections.find( endPoint ) != m_Connections.end() )
		{
			return EConnectCallResult::AlreadyExists;
		}
		Connection* g = new Connection( this, true, nullptr, timeoutSeconds, m_KeepAliveIntervalSeconds );
		m_Connections.insert( std::make_pair( endPoint, g ) );
		g->sendConnectRequest( pw );
		return EConnectCallResult::Succes;
	}

	EListenCallResult ConnectionNode::listenOn(i32_t port, const std::string& pw)
	{
		if ( m_CoreNode->getRoutingMethod() == ERoutingMethod::ClientServer && m_IsServer )
		{
			return EListenCallResult::AlreadyStartedServer;
		}

		if ( !m_DispatchNode->openSocketOnPort(port) )
		{
			return EListenCallResult::SocketError;
		}

		setPassword( pw );
		m_IsServer = true;
		return EListenCallResult::Succes;
	}

	EDisconnectCallResult ConnectionNode::disconnect(const EndPoint& endPoint, bool sendDisconnect)
	{
		// check if exists
		auto& it = m_Connections.find( endPoint );
		if ( it != m_Connections.end() )
		{
			Connection* conn = it->second;
			if ( conn->disconnect(sendDisconnect) ) // returns false if connection is not in 'connected' state
			{
				return EDisconnectCallResult::Succes;
			}
			return EDisconnectCallResult::NotConnected;
		}
		return EDisconnectCallResult::UnknownEndpoint;
	}

	void ConnectionNode::disconnectAll(bool sendDisconnect)
	{
		for ( auto it : m_Connections )
		{
			assert( it.second );
			if ( it.second ) 
			{
				it.second->disconnect(sendDisconnect);
			}
		}
	}

	i32_t ConnectionNode::getNumOpenConnections()
	{
		i32_t num = 0;
		for ( auto& kvp : m_Connections )
		{
			Connection* c = kvp.second;
			if ( c && c->isConnected() )
			{
				num++;
			}
		}
		return num;
	}

	//void ConnectionNode::update(std::function<void(const Packet&, IConnection*)> unhandledCb)
	//{
	//	// Called from game thread
	//	m_TempConnections.clear();
	//	copyConnectionsTo( m_TempConnections );
	//	for ( auto* conn : m_TempConnections )
	//	{
	//		// Connection* gc = dynamic_cast<Connection*>(conn);
	//		Connection* gc = static_cast<Connection*>(conn); // TODO change to dynamic when more Connection types are there
	//		if ( gc )
	//		{
	//			gc->beginPoll();
	//			Packet pack;
	//			// connection can have become pending delete after it has processed the packet, in that case do no longer update states or call callbacks
	//			while ( !gc->isPendingDelete() && gc->poll(pack) ) 
	//			{
	//				// all possible packet types that can be processed by higher level systems than rudp handler
	//				if ( (pack.type == EHeaderPacketType::Reliable_Ordered) || (pack.type == EHeaderPacketType::Unreliable_Sequenced) || 
	//					 (pack.type == EHeaderPacketType::Reliable_Newest) )
	//				{
	//					// returns false if packet is not handled.
	//					if ( (pack.type == EHeaderPacketType::Reliable_Newest) || !recvPacket( pack, gc ) )
	//					{
	//						// pass unhandled packets through to other Node systems
	//						unhandledCb( pack, gc );
	//					}
	//				}
	//				else
	//				{
	//					Platform::log("ERROR: invalid packet forwared to higher level packet processors");
	//					assert( false && "invalid packet forward to higher level packet processors" );
	//				}
	//				delete [] pack.data;
	//			}
	//			gc->endPoll();
	//			updateConnecting( gc );				// implicitely checks if connection is not a pendingDelete, no callbacks called then
	//			updateKeepAlive( gc );				// same
	//			updateDisconnecting( gc );			// same
	//		}
	//	}
	//}

	void ConnectionNode::update()
	{
		for ( auto& kvp : m_Connections )
		{
			Connection* c = kvp.second;
			if (!c) continue;
			updateConnecting( c );
			updateKeepAlive( c );	
			updateDisconnecting( c );
		}
	}

	bool ConnectionNode::beginProcessPacketsFor(const EndPoint& endPoint)
	{
		assert(!m_ProcessingConnection);
		auto it = m_Connections.find(endPoint);
		if ( it != m_Connections.end() )
		{
			m_ProcessingConnection = it->second;
			return true;
		}
		return false;
	}

	bool ConnectionNode::processPacket(const Packet& pack)
	{
		assert(m_ProcessingConnection);
		if (!m_ProcessingConnection) 
			return false;
		if ( pack.type == EHeaderPacketType::Reliable_Ordered )  // all connection node packets are reliable ordered
		{
			// returns false if packet was not consumed (handled)
			return recvPacket( pack, m_ProcessingConnection );
		}
		else
		{
			Platform::log("WARNING: invalid packet forwarded to higher level packet processors");
			assert( false && "invalid packet forward to higher level packet processors" );
		}
		return false;
	}

	void ConnectionNode::endProcessPackets()
	{
		m_ProcessingConnection = nullptr;
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
		for ( auto& kvp : m_Connections )
		{
			Connection* c = kvp.second;
			if ( c && c->isConnected() )
			{
				endpoints.emplace_back( toZpt(c->getEndPoint()) );
			}
		}
	}

	void ConnectionNode::prepareConnectionForDelete(Connection* g, const i8_t* fmt, ...)
	{
		if (!g) return;
		prepareConnectionForDelete(g);
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

	void ConnectionNode::prepareConnectionForDelete(Connection* g)
	{
		assert( g );
		if (!g) return;
		g->disconnect(false);
	}

	void ConnectionNode::sendRemoteConnected(const Connection* g)
	{
		if ( m_CoreNode->getRoutingMethod() != ERoutingMethod::ClientServer || !isServer() )
			return; 
		auto& etp = g->getEndPoint();
		i8_t buff[128];
		i32_t offs = etp.write( buff, 128 );
		if ( offs < 0 )
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION);
			return;
		}
		// to all except
		m_DispatchNode->send( (u8_t)EDataPacketType::RemoteConnected, buff, offs, &etp, true );
	}

	void ConnectionNode::sendRemoteDisconnected(const Connection* g, EDisconnectReason reason)
	{
		if ( m_CoreNode->getRoutingMethod() != ERoutingMethod::ClientServer || !isServer() )
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
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION);
			return;
		}
		// to all except
		m_DispatchNode->send( (u8_t)EDataPacketType::RemoteDisconnected, buff, offs+1, &etp, true );
	}

	bool ConnectionNode::recvPacket(const Packet& pack, class Connection* g)
	{
		assert(pack.type == EHeaderPacketType::Reliable_Ordered);
		if (!(pack.type == EHeaderPacketType::Reliable_Ordered))
		{
			Platform::log("WARNING: Unexpected packet type (%d) in %s", (i32_t)pack.type, __FUNCTION__);
			return false; // all connect node packets are reliable ordered
		}
		EDataPacketType packType = (EDataPacketType)pack.data[0];
		const i8_t* payload  = pack.data+1;		// first byte is PacketType
		i32_t payloadLen	 = pack.len-1;		// len includes the packetType byte
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
		case EDataPacketType::AlreadyConnected:
			recvAlreadyConnected(g, payload, payloadLen);
			break;
		case EDataPacketType::Rpc:
			recvRpcPacket(payload, payloadLen, g);
			break;
		default:
			// unandled packet
			return false; 
		}
		return true;
	}

	void ConnectionNode::recvConnectPacket(const i8_t* payload, i32_t payloadLen, class Connection* g)
	{
		// If already in list
		auto it = m_Connections.find(g->getEndPoint());
		if (it != m_Connections.end())
		{
			g->sendAlreadyConnected();
			return;
		}
		// Check password
		static const i32_t kBuffSize=1024;
		i8_t pw[kBuffSize];
		if ( !Util::readString( pw, kBuffSize, payload, payloadLen ))
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION);
			return; // invalid serialization
		}
		if ( strcmp( m_Password.c_str(), pw ) != 0 )
		{
			g->sendIncorrectPassword();
			return;
		}
		// Check if not exceeding max connections
		if ( (i32_t)m_Connections.size() >= m_MaxIncomingConnections )
		{
			g->sendMaxConnectionsReached();
			return;
		}
		// All fine..
		if (g->sendConnectAccept())
		{
			Util::forEachCallback(m_NewConnectionCallbacks, [&](auto& fcb)
			{
				(fcb)(g->getEndPoint());
			});
			sendRemoteConnected( g );
			Platform::log( "%s connected", g->getEndPoint().asString().c_str() );
		}
	}

	void ConnectionNode::recvConnectAccept(class Connection* g)
	{
		if (g->onReceiveConnectAccept()) // returns true if state was successfully handled
		{
			Util::forEachCallback(m_ConnectResultCallbacks, [&](auto& fcb)
			{
				(fcb)(g->getEndPoint(), EConnectResult::Succes);
			});
			Platform::log( "Connection accepted to %s", g->getEndPoint().asString().c_str() );
		}
		else
		{
			prepareConnectionForDelete( g );
		}
	}

	void ConnectionNode::recvDisconnectPacket(const i8_t* payload, i32_t len, class Connection* g)
	{
		if ( g->acceptDisconnect() ) // returns true if state was successfully handled
		{
			sendRemoteDisconnected( g, EDisconnectReason::Closed );
			Util::forEachCallback(m_DisconnectCallbacks, [&](auto& fcb)
			{
				(fcb)(true, g->getEndPoint(), EDisconnectReason::Closed);
			});
			prepareConnectionForDelete(g, "Disconnect received, removing connection %s", g->getEndPoint().asString().c_str());
		}
		else
		{
			prepareConnectionForDelete( g );
		}
	}

	void ConnectionNode::recvRemoteConnected(class Connection* g, const i8_t* data, i32_t len)
	{
		EndPoint etp;
		if (etp.read( data, len ) < 0)
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION);
			return;
		}
		Util::forEachCallback(m_NewConnectionCallbacks, [&](auto& fcb)
		{
			(fcb)(etp);
		});
		Platform::log( "Remote %s connected", etp.asString().c_str() );
	}

	void ConnectionNode::recvRemoteDisconnected(class Connection* g, const i8_t* data, i32_t len)
	{
		EndPoint etp;
		EDisconnectReason reason;
		i32_t offs = etp.read(data, len);
		if ( offs >= 0 )
		{
			assert(g->getEndPoint() != etp); // should not remote disconnect for a direct connection
			if (g->getEndPoint() == etp)
			{
				m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION);
				return;
			}
			reason = (EDisconnectReason)data[offs];
			Util::forEachCallback(m_DisconnectCallbacks, [&](auto& fcb)
			{
				assert ( etp != g->getEndPoint() && "received remote disc for ourselves..");
				(fcb)(false, etp, reason);
			});
			Platform::log( "Remote %s disconnected", etp.asString().c_str() );
			prepareConnectionForDelete( g );
		}
		else
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION);
		}
	}

	void ConnectionNode::recvInvalidPassword(class Connection* g, const i8_t* payload, i32_t payloadLen)
	{
		if ( g->setInvalidPassword() ) // returns true if state change ocurred
		{
			Util::forEachCallback(m_ConnectResultCallbacks, [&](auto& fcb)
			{
				(fcb)(g->getEndPoint(), EConnectResult::InvalidPassword);
			});
			Platform::log("Received invalid password for connection %s", g->getEndPoint().asString().c_str());
		}
		else
		{
			prepareConnectionForDelete( g );
		}
	}

	void ConnectionNode::recvMaxConnectionsReached(class Connection* g, const i8_t* payload, i32_t payloadLen)
	{
		if ( g->setMaxConnectionsReached() ) // returns true if state change occurred
		{
			Util::forEachCallback(m_ConnectResultCallbacks, [&](auto& fcb)
			{
				(fcb)(g->getEndPoint(), EConnectResult::MaxConnectionsReached);
			});
			Platform::log("Received max connections reached for connection %s", g->getEndPoint().asString().c_str());
		}
		else
		{
			prepareConnectionForDelete( g );
		}
	}

	void ConnectionNode::recvAlreadyConnected(class Connection* g, const i8_t* payload, i32_t payloadLen)
	{
		// No state change on the connection in this case, could already be successfully connected before.
		Util::forEachCallback(m_ConnectResultCallbacks, [&](auto& fcb)
		{
			(fcb)(g->getEndPoint(), EConnectResult::AlreadyConnected);
		});
		// Consider this a warning
		Platform::log("WARNING: Received already connected for %s", g->getEndPoint().asString().c_str());
	}

	void ConnectionNode::recvRpcPacket(const i8_t* payload, i32_t len, class Connection* g)
	{
		i8_t name[RPC_NAME_MAX_LENGTH];
		if ( !Util::readFixed( name, RPC_NAME_MAX_LENGTH, payload, (RPC_NAME_MAX_LENGTH<len?RPC_NAME_MAX_LENGTH:len)) )
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION);
			return;
		}
		i8_t fname[RPC_NAME_MAX_LENGTH*2];
		auto* ptrNext = Util::appendString(fname, RPC_NAME_MAX_LENGTH*2, "__rpc_deserialize_");
		Util::appendString(ptrNext, RPC_NAME_MAX_LENGTH, name);
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
			m_CoreNode->setCriticalError(ECriticalError::CannotFindExternalCFunction, ZERODELAY_FUNCTION);
			Platform::log("CRITICAL: Cannot find external C function %s", fname);
		}
	}

	//void ConnectionNode::recvUserPacket(class Connection* g, const Packet& pack)
	//{
	//	if ( pack.relay && m_RoutingMethod == ERoutingMethod::ClientServer && isServer() ) // send through to others
	//	{
	//		// except self
	//		send( pack.data[0], pack.data+1, pack.len-1, &g->getEndPoint(), true, pack.type, pack.channel, false /* relay only once */ );
	//	}
	//	forEachCallback(m_CustomDataCallbacks, [&](auto& fcb)
	//	{
	//		(fcb)(g->getEndPoint(), pack.data[0], pack.data+1, pack.len-1, pack.channel);
	//	});
	//}

	void ConnectionNode::updateConnecting(class Connection* g)
	{
		if ( g->updateConnecting() ) // Returns true if there is a state change! Else, already connected or timed out
		{
			if ( g->getState() == EConnectionState::InitiateTimedOut )
			{
				Util::forEachCallback( m_ConnectResultCallbacks, [g] (auto& fcb)
				{
					(fcb)( g->getEndPoint(), EConnectResult::Timedout );
				});
				// By calling this, the state will be set to 'Disconnected', so callbacks are not called multiple times.
				prepareConnectionForDelete( g, "Removing connection %s, timed out", g->getEndPoint().asString().c_str() );
			}
		}
	}

	void ConnectionNode::updateKeepAlive(class Connection* g)
	{
		if ( g->updateKeepAlive() ) // Returns true if there is a state change! Else arleady timed out, just not timed out, or no keep alive is managed
		{
			if ( g->getState() == EConnectionState::ConnectionTimedOut )
			{
				sendRemoteDisconnected( g, EDisconnectReason::Lost );
				Util::forEachCallback( m_DisconnectCallbacks, [g] (auto& fcb)
				{
					(fcb)( true, g->getEndPoint(), EDisconnectReason::Lost );
				});
				// By calling this, state is set to disconnected, so callbacks are called multiple times.
				prepareConnectionForDelete( g, "Removing connection %s, was lost", g->getEndPoint().asString().c_str() );
			}
		}
	}

	void ConnectionNode::updateDisconnecting(class Connection* g)
	{
		if ( g->updateDisconnecting() ) // Else, state already handled or not in disconnecting state
		{
			if ( g->getState() == EConnectionState::Disconnected ) // if linger state is passed, state is set to disconnected
			{
				Util::forEachCallback( m_DisconnectCallbacks, [g] (auto& fcb)
				{
					(fcb)( true, g->getEndPoint(), EDisconnectReason::Closed );
				});
				// By calling this, state is set to disconnected, so callbacks are called multiple times.
				prepareConnectionForDelete( g, "Removing connection %s, disonnected gracefully", g->getEndPoint().asString().c_str() );
			}
		}
	}
}
