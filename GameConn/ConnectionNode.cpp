#include "ConnectionNode.h"
#include "Connection.h"
#include "EndPoint.h"
#include "RpcMacros.h"
#include "CoreNode.h"
#include "RecvNode.h"
#include "RUDPLink.h"
#include "Util.h"


namespace Zerodelay
{
	extern ZEndpoint toZpt( const EndPoint& r );
	extern EndPoint toEtp( const ZEndpoint& z );


	ConnectionNode::ConnectionNode(i32_t keepAliveIntervalSeconds):
		m_DispatchNode(nullptr),
		m_ProcessingConnection(nullptr),
		m_RelayConnectAndDisconnect(false),
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
		RUDPLink* link = m_DispatchNode->getOrAddLink( endPoint, false );
		if (!link) // is nullptr if is pending delete
		{
			return EConnectCallResult::AlreadyExists;
		}
		m_DispatchNode->startThreads(); // start after socket is opened
		Connection* g = new Connection( this, true, link, timeoutSeconds, m_KeepAliveIntervalSeconds );
		m_Connections.insert( std::make_pair( endPoint, g ) );
		g->sendConnectRequest( pw );
		return EConnectCallResult::Succes;
	}

	EListenCallResult ConnectionNode::listenOn(i32_t port, const std::string& pw)
	{
		if ( !m_DispatchNode->openSocketOnPort(port) )
		{
			return EListenCallResult::SocketError;
		}
		m_DispatchNode->startThreads(); // start after socket is opened
		setPassword( pw );
		return EListenCallResult::Succes;
	}

	EDisconnectCallResult ConnectionNode::disconnect(const EndPoint& endPoint)
	{
		// check if exists
		auto& it = m_Connections.find( endPoint );
		if ( it != m_Connections.end() )
		{
			Connection* conn = it->second;
			if ( conn->disconnect() ) // returns false if connection is not in 'connected' state
			{
				return EDisconnectCallResult::Succes;
			}
			return EDisconnectCallResult::NotConnected;
		}
		return EDisconnectCallResult::UnknownEndpoint;
	}

	void ConnectionNode::disconnectAll()
	{
		for ( auto it : m_Connections )
		{
			assert( it.second );
			if ( it.second ) 
			{
				it.second->disconnect();
			}
		}
	}

	i32_t ConnectionNode::getNumOpenConnections() const
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

	bool ConnectionNode::isInConnectionList(const ZEndpoint& ztp) const
	{
		EndPoint etp = toEtp(ztp);
		return m_Connections.count(etp) != 0;
	}

	void ConnectionNode::update()
	{
		for ( auto it = m_Connections.begin(); it != m_Connections.end(); )
		{
			Connection* c = it->second;
			if (!c)
			{
				it = m_Connections.erase(it);
				continue;
			}
			else
			{
				// if connection is not one of the following states, remove it
				auto s = c->getState();
				assert(s != EConnectionState::Idle); // invalid state, should never be the case
				if ( !(s == EConnectionState::Connecting || s == EConnectionState::Connected || s == EConnectionState::Disconnecting) )
				{
					c->disconnect();
					delete c;
					it = m_Connections.erase(it);
					continue;
				}
			}
			updateConnecting( c );
			updateKeepAlive( c );	
			updateDisconnecting( c );
			it++;
		}
	}

	void ConnectionNode::beginProcessPacketsFor(const EndPoint& endPoint)
	{
		assert(!m_ProcessingConnection);
		auto it = m_Connections.find(endPoint);
		if ( it != m_Connections.end() )
		{
			m_ProcessingConnection = it->second;
		}
	}

	bool ConnectionNode::processPacket(const Packet& pack, RUDPLink& link)
	{
		// all connection node packets are reliable ordered
		if ( pack.type == EHeaderPacketType::Reliable_Ordered )  
		{
			// returns false if packet was not consumed (handled)
			return recvPacket( pack, m_ProcessingConnection, link );
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
		if ( maxNumConnections < 1 )
		{
			Platform::log( "WARNING: maxNumConnections less than 1, call ignored. ");
			return;
		}
		m_MaxIncomingConnections = maxNumConnections;
	}

	void ConnectionNode::setRelayConnectAndDisconnectEvents(bool relay)
	{
		m_RelayConnectAndDisconnect = relay;
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

	void ConnectionNode::sendRemoteConnected(const Connection* g)
	{
		if ( !getRelayConnectAndDisconnect() )
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
		if ( !getRelayConnectAndDisconnect() )
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

	void ConnectionNode::sendSystemMessage(RUDPLink& link, EDataPacketType state, const i8_t* payload, i32_t len)
	{
		link.addToSendQueue( (u8_t)state, payload, len, EHeaderPacketType::Reliable_Ordered );
	}

	bool ConnectionNode::recvPacket(const Packet& pack, class Connection* g, RUDPLink& link)
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
		// if not a connection yet, only interested in connect attempt packets
		if (!g)
		{
			if (packType == EDataPacketType::ConnectRequest) 
			{
				recvConnectPacket(payload, payloadLen, link);
				return true; // packet handled
			}
		}
		else
		{
			switch (packType)
			{
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
				return false;
			}
			return true;
		}
		return false;
	}

	void ConnectionNode::handleInvalidConnectAttempt(EDataPacketType responseType, RUDPLink& link)
	{
		sendSystemMessage( link, responseType );
		link.markPendingDelete();
		link.blockAllUpcomingSends();
	}

	void ConnectionNode::recvConnectPacket(const i8_t* payload, i32_t payloadLen, RUDPLink& link)
	{
		// If already in list
		auto it = m_Connections.find(link.getEndPoint());
		if (it != m_Connections.end())
		{
			handleInvalidConnectAttempt( EDataPacketType::AlreadyConnected, link );
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
			handleInvalidConnectAttempt( EDataPacketType::IncorrectPassword, link );
			return;
		}
		// Check if not exceeding max connections
		if ( (i32_t)m_Connections.size() >= m_MaxIncomingConnections )
		{
			handleInvalidConnectAttempt( EDataPacketType::MaxConnectionsReached, link );
			return;
		}
		// All fine..
		Connection* g = new Connection( this, false, &link );
		g->sendConnectAccept();
		m_Connections.insert( std::make_pair(link.getEndPoint(), g) );
		Util::forEachCallback(m_NewConnectionCallbacks, [&](auto& fcb)
		{
			(fcb)(link.getEndPoint());
		});
		sendRemoteConnected( g );
		Platform::log( "%s connected", g->getEndPoint().asString().c_str() );
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
			Platform::log( "Disconnect received, removing connection %s", g->getEndPoint().asString().c_str() );
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
		assert(etp != g->getEndPoint());
		if (g->getEndPoint() == etp)
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
		if ( offs < 0 )
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION);
			return;
		}
		assert(g->getEndPoint() != etp); // should not remote disconnect for a direct connection
		if (g->getEndPoint() == etp)
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION);
			return;
		}
		reason = (EDisconnectReason)data[offs];
		Util::forEachCallback(m_DisconnectCallbacks, [&](auto& fcb)
		{
			(fcb)(false, etp, reason);
		});
		Platform::log( "Remote %s disconnected", etp.asString().c_str() );
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

	void ConnectionNode::updateConnecting(class Connection* g)
	{
		// updateConnecting only returns true if was Connecting and state changed
		if ( g->updateConnecting() && g->getState() == EConnectionState::InitiateTimedOut )
		{
			Util::forEachCallback( m_ConnectResultCallbacks, [g] (auto& fcb)
			{
				(fcb)( g->getEndPoint(), EConnectResult::Timedout );
			});
			Platform::log( "Removing connection %s, timed out", g->getEndPoint().asString().c_str() );
		}
	}

	void ConnectionNode::updateKeepAlive(class Connection* g)
	{
		// updateKeepAlive only returns true if was Connected and state change
		if ( g->updateKeepAlive() && g->getState() == EConnectionState::ConnectionTimedOut )
		{
			sendRemoteDisconnected( g, EDisconnectReason::Lost );
			Util::forEachCallback( m_DisconnectCallbacks, [g] (auto& fcb)
			{
				(fcb)( true, g->getEndPoint(), EDisconnectReason::Lost );
			});
			Platform::log( "Removing connection %s as it was timed out", g->getEndPoint().asString().c_str() );
		}
	}

	void ConnectionNode::updateDisconnecting(class Connection* g)
	{
		// updateDisconnecting only returns true if was disconnecting and a state change occurred
		if ( g->updateDisconnecting() && g->getState() == EConnectionState::Disconnected )
		{
			Util::forEachCallback( m_DisconnectCallbacks, [g] (auto& fcb)
			{
				(fcb)( true, g->getEndPoint(), EDisconnectReason::Closed );
			});
			Platform::log( "Disconnected connection %s gracefully", g->getEndPoint().asString().c_str() );
		}
	}

}
