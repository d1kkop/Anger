#include "Zerodelay.h"
#include "ConnectionNode.h"
#include "RUDPLink.h"
#include "Socket.h"
#include "VariableGroup.h"
#include "VariableGroupNode.h"


namespace Zerodelay
{

	// -------- Support --------------------------------------------------------------------------------------------------

	EndPoint toEtp( const ZEndpoint& z )
	{
		static_assert( sizeof(EndPoint) <= sizeof(ZEndpoint), "ZEndpoint size to small" );
		EndPoint r;
		memcpy( &r, &z, sizeof(EndPoint) ); 
		return r;
	}

	ZEndpoint toZpt( const EndPoint& r )
	{
		static_assert( sizeof(EndPoint) <= sizeof(ZEndpoint), "Zendpoint size is too small" );
		ZEndpoint z;
		memcpy( &z, &r, sizeof(EndPoint) ); 
		return z;
	}

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

	std::string ZEndpoint::asString() const
	{
		return asEpt(this)->asString();
	}

	i32_t ZEndpoint::getLastError() const
	{
		return asEpt(this)->getLastError();
	}


	// -------- ZNode ----------------------------------------------------------------------------------------------


	ZNode::ZNode(ERoutingMethod routingMethod, i32_t sendThreadSleepTimeMs, i32_t keepAliveIntervalSeconds, bool captureSocketErrors) :
		rn(new RecvNode(captureSocketErrors, sendThreadSleepTimeMs)),
		cn(new ConnectionNode(routingMethod, keepAliveIntervalSeconds)),
		vgn(new VariableGroupNode())
	{
		cn->postInitialize(rn);
		rn->postInitialize(cn);
		vgn->postInitialize(this, cn);
	}

	ZNode::~ZNode()
	{
		delete vgn;
		delete cn;
		delete rn;
	}

	EConnectCallResult ZNode::connect(const ZEndpoint& endPoint, const std::string& pw, i32_t timeoutSeconds)
	{
		return cn->connect( toEtp(endPoint), pw, timeoutSeconds );
	}

	EConnectCallResult ZNode::connect(const std::string& name, i32_t port, const std::string& pw, i32_t timeoutSeconds)
	{
		return cn->connect( name, port, pw, timeoutSeconds );
	}

	EListenCallResult ZNode::listenOn(i32_t port, const std::string& pw, i32_t maxConnections, bool relayEvents)
	{
		cn->setMaxIncomingConnections( maxConnections );
		return cn->listenOn( port, pw );
	}

	EDisconnectCallResult ZNode::disconnect(const ZEndpoint& endPoint)
	{
		return cn->disconnect( toEtp( endPoint ), true );
	}

	void ZNode::disconnectAll()
	{
		return cn->disconnectAll(true);
	}

	i32_t ZNode::getNumOpenConnections() const
	{
		return cn->getNumOpenConnections();
	}

	void ZNode::update()
	{
		u32_t linkIdx = 0;
		// When pinned, the link will not be destroyed from memory
		RUDPLink* link = rn->getLinkAndPinIt(linkIdx);
		while (link)
		{
			if (cn->beginProcessPacketsFor(link->getEndPoint()))
			{
				link->beginPoll();
				Packet pack;
				while (link->poll(pack))
				{
					// try at all nodes, returns false if packet is not processed
					if (!cn->processPacket(pack))
						if (!vgn->processPacket(pack, link->getEndPoint()))
						{
						}
				}
				link->endPoll();
				cn->endProcessPackets();
			}
			rn->unpinLink(link);
			linkIdx++;
		}

		cn->update();
		vgn->update();
	}

	void ZNode::setPassword(const std::string& pw)
	{
		cn->setPassword( pw );
	}

	void ZNode::setMaxIncomingConnections(i32_t maxNumConnections)
	{
		cn->setMaxIncomingConnections( maxNumConnections );
	}

	void ZNode::getConnectionListCopy(std::vector<ZEndpoint>& listOut)
	{
		cn->getConnectionListCopy(listOut);
	}

