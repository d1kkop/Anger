#include "Zerodelay.h"
#include "RUDPLink.h"
#include "Socket.h"
#include "CoreNode.h"
#include "RecvNode.h"
#include "Connection.h"
#include "ConnectionNode.h"
#include "VariableGroupNode.h"
#include "MasterServer.h"


namespace Zerodelay
{

	// -------- Support --------------------------------------------------------------------------------------------------

	void storeEtp( const EndPoint& r, ZEndpoint& z )
	{
		static_assert( sizeof(EndPoint) <= sizeof(ZEndpoint), "Zendpoint size is too small" );
		memcpy( &z, &r, sizeof(EndPoint) ); 
	}

	EndPoint* asEpt( ZEndpoint* z )
	{
		return (EndPoint*)z;
	}

	const EndPoint* asEpt( const ZEndpoint* z )
	{
		return (const EndPoint*)z;
	}


	// -------- End Support ----------------------------------------------------------------------------------------------


	// -------- ZEndpoint ----------------------------------------------------------------------------------------------

	ZEndpoint::ZEndpoint()
	{
		::memset(this, 0, sizeof(ZEndpoint));
	}

	ZEndpoint::ZEndpoint(const std::string& name, u16_t port)
	{
		::memset(this, 0, sizeof(ZEndpoint));
		resolve(name, port);
	}

	bool ZEndpoint::operator==(const ZEndpoint& other) const
	{
		return ::memcmp( this, &other, sizeof(ZEndpoint) ) == 0;
	}

	bool ZEndpoint::operator()(const ZEndpoint& left, const ZEndpoint& right) const
	{
		return memcmp( &left, &right, sizeof(ZEndpoint) ) < 0;
	}

	i32_t ZEndpoint::write(i8_t* buff, i32_t bufSize) const
	{
		EndPoint* e = (EndPoint*)(this);
		return e->write(buff, bufSize);
	}

	i32_t ZEndpoint::read(const i8_t* buff, i32_t bufSize)
	{
		EndPoint* e = (EndPoint*)(this);
		return e->read(buff, bufSize);
	}

	bool ZEndpoint::resolve(const std::string& name, u16_t port)
	{
		EndPoint etp;
		if ( etp.resolve( name, port ) )
		{
			storeEtp( etp, *this );
			return true;
		}
		return false;
	}

	std::string ZEndpoint::toIpAndPort() const
	{
		return asEpt(this)->toIpAndPort();
	}

	i32_t ZEndpoint::getLastError() const
	{
		return asEpt(this)->getLastError();
	}

	bool ZEndpoint::isValid() const
	{
		i8_t* c = (i8_t*)this;
		for ( auto i=0; i<sizeof(ZEndpoint); ++i ) if (c[i] != 0) return false;
		return true;
	}

	// -------- ZNode ----------------------------------------------------------------------------------------------


	ZNode::ZNode(u32_t reliableNewestUpdateIntervalMs, u32_t ackAggregateTimeMs, u32_t keepAliveIntervalSeconds) :
		C(new CoreNode
		(
			this,
			(new RecvNode(reliableNewestUpdateIntervalMs, ackAggregateTimeMs)),
			(new ConnectionNode(keepAliveIntervalSeconds)),
			(new VariableGroupNode()),
			(new MasterServer())
		))
	{
		C->vgn()->setupConnectionCallbacks();
	}

	ZNode::~ZNode()
	{
		delete C;
	}

	EConnectCallResult ZNode::connect(const ZEndpoint& endPoint, const std::string& pw, u32_t timeoutSeconds, const std::map<std::string, std::string>& additionalData, bool sendRequest)
	{
		return C->cn()->connect( Util::toEtp(endPoint), pw, timeoutSeconds, sendRequest, additionalData );
	}

	EConnectCallResult ZNode::connect(const std::string& name, u16_t port, const std::string& pw, u32_t timeoutSeconds, const std::map<std::string, std::string>& additionalData, bool sendRequest)
	{
		return C->cn()->connect( name, port, pw, timeoutSeconds, sendRequest, additionalData );
	}

