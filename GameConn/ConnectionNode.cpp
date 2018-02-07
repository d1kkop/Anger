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
		disconnect();
	}

	// Synonym for 'reset' in the ConnectionNode
	void ConnectionNode::disconnect()
	{
		Platform::log("Disconnect called, num connections %d.", (i32_t)m_Connections.size());
		// destruct memory
		for ( auto& kvp : m_Connections )
		{
			Connection* c = kvp.second;
			c->disconnect(true, c->getEndPoint(), EDisconnectReason::Closed, EConnectionState::Disconnected, true);
			delete c;
		}
		// reset state
		m_Connections.clear();
		m_ProcessingConnection = nullptr;
		//
		//  !! leave user specified settings such as keep alive interval and max connections and callbacks in tact !!
		//
		//bool m_RelayConnectAndDisconnect;
		//i32_t m_KeepAliveIntervalSeconds;
		//i32_t m_MaxIncomingConnections;
		//std::string m_Password;
		//class Connection* m_ProcessingConnection;
		//std::map<EndPoint, class Connection*, EndPoint::STLCompare> m_Connections;
		//std::vector<ConnectResultCallback>	m_ConnectResultCallbacks;
		//std::vector<DisconnectCallback>		m_DisconnectCallbacks;
		//std::vector<NewConnectionCallback>	m_NewConnectionCallbacks;
	}

	void ConnectionNode::postInitialize(CoreNode* coreNode)
	{
		assert(!m_CoreNode && !m_DispatchNode);
		m_CoreNode = coreNode;
		m_DispatchNode = coreNode->rn();
	}

	EConnectCallResult ConnectionNode::connect(const std::string& name, i32_t port, const std::string& pw, 
											   i32_t timeoutSeconds, bool sendRequest,
											   const std::map<std::string, std::string>& additionalData)
	{
		EndPoint endPoint;
		if ( !endPoint.resolve( name, port ) )
		{
			return EConnectCallResult::CannotResolveHost;
		}
		return connect( endPoint, pw, timeoutSeconds, sendRequest, additionalData );
	}

	EConnectCallResult ConnectionNode::connect(const EndPoint& endPoint, const std::string& pw, 
											   i32_t timeoutSeconds, bool sendRequest,
											   const std::map<std::string, std::string>& additionalData)
	{
		// For p2p, the socket is already listening on a specific port, the function will return true
		if ( !m_DispatchNode->openSocketOnPort(0) )
		{
			return EConnectCallResult::SocketError; 
		}
		if ( m_Connections.find( endPoint ) != m_Connections.end() )
		{
			return EConnectCallResult::AlreadyExists;
		}
		// add link fails if a previous already exists
		RUDPLink* link = m_DispatchNode->addLink( endPoint, nullptr );
		if (!link)
		{
			return EConnectCallResult::AlreadyExists;
		}
		Connection* g = new Connection( this, true, link, timeoutSeconds, m_KeepAliveIntervalSeconds );
		m_Connections.insert( std::make_pair( endPoint, g ) );
		m_DispatchNode->startThreads(); // start after socket is opened
		if ( sendRequest )
		{
			if (!g->sendConnectRequest( pw, additionalData ))
			{
				return EConnectCallResult::TooMuchAdditionalData;
			}
		}
		return EConnectCallResult::Succes;
	}

	EListenCallResult ConnectionNode::listenOn(i32_t port)
	{
		if ( !m_DispatchNode->openSocketOnPort(port) )
		{
			return EListenCallResult::SocketError;
		}
		m_DispatchNode->startThreads(); // start after socket is opened
		return EListenCallResult::Succes;
	}

	EDisconnectCallResult ConnectionNode::disconnect(const EndPoint& endPoint, EDisconnectReason reason, EConnectionState newState, bool sendMsg, bool deleteAndRemove)
	{
		// check if exists
		auto& it = m_Connections.find( endPoint );
		if ( it != m_Connections.end() )
		{
			Connection* conn = it->second;
			EDisconnectCallResult disconResult = (conn->isConnected() ? EDisconnectCallResult::Succes : EDisconnectCallResult::NotConnected);
			conn->disconnect(true, endPoint, reason, newState, sendMsg);
			if (deleteAndRemove)
			{
				delete conn;
				m_Connections.erase(it);
			}
			return disconResult;
		}
		return EDisconnectCallResult::UnknownEndpoint;
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
		EndPoint etp = Util::toEtp(ztp);
		return m_Connections.count(etp) != 0;
	}

	Zerodelay::Connection* ConnectionNode::getConnection(const ZEndpoint& ztp) const
	{
		EndPoint etp = Util::toEtp(ztp);
		auto it = m_Connections.find(etp);
		if (it != m_Connections.end()) return it->second;
		return nullptr;
	}

	void ConnectionNode::update()
	{
		if (m_CoreNode->hasCriticalErrors()) return;

		for ( auto it = m_Connections.begin(); it != m_Connections.end(); )
		{
			Connection* c = it->second;

			// Instead of trying to remember on which event we have to remove the connection. Only a state is changed.
			// Every update cycle, check if the state of the connection is valid to continue, otherwise remove it.
			EConnectionState state = c->getState();
			if ( !(state == EConnectionState::Connected || state == EConnectionState::Connecting) )
			{
				Platform::log("Deleted and removed connection to %s. Num remaining connections before delete %d.", 
							  c->getEndPoint().toIpAndPort().c_str(), (i32_t)m_Connections.size());
				delete c;
				it = m_Connections.erase( it );
				continue;
			}

			updateConnecting( c );
			updateKeepAlive( c );	
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

	void ConnectionNode::setMaxIncomingConnections(u32_t maxNumConnections)
	{
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
			if ( c->isConnected() )
			{
				endpoints.emplace_back( Util::toZpt(c->getEndPoint()) );
			}
		}
	}

	void ConnectionNode::forConnections(const EndPoint* specific, bool exclude, const std::function<void(Connection&)>& cb)
	{
		assert(cb);
		if(!cb)
		{
			m_CoreNode->setCriticalError(ECriticalError::InvalidLogic, ZERODELAY_FUNCTION_LINE); 
			return;
		}
		if ( specific )
		{
			if ( exclude )
			{
				for ( auto& kvp : m_Connections )
				{
					Connection* c = kvp.second;
					if (c->getEndPoint() == *specific) continue; // skip this one
					cb(*c);
				}
			}
			else
			{
				auto it = m_Connections.find( *specific );
				if ( it != m_Connections.end() )
				{
					cb( *it->second );
				}
				else
				{
					Platform::log("WARNING: Trying to send to specific endpoint %s which is not in the list of connections.", specific->toIpAndPort().c_str());
				}
			}
		}
		else
		{
			for ( auto& kvp : m_Connections )
			{
				Connection* c = kvp.second;
				cb(*c);
			}
		}
	}

	void ConnectionNode::doConnectResultCallbacks(const EndPoint& remote, EConnectResult result)
	{
		ZEndpoint ztp = Util::toZpt(remote);
		Util::forEachCallback(m_ConnectResultCallbacks,[&](const ConnectResultCallback& crb)
		{
			(crb)(ztp, result);
		});
	}

	void ConnectionNode::doDisconnectCallbacks(bool directLink, const EndPoint& remote,EDisconnectReason reason)
	{
		ZEndpoint ztp = Util::toZpt(remote);
		Util::forEachCallback(m_DisconnectCallbacks,[&](const DisconnectCallback& dcb)
		{
			(dcb)(directLink, ztp, reason);
		});
	}

	void ConnectionNode::doNewIncomingConnectionCallbacks(bool directLink, const EndPoint& remote, const std::map<std::string, std::string>& additionalData)
	{
		ZEndpoint ztp = Util::toZpt(remote);
		Util::forEachCallback(m_NewConnectionCallbacks,[&](const NewConnectionCallback& ncb)
		{
			(ncb)(directLink, ztp, additionalData);
		});
	}

	void ConnectionNode::sendRemoteConnected(const Connection* g, const std::map<std::string, std::string>& additionalData)
	{
		if ( !shouldRelayConnectAndDisconnect() ) 
		{
			assert(false);
			return; 
		}
		auto& etp = g->getEndPoint();
		i32_t dstSize = ZERODELAY_BUFF_SIZE;
		i8_t buff[ZERODELAY_BUFF_SIZE];
		i32_t offs = etp.write( buff, dstSize );
		if ( offs < 0 )
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
			return;
		}
		dstSize -= offs;
		i8_t* ptr = buff + offs;
		for (auto& kvp : additionalData)
		{
			bool bSucces;
			// key
			ptr = Util::appendString2( ptr, dstSize, kvp.first.c_str(), bSucces );
			if (!bSucces)
			{
				m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
				return;
			}
			// value
			ptr = Util::appendString2( ptr, dstSize, kvp.second.c_str(), bSucces );
			if (!bSucces)
			{
				m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
				return;
			}
		}
		// to all except
		forConnections(&g->getEndPoint(), true, [&](Connection& c)
		{
			if (!c.isConnected()) return;
			ESendCallResult sendResult;
			c.getLink()->addToSendQueue( sendResult, (u8_t)EDataPacketType::RemoteConnected, buff, ZERODELAY_BUFF_SIZE-dstSize, 
										 EHeaderPacketType::Reliable_Ordered, 0, false );
		});
	}

	void ConnectionNode::sendRemoteDisconnected(const Connection* g, EDisconnectReason reason)
	{
		if ( !shouldRelayConnectAndDisconnect() )
		{
			assert(false);
			return; 
		}
		auto& etp = g->getEndPoint();
		i8_t buff[128];
		i32_t offs = etp.write( buff, 128 );
		if ( offs >= 0 )
		{
			buff[offs] = (u8_t)reason;
		}
		else
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
			return;
		}
		// to all except
		forConnections(&g->getEndPoint(), true, [&](Connection& c)
		{
			if (!c.isConnected()) return; // skip 
			// one added byte because of disconnect reason (8 bits)
			ESendCallResult sendResult;
			c.getLink()->addToSendQueue( sendResult, (u8_t)EDataPacketType::RemoteDisconnected, buff, offs+1, EHeaderPacketType::Reliable_Ordered, 0, false );
		});
	}

	void ConnectionNode::sendSystemMessage(RUDPLink& link, EDataPacketType state, const i8_t* payload, i32_t len)
	{
		ESendCallResult sendResult;
		link.addToSendQueue( sendResult, (u8_t)state, payload, len, EHeaderPacketType::Reliable_Ordered );
	}

	bool ConnectionNode::recvPacket(const Packet& pack, class Connection* g, RUDPLink& link)
	{
		assert(pack.type == EHeaderPacketType::Reliable_Ordered);
		if (!(pack.type == EHeaderPacketType::Reliable_Ordered))
		{
			Platform::log("WARNING: Unexpected packet type (%d) in %s.", (i32_t)pack.type, ZERODELAY_FUNCTION_LINE);
			return false; // all connect node packets are reliable ordered
		}
		// this assumes that first byte of payload is higher level data id followed by variable data length (payload), assert this
		static_assert(RUDPLink::off_Norm_Data - RUDPLink::off_Norm_Id == 1, "Invalid offset");
		EDataPacketType packType = (EDataPacketType)pack.data[0];
		const i8_t* payload  = pack.data+1;		// first byte is PacketType
		i32_t payloadLen	 = pack.len-1;		// len includes the packetType (first byte), therefore subtract 1 to obtain payload
		// if not a connection yet, only interested in connect attempt packets
		if (!g)
		{
			if (packType == EDataPacketType::ConnectRequest) 
			{
				// pack.linkId is not link.linkId here, it is the connectorId
				recvConnectPacket(payload, payloadLen, link);
			}
			else
			{
				// for now, only allow other packets when having a connectin, so if not, remove the link
				handleInvalidConnectAttempt(EDataPacketType::InvalidConnectPacket, link);
			}
			return true;
		}
		else
		{
			switch (packType)
			{
			case EDataPacketType::ConnectRequest: // in p2p, there is already a connection for a request
				recvConnectPacket(payload, payloadLen, link);
				break;
			case EDataPacketType::ConnectAccept:
				g->onReceiveConnectAccept();
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
				g->setInvalidPassword();
				break;
			case EDataPacketType::MaxConnectionsReached:
				g->setMaxConnectionsReached();
				break;
			case EDataPacketType::AlreadyConnected:
				g->setInvalidConnectPacket();
				break;
			case EDataPacketType::InvalidConnectPacket:
				break;
			default:
				return false; // unhandled;
			}
			return true; // handeld
		}
		return false; // unhandeld
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
			handleInvalidConnectAttempt(EDataPacketType::AlreadyConnected, link );
			return;
		}
		// Check password
		const i32_t kBuffSize=ZERODELAY_BUFF_RECV_SIZE;
		i8_t pw[kBuffSize];
		i32_t readBytes = Util::readString( pw, kBuffSize, payload, payloadLen );
		if ( readBytes < 0 )
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
			return; // invalid serialization
		}
		if ( strcmp( m_Password.c_str(), pw ) != 0 )
		{
			handleInvalidConnectAttempt(EDataPacketType::IncorrectPassword, link );
			return;
		}
		// Check if not exceeding max connections
		if ( (i32_t)m_Connections.size() >= m_MaxIncomingConnections )
		{
			handleInvalidConnectAttempt(EDataPacketType::MaxConnectionsReached, link);
			return;
		}
		// read additional data
		std::map<std::string, std::string> additionalData;
		bool succes = Util::deserializeMap(additionalData, payload + readBytes, payloadLen - readBytes);
		if (!succes)
		{
			m_CoreNode->setCriticalError( ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE );
			return;
		}
		// All fine..
		Connection* g = new Connection( this, false, &link );
		g->sendConnectAccept();
		m_Connections.insert( std::make_pair(link.getEndPoint(), g) );
		doNewIncomingConnectionCallbacks(true, link.getEndPoint(), additionalData);
		if (m_RelayConnectAndDisconnect) sendRemoteConnected( g, additionalData );
		Platform::log( "New incoming connection to %s (id %d).", g->getEndPoint().toIpAndPort().c_str(), link.id() );
	}

	void ConnectionNode::recvDisconnectPacket(const i8_t* payload, i32_t len, class Connection* g)
	{
		bool bWasConnected = (g->getState() == EConnectionState::Connected);
		g->onReceiveDisconnect();
		if (bWasConnected && m_RelayConnectAndDisconnect)
		{
			sendRemoteDisconnected( g, EDisconnectReason::Closed );
		}
	}

	void ConnectionNode::recvRemoteConnected(class Connection* g, const i8_t* data, i32_t len)
	{
		EndPoint etp;
		i32_t offs = etp.read( data, len );
		if ( offs < 0 )
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
			return;
		}
		assert(etp != g->getEndPoint()); // should never get remote endpoint msg from direct link
		if (g->getEndPoint() == etp)
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
			return;
		}
		std::map<std::string, std::string> additionalData;
		bool succes = Util::deserializeMap(additionalData, data+offs, len-offs);
		if (!succes)
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
			return;
		}
		doNewIncomingConnectionCallbacks(false, etp, additionalData);
		Platform::log( "Remote %s connected.", etp.toIpAndPort().c_str() );
	}

	void ConnectionNode::recvRemoteDisconnected(class Connection* g, const i8_t* data, i32_t len)
	{
		EndPoint etp;
		EDisconnectReason reason;
		i32_t offs = etp.read(data, len);
		if ( offs < 0 )
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
			return;
		}
		assert(g->getEndPoint() != etp); // should not remote disconnect for a direct link
		if (g->getEndPoint() == etp)
		{
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
			return;
		}
		reason = (EDisconnectReason)data[offs];
		doDisconnectCallbacks(false, etp, reason);
		Platform::log( "Remote %s disconnected.", etp.toIpAndPort().c_str() );
	}

	void ConnectionNode::recvAlreadyConnected(class Connection* g, const i8_t* payload, i32_t payloadLen)
	{
		// No state change on the connection in this case, could already be successfully connected before.
		ZEndpoint ztp = Util::toZpt(g->getEndPoint());
		Util::forEachCallback(m_ConnectResultCallbacks, [&](const ConnectResultCallback& crc)
		{
			(crc)(ztp, EConnectResult::AlreadyConnected);
		});
	}

	void ConnectionNode::updateConnecting(class Connection* g)
	{
		g->updateConnecting();
	}

	void ConnectionNode::updateKeepAlive(class Connection* g)
	{
		bool bWasConnected = g->isConnected();
		g->updateKeepAlive();
		if (bWasConnected && !g->isConnected() && m_RelayConnectAndDisconnect) 
		{
			sendRemoteDisconnected( g, EDisconnectReason::Lost );
		}
	}
}
