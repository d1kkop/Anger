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

	bool ZEndpoint::resolve(const std::string& name, unsigned short port)
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

	int ZEndpoint::getLastError() const
	{
		return asEpt(this)->getLastError();
	}


	// -------- ZNode ----------------------------------------------------------------------------------------------


	ZNode::ZNode(int sendThreadSleepTimeMs, int keepAliveIntervalSeconds, bool captureSocketErrors) :
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

	EConnectCallResult ZNode::connect(const ZEndpoint& endPoint, const std::string& pw, int timeoutSeconds)
	{
		return p->connect( toEtp(endPoint), pw, timeoutSeconds );
	}

	EConnectCallResult ZNode::connect(const std::string& name, int port, const std::string& pw, int timeoutSeconds)
	{
		return p->connect( name, port, pw, timeoutSeconds );
	}

	EListenCallResult ZNode::listenOn(int port, const std::string& pw, int maxConnections, bool relayEvents)
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

	void ZNode::setMaxIncomingConnections(int maxNumConnections)
	{
		p->setMaxIncomingConnections( maxNumConnections );
	}

	void ZNode::relayClientEvents(bool relay)
	{
		p->relayClientEvents( relay );
	}

	void ZNode::simulatePacketLoss(int percentage)
	{
		p->simulatePacketLoss( percentage );
	}

	void ZNode::sendSingle(unsigned char id, const char* data, int len, const ZEndpoint* specific, bool exclude, EPacketType type, unsigned char channel, bool relay)
	{
		p->beginSend( asEpt(specific), exclude );
		send( id, data, len, type, channel, relay );
		p->endSend();
	}

	void ZNode::beginSend(const ZEndpoint* specific, bool exclude)
	{
		p->beginSend( asEpt( specific ), exclude );
	}

	void ZNode::send(unsigned char id, const char* data, int len, EPacketType type, unsigned char channel, bool relay)
	{
		ISocket* sock = p->getSocket();
		if ( sock && sock->isBound() && sock->isOpen() )
		{
			p->send( id, data, len, type, channel );
		}
		else
		{
			Platform::log( "trying to send data over invalid socket, ignoring send" );
		}
	}

	void ZNode::endSend()
	{
		p->endSend();
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

	void ZNode::bindOnCustomData(std::function<void (const ZEndpoint&, unsigned char id, const char* data, int length, unsigned char channel)> cb)
	{
		p->bindOnCustomData( [=] ( auto etp, auto id, auto data, auto len, auto chan ) {
			cb( toZpt( etp ), id, data, len, chan );
		});
	}

	void ZNode::beginVariableGroup(const char* paramData, int paramDataLen, char channel)
	{
		vgn->beginGroup( paramData, paramDataLen, channel );
	}

	void ZNode::endVariableGroup()
	{
		vgn->endGroup();
	}



	// -------- ZNodePrivate ----------------------------------------------------------------------------------------------


	void ZNodePrivate::priv_beginVarialbeGroupRemote(unsigned int nid, const ZEndpoint& ztp )
	{
		vgn->beginGroupFromRemote(nid, ztp);
	}

	void ZNodePrivate::priv_endVariableGroup()
	{
		vgn->endGroup();
	}

}