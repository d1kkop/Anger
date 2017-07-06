#include "Zerodelay.h"
#include "ConnectionNode.h"
#include "RUDPConnection.h"
#include "Socket.h"
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


	ZNode::ZNode(i32_t sendThreadSleepTimeMs, i32_t keepAliveIntervalSeconds, bool captureSocketErrors) :
		p(new ConnectionNode(sendThreadSleepTimeMs, keepAliveIntervalSeconds, captureSocketErrors)),
		vgn(new VariableGroupNode()),
		zp(new ZNodePrivate())
	{
		vgn->m_ZNode = this;
		vgn->m_PrivZ = zp;
		zp->m_ZNode = this;
		zp->vgn = this->vgn;
	}

	ZNode::~ZNode()
	{
		delete p;
		delete vgn;
		delete zp;
	}

	EConnectCallResult ZNode::connect(const ZEndpoint& endPoint, const std::string& pw, i32_t timeoutSeconds)
	{
		return p->connect( toEtp(endPoint), pw, timeoutSeconds );
	}

	EConnectCallResult ZNode::connect(const std::string& name, i32_t port, const std::string& pw, i32_t timeoutSeconds)
	{
		return p->connect( name, port, pw, timeoutSeconds );
	}

	EListenCallResult ZNode::listenOn(i32_t port, const std::string& pw, i32_t maxConnections, bool relayEvents)
	{
		p->setMaxIncomingConnections( maxConnections );
		p->relayClientEvents( relayEvents );
		return p->listenOn( port, pw );
	}

	EDisconnectCallResult ZNode::disconnect(const ZEndpoint& endPoint)
	{
		return p->disconnect( toEtp( endPoint ) );
	}

	void ZNode::disconnectAll()
	{
		return p->disconnectAll();
	}

	void ZNode::update()
	{
		p->update( [=] (auto p, auto g) 
		{
			// unhandled packets, are sent through this callback
			if ( vgn->recvPacket( p, g ) )
				return;
			Platform::log( "received unknown packet from %s", g->getEndPoint().asString().c_str() );
			// TODO handle packet in other systems..
		});
		vgn->update();
	}

	void ZNode::setPassword(const std::string& pw)
	{
		p->setPassword( pw );
	}

	void ZNode::setMaxIncomingConnections(i32_t maxNumConnections)
	{
		p->setMaxIncomingConnections( maxNumConnections );
	}

	void ZNode::relayClientEvents(bool relay)
	{
		p->relayClientEvents( relay );
	}

	void ZNode::simulatePacketLoss(i32_t percentage)
	{
		p->simulatePacketLoss( percentage );
	}

	void ZNode::sendReliableOrdered(u8_t id, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude, u8_t channel, bool relay)
	{
		ISocket* sock = p->getSocket();
		if ( sock )
			p->send( id, data, len, asEpt(specific), exclude, EPacketType::Reliable_Ordered, channel );
		else
			Platform::log( "socket was not created, possibly a platform issue" );
	}

	void ZNode::sendReliableNewest(u8_t packId, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude, bool relay)
	{
		ISocket* sock = p->getSocket();
		if ( sock )
			p->sendReliableNewest( packId, groupId, groupBit, data, len, asEpt(specific), exclude, relay );
		else
			Platform::log( "socket was not created, possibly a platform issue" );
	}

	void ZNode::sendUnreliableSequenced(u8_t packId, const i8_t* data, i32_t len, const ZEndpoint* specific, bool exclude, u8_t channel, bool relay)
	{
		ISocket* sock = p->getSocket();
		if ( sock )
			p->send( packId, data, len, asEpt(specific), exclude, EPacketType::Unreliable_Sequenced, channel );
		else
			Platform::log( "socket was not created, possibly a platform issue" );
	}

	void ZNode::setIsNetworkIdProvider(bool isProvider)
	{
		vgn->setIsNetworkIdProvider( isProvider );
	}

	void ZNode::bindOnConnectResult(std::function<void(const ZEndpoint&, EConnectResult)> cb)
	{
		p->bindOnConnectResult( [=] (auto etp, auto res) {
			cb( toZpt(etp), res );
		});
	}

	void ZNode::bindOnNewConnection(std::function<void (const ZEndpoint&)> cb)
	{
		p->bindOnNewConnection( [=] (auto etp) {
			cb( toZpt(etp) );
		});
	}

	void ZNode::bindOnDisconnect(std::function<void (bool isThisConnection, const ZEndpoint&, EDisconnectReason)> cb)
	{
		p->bindOnDisconnect( [=] (bool thisConn, auto etp, auto reason) {
			cb( thisConn, toZpt( etp ), reason );
		});
	}

	void ZNode::bindOnCustomData(std::function<void (const ZEndpoint&, u8_t id, const i8_t* data, i32_t length, u8_t channel)> cb)
	{
		p->bindOnCustomData( [=] ( auto etp, auto id, auto data, auto len, auto chan ) {
			cb( toZpt( etp ), id, data, len, chan );
		});
	}

	void ZNode::setUserDataPtr(void* ptr)
	{
		p->setUserDataPtr( ptr );
	}

	void* ZNode::getUserDataPtr() const
	{
		return p->getUserDataPtr();
	}

	void ZNode::setUserDataIdx(i32_t idx)
	{
		p->setUserDataIdx( idx );
	}

	i32_t ZNode::gtUserDataIdx() const
	{
		return p->getUserDataIdx();
	}

	void ZNode::beginVariableGroup(const i8_t* paramData, i32_t paramDataLen, i8_t channel, EPacketType type)
	{
		vgn->beginGroup( paramData, paramDataLen, channel, type );
	}

	void ZNode::endVariableGroup()
	{
		vgn->endGroup();
	}



	// -------- ZNodePrivate ----------------------------------------------------------------------------------------------


	void ZNodePrivate::priv_beginVarialbeGroupRemote(u32_t nid, const ZEndpoint& ztp, EPacketType type)
	{
		vgn->beginGroupFromRemote(nid, ztp, type);
	}

	void ZNodePrivate::priv_endVariableGroup()
	{
		vgn->endGroup();
	}

	ZNode* ZNodePrivate::priv_getUserNode() const
	{
		return m_ZNode;
	}

}