	void ZNode::disconnect(u32_t lingerTimeMs)
	{
		C->cn()->disconnect(); // synonym for reset in ConnectionNode
		if (lingerTimeMs > 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(lingerTimeMs));
		}
		C->rn()->reset();
		C->vgn()->reset(false);
	}

	EDisconnectCallResult ZNode::disconnect(const ZEndpoint& endPoint)
	{
		return C->cn()->disconnect( Util::toEtp(endPoint), EDisconnectReason::Closed, EConnectionState::Disconnected, true, true );
	}

	EListenCallResult ZNode::listen(i32_t port, const std::string& pw, i32_t maxConnections)
	{
		if (C->isListening()) return EListenCallResult::AlreadyStartedServer;
		// set session inital states
		C->cn()->setMaxIncomingConnections( maxConnections );
		C->cn()->setRelayConnectAndDisconnectEvents( true );
		C->cn()->setPassword( pw );
		C->vgn()->setIsNetworkIdProvider( true );
		EListenCallResult res = C->cn()->listenOn( port );
		if (res == EListenCallResult::Succes)
		{
			C->setIsListening(true);
		}
		return res;
	}

	i32_t ZNode::getNumOpenConnections() const
	{
		return C->cn()->getNumOpenConnections();
	}

	i32_t ZNode::getNumOpenLinks() const
	{
		return C->rn()->getNumOpenLinks();
	}

	bool ZNode::isConnectedTo(const ZEndpoint& ztp) const
	{
		Connection* c = C->cn()->getConnection(ztp);
		return c && c->isConnected();
	}

	bool ZNode::isConnectionKnown(const ZEndpoint& ztp) const
	{
		auto res = C->rn()->getLink(Util::toEtp(ztp), true) != nullptr;
		//bool res = (C->cn()->isInConnectionList(ztp) || C->rn()->getLink(Util::toEtp(ztp), true) != nullptr);
		return res;
	}

	bool ZNode::hasPendingData() const
	{
		u32_t linkIdx = 0;
		// When pinned, the link will not be destroyed from memory
		RUDPLink* link = C->rn()->getLinkAndPinIt(linkIdx);
		while (link)
		{
			bool bQueuesEmpty = link->areAllQueuesEmpty();
			C->rn()->unpinLink(link);
			if ( !bQueuesEmpty ) return true;
			link = C->rn()->getLinkAndPinIt(++linkIdx);
		}
		return false;
	}

	void ZNode::update()
	{
		if (C->hasCriticalErrors())
			return;

		u32_t linkIdx = 0;
		// When pinned, the link will not be destroyed from memory
		RUDPLink* link = C->rn()->getLinkAndPinIt(linkIdx);
		while (link)
		{
			C->cn()->beginProcessPacketsFor(link->getEndPoint());
			link->beginPoll();
			Packet pack;
			while (link->poll(pack))
			{
				// try at all nodes, returns false if packet is not processed
				if (!C->cn()->processPacket(pack, *link))
				{
					if (!C->vgn()->processPacket(pack, link->getEndPoint()))
					{
						C->processUnhandledPacket(pack, link->getEndPoint());
					}
				}
				delete [] pack.data;
			}
			link->endPoll();
			C->cn()->endProcessPackets();
			C->rn()->unpinLink(link);
			link = C->rn()->getLinkAndPinIt(++linkIdx);
		}

		C->cn()->update();
		C->vgn()->update();
	}

	void ZNode::setPassword(const std::string& pw)
	{
		C->cn()->setPassword( pw );
	}

	void ZNode::setMaxIncomingConnections(u32_t maxNumConnections)
	{
		C->cn()->setMaxIncomingConnections( maxNumConnections );
	}

	void ZNode::getConnectionListCopy(std::vector<ZEndpoint>& listOut)
	{
		C->cn()->getConnectionListCopy(listOut);
	}

	bool ZNode::isAuthorative() const
	{
		return (!C->isP2P() && C->isListening()) || (C->isP2P() && C->isSuperPeer());
	}

	bool ZNode::isClient() const
	{
		return !(C->isP2P() || C->isListening());
	}

	ZEndpoint ZNode::getFirstEndpoint() const
	{
		return C->cn()->getFirstEndpoint();
	}

	void ZNode::simulatePacketLoss(u32_t percentage)
	{
		C->rn()->simulatePacketLoss( percentage );
	}

	ESendCallResult ZNode::sendReliableOrdered(u8_t id, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude, u8_t channel, 
											   bool relay, bool requiresConnection, std::vector<ZAckTicket>* deliveryTraceOut)
	{
		ESendCallResult sendResult = ESendCallResult::NotSent;
		if (requiresConnection)
		{
			C->cn()->forConnections(asEpt(specific), exclude, [&](Connection& c)
			{
				if (!c.isConnected())
				{
					Util::addTraceCallResult( deliveryTraceOut, c.getEndPoint(), ETraceCallResult::ConnectionWasRequired, 0, 0, channel );
					return;
				}
				ESendCallResult individualSendResult;
				if ( deliveryTraceOut )
				{
					u32_t sequence;
					u32_t numFragments;
					individualSendResult = c.getLink()->addToSendQueue( id, data, len, EHeaderPacketType::Reliable_Ordered, channel, relay, &sequence, &numFragments );
					Util::addTraceCallResult( deliveryTraceOut, c.getEndPoint(), ETraceCallResult::Tracking, sequence, numFragments, channel );
				}
				else
				{
					individualSendResult = c.getLink()->addToSendQueue( id, data, len, EHeaderPacketType::Reliable_Ordered, channel, relay );
				}
				// If at least a single is sent to, consider succes call
				if ( sendResult == ESendCallResult::NotSent && individualSendResult == ESendCallResult::Succes ) 
				{
					sendResult = ESendCallResult::Succes;
				}
			});
		}
		else
		{
			ISocket* sock = C->rn()->getSocket();
			if (!sock) // For connectionless setups, no listen call is required. Implictely open and bind to a socket.
			{
				if (C->rn()->openSocketOnPort( 0 ))
				{
					sock = C->rn()->getSocket(); // reacquire
				}
			}
			if ( sock ) // send on recvNode does not check if the link is connected
			{
				sendResult = C->rn()->send( id, data, len, asEpt(specific), exclude, EHeaderPacketType::Reliable_Ordered, channel, relay, deliveryTraceOut );
			}
			else
			{
				C->setCriticalError(ECriticalError::SocketIsNull, ZERODELAY_FUNCTION_LINE);
				sendResult = ESendCallResult::InternalError;
			}
		}
		return sendResult;
	}

	void ZNode::sendReliableNewest(u8_t packId, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude, bool requiresConnection)
	{
		if (requiresConnection)
		{
			C->cn()->forConnections(asEpt(specific), exclude, [&](Connection& c)
			{
				if (!c.isConnected()) return;
				c.getLink()->addReliableNewest( packId,data, len, groupId, groupBit );
			});
		}
		else // send raw without requiring a connection
		{
			ISocket* sock = C->rn()->getSocket();
			if ( sock )
				C->rn()->sendReliableNewest( packId, groupId, groupBit, data, len, asEpt(specific), exclude );
			else
				C->setCriticalError(ECriticalError::SocketIsNull, ZERODELAY_FUNCTION_LINE);
		}
	}

	ESendCallResult ZNode::sendUnreliableSequenced(u8_t packId, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude, u8_t channel, bool relay, bool requiresConnection)
	{
		ESendCallResult sendResult = ESendCallResult::NotSent;
		if (requiresConnection)
		{
			C->cn()->forConnections(asEpt(specific), exclude, [&](Connection& c)
			{
				if (!c.isConnected()) return;
				ESendCallResult individualResult = c.getLink()->addToSendQueue( packId, data, len, EHeaderPacketType::Unreliable_Sequenced, channel, relay );
				if ( sendResult == ESendCallResult::NotSent && individualResult == ESendCallResult::Succes )
				{
					sendResult = ESendCallResult::Succes;
				}
			});
		}
		else // send raw without requiring a connection
		{
			ISocket* sock = C->rn()->getSocket();
			if ( sock )
			{
			 	sendResult = C->rn()->send( packId, data, len, asEpt(specific), exclude, EHeaderPacketType::Unreliable_Sequenced, channel, relay );
			}
			else
			{
				C->setCriticalError(ECriticalError::SocketIsNull, ZERODELAY_FUNCTION_LINE);
				sendResult = ESendCallResult::InternalError;
			}
		}
		return sendResult;
	}

	void ZNode::bindOnConnectResult(const std::function<void(const ZEndpoint&, EConnectResult)>& cb)
	{
		C->cn()->bindOnConnectResult( cb);
	}

	void ZNode::bindOnNewConnection(const std::function<void (bool directLink, const ZEndpoint&, const std::map<std::string, std::string>&)>& cb)
	{
		C->cn()->bindOnNewConnection( cb );
	}

	void ZNode::bindOnDisconnect(const std::function<void (bool isThisConnection, const ZEndpoint&, EDisconnectReason)>& cb)
	{
		C->cn()->bindOnDisconnect( cb );
	}

	void ZNode::bindOnCustomData(const std::function<void (const ZEndpoint&, u8_t id, const i8_t* data, i32_t length, u8_t channel)>& cb)
	{
		C->bindOnCustomData( cb );
	}

	void ZNode::bindOnGroupUpdated(const std::function<void(const ZEndpoint*, u8_t id)>& cb)
	{
		C->vgn()->bindOnGroupUpdated( [=] ( auto etp, auto id ) 
		{
			ZEndpoint zept;
			ZEndpoint* zeptr = nullptr;
			if ( etp )
			{
				zept  = Util::toZpt(*etp);
				zeptr = &zept;
			}
			cb( zeptr, id );
		});
	}

	void ZNode::bindOnGroupDestroyed(const std::function<void(const ZEndpoint*, u8_t id)>& cb)
	{
		C->vgn()->bindOnGroupDestroyed( [=] ( auto etp, auto id ) 
		{
			ZEndpoint zept;
			ZEndpoint* zeptr = nullptr;
			if ( etp )
			{
				zept  = Util::toZpt(*etp);
				zeptr = &zept;
			}
			cb( zeptr, id );
		});
	}

	void ZNode::setUserDataPtr(void* ptr)
	{
		C->setUserDataPtr( ptr );
	}

	void* ZNode::getUserDataPtr() const
	{
		return C->getUserDataPtr();
	}

	bool ZNode::isPacketDelivered(const ZAckTicket& ticket) const
	{
		return C->rn()->isPacketDelivered(ticket.endpoint, ticket.sequence, ticket.numFragments, ticket.channel);
	}

	void ZNode::deferredCreateVariableGroup(const i8_t* paramData, i32_t paramDataLen)
	{
		C->vgn()->deferredCreateGroup( paramData, paramDataLen );
	}

}