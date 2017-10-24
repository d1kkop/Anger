#include "Zerodelay.h"
#include "RUDPLink.h"
#include "Socket.h"
#include "CoreNode.h"
#include "RecvNode.h"
#include "ConnectionNode.h"
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
		C(new CoreNode
		(
			routingMethod,
			this,
			(new RecvNode(captureSocketErrors, sendThreadSleepTimeMs)),
			(new ConnectionNode(keepAliveIntervalSeconds)),
			(new VariableGroupNode())
		))
	{
	}

	ZNode::~ZNode()
	{
		delete C;
	}

	EConnectCallResult ZNode::connect(const ZEndpoint& endPoint, const std::string& pw, i32_t timeoutSeconds)
	{
		return C->cn()->connect( toEtp(endPoint), pw, timeoutSeconds );
	}

	EConnectCallResult ZNode::connect(const std::string& name, i32_t port, const std::string& pw, i32_t timeoutSeconds)
	{
		return C->cn()->connect( name, port, pw, timeoutSeconds );
	}

	EListenCallResult ZNode::listenOn(i32_t port, const std::string& pw, i32_t maxConnections, bool relayEvents)
	{
		C->cn()->setMaxIncomingConnections( maxConnections );
		return C->cn()->listenOn( port, pw );
	}

	EDisconnectCallResult ZNode::disconnect(const ZEndpoint& endPoint)
	{
		return C->cn()->disconnect( toEtp( endPoint ), true );
	}

	void ZNode::disconnectAll()
	{
		return C->cn()->disconnectAll(true);
	}

	i32_t ZNode::getNumOpenConnections() const
	{
		return C->cn()->getNumOpenConnections();
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
			if (C->cn()->beginProcessPacketsFor(link->getEndPoint()))
			{
				link->beginPoll();
				Packet pack;
				while (link->poll(pack))
				{
					// try at all nodes, returns false if packet is not processed
					if (!C->cn()->processPacket(pack))
						if (!C->vgn()->processPacket(pack, link->getEndPoint()))
						{
						}
				}
				link->endPoll();
				C->cn()->endProcessPackets();
			}
			C->rn()->unpinLink(link);
			linkIdx++;
		}

		C->cn()->update();
		C->vgn()->update();
	}

	void ZNode::setPassword(const std::string& pw)
	{
		C->cn()->setPassword( pw );
	}

	void ZNode::setMaxIncomingConnections(i32_t maxNumConnections)
	{
		C->cn()->setMaxIncomingConnections( maxNumConnections );
	}

	void ZNode::getConnectionListCopy(std::vector<ZEndpoint>& listOut)
	{
		C->cn()->getConnectionListCopy(listOut);
	}

	Zerodelay::ERoutingMethod ZNode::getRoutingMethod() const
	{
		return C->getRoutingMethod();
	}

	void ZNode::simulatePacketLoss(i32_t percentage)
	{
		C->rn()->simulatePacketLoss( percentage );
	}

	bool ZNode::sendReliableOrdered(u8_t id, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude, u8_t channel, bool relay)
	{
		ISocket* sock = C->rn()->getSocket();
		bool bAdded = false;
		if ( sock )
			bAdded = C->rn()->send( id, data, len, asEpt(specific), exclude, EHeaderPacketType::Reliable_Ordered, channel );
		else
			C->setCriticalError(ECriticalError::SocketIsNull, ZERODELAY_FUNCTION);
		return bAdded;
	}

	void ZNode::sendReliableNewest(u8_t packId, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude)
	{
		ISocket* sock = C->rn()->getSocket();
		if ( sock )
			C->rn()->sendReliableNewest( packId, groupId, groupBit, data, len, asEpt(specific), exclude );
		else
			C->setCriticalError(ECriticalError::SocketIsNull, ZERODELAY_FUNCTION);
	}

	void ZNode::sendUnreliableSequenced(u8_t packId, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude, u8_t channel, bool relay, bool discardSendIfNotConnected)
	{
		ISocket* sock = C->rn()->getSocket();
		if ( sock )
			C->rn()->send( packId, data, len, asEpt(specific), exclude, EHeaderPacketType::Unreliable_Sequenced, channel, discardSendIfNotConnected );
		else
			C->setCriticalError(ECriticalError::SocketIsNull, ZERODELAY_FUNCTION);
	}

	void ZNode::setIsNetworkIdProvider(bool isProvider)
	{
		C->vgn()->setIsNetworkIdProvider( isProvider );
	}

	void ZNode::bindOnConnectResult(const std::function<void(const ZEndpoint&, EConnectResult)>& cb)
	{
		C->cn()->bindOnConnectResult( [=] (auto etp, auto res) 
		{
			cb( toZpt(etp), res );
		});
	}

	void ZNode::bindOnNewConnection(const std::function<void (const ZEndpoint&)>& cb)
	{
		C->cn()->bindOnNewConnection( [=] (auto etp) 
		{
			cb( toZpt(etp) );
		});
	}

	void ZNode::bindOnDisconnect(const std::function<void (bool isThisConnection, const ZEndpoint&, EDisconnectReason)>& cb)
	{
		C->cn()->bindOnDisconnect( [=] (bool thisConn, auto etp, auto reason) 
		{
			cb( thisConn, toZpt( etp ), reason );
		});
	}

	void ZNode::bindOnCustomData(const std::function<void (const ZEndpoint&, u8_t id, const i8_t* data, i32_t length, u8_t channel)>& cb)
	{
		C->cn()->bindOnCustomData( [=] ( auto etp, auto id, auto data, auto len, auto chan ) 
		{
			cb( toZpt( etp ), id, data, len, chan );
		});
	}

	void ZNode::bindOnGroupUpdated(const std::function<void(const ZEndpoint*, u8_t id)>& cb)
	{
		C->vgn()->bindOnGroupUpdated( [=] ( auto etp, auto id ) 
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
		C->vgn()->bindOnGroupDestroyed( [=] ( auto etp, auto id ) 
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
		C->setUserDataPtr( ptr );
	}

	void* ZNode::getUserDataPtr() const
	{
		return C->getUserDataPtr();
	}

	void ZNode::deferredCreateVariableGroup(const i8_t* paramData, i32_t paramDataLen, i8_t channel)
	{
		C->vgn()->deferredCreateGroup( paramData, paramDataLen, channel );
	}

}