	Zerodelay::ERoutingMethod ZNode::getRoutingMethod() const
	{
		return cn->getRoutingMethod();
	}

	void ZNode::simulatePacketLoss(i32_t percentage)
	{
		rn->simulatePacketLoss( percentage );
	}

	bool ZNode::sendReliableOrdered(u8_t id, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude, u8_t channel, bool relay)
	{
		ISocket* sock = rn->getSocket();
		bool bAdded = false;
		if ( sock )
			bAdded = rn->send( id, data, len, asEpt(specific), exclude, EHeaderPacketType::Reliable_Ordered, channel );
		else
			Platform::log( "socket was not created, possibly a platform issue" );
		return bAdded;
	}

	void ZNode::sendReliableNewest(u8_t packId, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude)
	{
		ISocket* sock = rn->getSocket();
		if ( sock )
			rn->sendReliableNewest( packId, groupId, groupBit, data, len, asEpt(specific), exclude );
		else
			Platform::log( "socket was not created, possibly a platform issue" );
	}

	void ZNode::sendUnreliableSequenced(u8_t packId, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude, u8_t channel, bool relay, bool discardSendIfNotConnected)
	{
		ISocket* sock = rn->getSocket();
		if ( sock )
			rn->send( packId, data, len, asEpt(specific), exclude, EHeaderPacketType::Unreliable_Sequenced, channel, discardSendIfNotConnected );
		else
			Platform::log( "socket was not created, possibly a platform issue" );
	}

	void ZNode::setIsNetworkIdProvider(bool isProvider)
	{
		vgn->setIsNetworkIdProvider( isProvider );
	}

	void ZNode::bindOnConnectResult(const std::function<void(const ZEndpoint&, EConnectResult)>& cb)
	{
		cn->bindOnConnectResult( [=] (auto etp, auto res) {
			cb( toZpt(etp), res );
		});
	}

	void ZNode::bindOnNewConnection(const std::function<void (const ZEndpoint&)>& cb)
	{
		cn->bindOnNewConnection( [=] (auto etp) {
			cb( toZpt(etp) );
		});
	}

	void ZNode::bindOnDisconnect(const std::function<void (bool isThisConnection, const ZEndpoint&, EDisconnectReason)>& cb)
	{
		cn->bindOnDisconnect( [=] (bool thisConn, auto etp, auto reason) {
			cb( thisConn, toZpt( etp ), reason );
		});
	}

	void ZNode::bindOnCustomData(const std::function<void (const ZEndpoint&, u8_t id, const i8_t* data, i32_t length, u8_t channel)>& cb)
	{
		cn->bindOnCustomData( [=] ( auto etp, auto id, auto data, auto len, auto chan ) {
			cb( toZpt( etp ), id, data, len, chan );
		});
	}

	void ZNode::bindOnGroupUpdated(const std::function<void(const ZEndpoint*, u8_t id)>& cb)
	{
		vgn->bindOnGroupUpdated( [=] ( auto etp, auto id ) 
		{
			ZEndpoint zept;
			ZEndpoint* zeptr = nullptr;
			if ( etp )
			{
				zept = toZpt(*etp);
				zeptr = &zept;
			}
			cb( zeptr, id );
		});
	}

	void ZNode::bindOnGroupDestroyed(const std::function<void(const ZEndpoint*, u8_t id)>& cb)
	{
		vgn->bindOnGroupDestroyed( [=] ( auto etp, auto id ) 
		{
			ZEndpoint zept;
			ZEndpoint* zeptr = nullptr;
			if ( etp )
			{
				zept = toZpt(*etp);
				zeptr = &zept;
			}
			cb( zeptr, id );
		});
	}

	void ZNode::setUserDataPtr(void* ptr)
	{
		//cn->setUserDataPtr( ptr ); // TODO centralize
	}

	void* ZNode::getUserDataPtr() const
	{
		// TODO centralize
		return nullptr;
	}

	void ZNode::deferredCreateVariableGroup(const i8_t* paramData, i32_t paramDataLen, i8_t channel)
	{
		vgn->deferredCreateGroup( paramData, paramDataLen, channel );
	}

}