#include "Connection.h"
#include "ConnectionNode.h"
#include "Platform.h"
#include "RUDPLink.h"


namespace Zerodelay
{
#define Check_State( state ) \
	if ( m_State != EConnectionState::##state ) \
	{\
		return; \
	}

#define Ensure_State( state ) \
	if ( m_State != EConnectionState::##state ) \
	{\
		Platform::log("WARNING state mismatch in %s, wanted state %s, but is %d\n", (__FUNCTION__), #state, m_State); \
		return; \
	}


	Connection::Connection(ConnectionNode* connectionNode, bool wasConnector, RUDPLink* link, i32_t timeoutSeconds, i32_t keepAliveIntervalSeconds):
		m_ConnectionNode(connectionNode),
		m_Link(link),
		m_Endpoint(link->getEndPoint()),
		m_WasConnector(wasConnector),
		m_DisconnectCalled(false),
		m_ConnectTimeoutSeconMs(timeoutSeconds*1000),
		m_KeepAliveIntervalMs(keepAliveIntervalSeconds*1000),
		m_StartConnectingTS(-1),
		m_KeepAliveTS(-1),
		m_connectId(0),
		m_IsWaitingForKeepAlive(false),
		m_State(EConnectionState::Idle)
	{
	}

	Connection::~Connection()
	{
		cleanLink();
	}

	void Connection::cleanLink()
	{
		if (m_Link)
		{
			Platform::log("Clean link called on connection to %s.", m_Endpoint.toIpAndPort().c_str());
			m_Link->blockAllUpcomingSends();
			m_Link->markPendingDelete();
			m_Link = nullptr; // link itself is deleted from sendThread in receive/dispather node
		}
	}

	void Connection::disconnect(bool isDirectLink, const EndPoint& directOrRemoteEndpoint, EDisconnectReason reason, EConnectionState newState, bool sendMsg)
	{
		if (m_DisconnectCalled)
			return;
		m_DisconnectCalled = true;
		m_DisconnectTS = ::clock();
		if ( m_State == EConnectionState::Connected ) 
		{
			if (sendMsg) sendSystemMessage( EDataPacketType::Disconnect );
			m_ConnectionNode->doDisconnectCallbacks( isDirectLink, directOrRemoteEndpoint, reason );
		}
		m_State = newState;
		cleanLink();
	}

	void Connection::setInvalidPassword()
	{
		Ensure_State( Connecting );
		m_State = EConnectionState::InvalidPassword;
		m_ConnectionNode->doConnectResultCallbacks( getEndPoint(), EConnectResult::InvalidPassword );
		Platform::log("Received invalid password for connection %s.", getEndPoint().toIpAndPort().c_str());
	}

	void Connection::setMaxConnectionsReached()
	{
		Ensure_State( Connecting );
		m_State = EConnectionState::MaxConnectionsReached;
		m_ConnectionNode->doConnectResultCallbacks( getEndPoint(), EConnectResult::MaxConnectionsReached );
		Platform::log("Received max connections reached for connection %s.", getEndPoint().toIpAndPort().c_str());
	}

	void Connection::setInvalidConnectPacket()
	{
		Ensure_State(Connecting);
		m_State = EConnectionState::InvalidConnectPacket;
		m_ConnectionNode->doConnectResultCallbacks(getEndPoint(), EConnectResult::InvalidConnectPacket);
		Platform::log("Received invalid connect packet for connection %s.", getEndPoint().toIpAndPort().c_str());
	}

	bool Connection::sendConnectRequest(const std::string& pw, const std::map<std::string, std::string>& additionalData)
	{
		assert( m_State == EConnectionState::Idle ); // just called after creation
		m_State = EConnectionState::Connecting;
		m_StartConnectingTS = ::clock();
		m_connectId = ::rand();
		i32_t dstSize = ZERODELAY_BUFF_SIZE;
		i8_t mapBuffer[ZERODELAY_BUFF_RECV_SIZE]; // deliberately bigger than dstSize
		i8_t* ptr = mapBuffer;
		bool bSucces = false;
		ptr = Util::appendString2( ptr, dstSize, pw.c_str(), bSucces );
		for (auto& kvp : additionalData)
		{
			ptr = Util::appendString2( ptr, dstSize, kvp.first.c_str(), bSucces );
			if (!bSucces) return false;
			ptr = Util::appendString2( ptr, dstSize, kvp.second.c_str(), bSucces );
			if (!bSucces) return false;
		}
		sendSystemMessage( EDataPacketType::ConnectRequest, ptr, ZERODELAY_BUFF_SIZE-dstSize );
		return true;
	}

	void Connection::sendConnectAccept()
	{
		assert( m_State == EConnectionState::Idle ); // just called after creation
		m_State = EConnectionState::Connected;
		sendSystemMessage( EDataPacketType::ConnectAccept );
	}

	void Connection::sendKeepAliveRequest()
	{
		Check_State( Connected );
		m_KeepAliveTS = ::clock();
		sendSystemMessage( EDataPacketType::KeepAliveRequest );
	}

	void Connection::sendKeepAliveAnswer()
	{
		Check_State( Connected );
		sendSystemMessage( EDataPacketType::KeepAliveAnswer );
	}

	void Connection::onReceiveConnectAccept()
	{
		Check_State( Connecting );
		m_State = EConnectionState::Connected;
		m_KeepAliveTS = ::clock();
		m_ConnectionNode->doConnectResultCallbacks(getEndPoint(), EConnectResult::Succes);
		Platform::log( "Connection accepted to %s.", getEndPoint().toIpAndPort().c_str() );
	}

	void Connection::onReceiveDisconnect()
	{
		Ensure_State( Connected )
		disconnect(true, getEndPoint(), EDisconnectReason::Closed, EConnectionState::Disconnected, false);
		Platform::log( "Disconnect received, removing connection %s.", getEndPoint().toIpAndPort().c_str() );
	}

	void Connection::onReceiveKeepAliveRequest()
	{
		Ensure_State( Connected );
		sendSystemMessage( EDataPacketType::KeepAliveAnswer );
	}

	void Connection::onReceiveKeepAliveAnswer()
	{
		if ( m_IsWaitingForKeepAlive )
		{
			m_IsWaitingForKeepAlive = false;
			m_KeepAliveTS = ::clock();
			// printf("received keep alive answer...\n"); // dbg
		}
	}

	void Connection::updateConnecting()
	{
		Check_State( Connecting );
		if ( Util::getTimeSince( m_StartConnectingTS ) >= m_ConnectTimeoutSeconMs )
		{
			m_State = EConnectionState::InitiateTimedOut;
			m_ConnectionNode->doConnectResultCallbacks(getEndPoint(), EConnectResult::Timedout);
			cleanLink();
			Platform::log("Connection attempt timed out to %s.", getEndPoint().toIpAndPort().c_str());
		}
	}

	void Connection::updateKeepAlive()
	{
		Check_State( Connected );
		if ( m_KeepAliveIntervalMs <= 0 )
			return; // discard update
		if ( !m_IsWaitingForKeepAlive )
		{
			if ( Util::getTimeSince( m_KeepAliveTS ) > m_KeepAliveIntervalMs )
			{
				sendKeepAliveRequest();
				m_IsWaitingForKeepAlive = true;
				// printf("alive request..\n"); // dbg
			}
		}
		else if ( Util::getTimeSince( m_KeepAliveTS ) > 5000 ) // 5 seconds is rediculous ping, so consider it lost
		{
			disconnect(true, getEndPoint(), EDisconnectReason::Lost, EConnectionState::ConnectionTimedOut, false);
			Platform::log("Connection timed out to %s.", getEndPoint().toIpAndPort().c_str());
		}
	}

	const EndPoint& Connection::getEndPoint() const
	{
		return m_Endpoint;
	}

	EConnectionState Connection::getState() const
	{
		return m_State;
	}

	bool Connection::isConnected() const
	{
		return getState() == EConnectionState::Connected;
	}

	class RUDPLink* Connection::getLink() const
	{
		return m_Link;
	}

	void Connection::sendSystemMessage(EDataPacketType packType, const i8_t* payload, i32_t payloadLen)
	{
		assert(m_Link);
		if (!m_Link) return;
		m_Link->addToSendQueue( (u8_t)packType, payload, payloadLen, EHeaderPacketType::Reliable_Ordered );
	}

}