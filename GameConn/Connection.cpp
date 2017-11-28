#include "Connection.h"
#include "ConnectionNode.h"
#include "Platform.h"
#include "RUDPLink.h"


namespace Zerodelay
{
#define Check_State( state ) \
	if ( m_State != EConnectionState::##state ) \
	{\
		return false; \
	}

#define Check_State_NoRet( state ) \
	if ( m_State != EConnectionState::##state ) \
	{\
		return; \
	}

#define Ensure_State( state ) \
	if ( m_State != EConnectionState::##state ) \
	{\
		Platform::log("WARNING state mismatch in %s, wanted state %s, but is %d\n", (__FUNCTION__), #state, m_State); \
		return false;\
	}


	Connection::Connection(ConnectionNode* connectionNode, bool wasConnector, RUDPLink* link, i32_t timeoutSeconds, i32_t keepAliveIntervalSeconds):
		m_ConnectionNode(connectionNode),
		m_Link(link),
		m_WasConnector(wasConnector),
		m_DisconnectCalled(false),
		m_ConnectTimeoutSeconMs(timeoutSeconds*1000),
		m_KeepAliveIntervalMs(keepAliveIntervalSeconds*1000),
		m_StartConnectingTS(-1),
		m_KeepAliveTS(-1),
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
			m_Link->blockAllUpcomingSends();
			m_Link->markPendingDelete();
			m_Link = nullptr; // link itself is deleted from sendThread in receive/dispather node
		}
	}

	void Connection::disconnect(const std::function<void ()>& cb)
	{
		if (m_DisconnectCalled)
			return;

		m_DisconnectCalled = true;
		m_DisconnectTS = ::clock();
		if ( m_State == EConnectionState::Connected ) 
		{
			sendSystemMessage( EDataPacketType::Disconnect );
			cb();
		}
		m_State = EConnectionState::Disconnected;
		cleanLink();
	}

	bool Connection::acceptDisconnect()
	{
		Ensure_State( Connected )
		m_State = EConnectionState::Disconnected;
		return true;
	}

	bool Connection::setInvalidPassword()
	{
		Ensure_State( Connecting );
		m_State = EConnectionState::InvalidPassword;
		return true;
	}

	bool Connection::setMaxConnectionsReached()
	{
		Ensure_State( Connecting );
		m_State = EConnectionState::MaxConnectionsReached;
		return true;
	}

	bool Connection::sendConnectRequest(const std::string& pw)
	{
		Ensure_State( Idle );
		m_State = EConnectionState::Connecting;
		m_StartConnectingTS = ::clock();
		sendSystemMessage( EDataPacketType::ConnectRequest, pw.c_str(), (i32_t)pw.size()+1 );
		return true;
	}

	bool Connection::sendConnectAccept()
	{
		Ensure_State( Idle )
		m_State = EConnectionState::Connected;
		sendSystemMessage( EDataPacketType::ConnectAccept );
		return true;
	}

	bool Connection::sendKeepAliveRequest()
	{
		Ensure_State( Connected );
		m_KeepAliveTS = ::clock();
		sendSystemMessage( EDataPacketType::KeepAliveRequest );
		return true;
	}

	bool Connection::sendKeepAliveAnswer()
	{
		Ensure_State( Connected );
		sendSystemMessage( EDataPacketType::KeepAliveAnswer );
		return true;
	}

	bool Connection::onReceiveConnectAccept()
	{
		Ensure_State( Connecting );
		m_State = EConnectionState::Connected;
		m_KeepAliveTS = ::clock();
		return true;
	}

	bool Connection::onReceiveKeepAliveRequest()
	{
		Ensure_State( Connected );
		sendSystemMessage( EDataPacketType::KeepAliveAnswer );
		return true;
	}

	bool Connection::onReceiveKeepAliveAnswer()
	{
		if ( m_IsWaitingForKeepAlive )
		{
			m_IsWaitingForKeepAlive = false;
			m_KeepAliveTS = ::clock();
			// printf("alive response..\n"); // dbg
			return true;
		}
		return false;
	}

	void Connection::updateConnecting(const std::function<void ()>& cb)
	{
		Check_State_NoRet( Connecting );
		if ( Util::getTimeSince( m_StartConnectingTS ) >= m_ConnectTimeoutSeconMs )
		{
			m_State = EConnectionState::InitiateTimedOut;
			cb();
		}
	}

	void Connection::updateKeepAlive(const std::function<void ()>& cb)
	{
		Check_State_NoRet( Connected );
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
		else if ( Util::getTimeSince( m_KeepAliveTS ) > 3000 ) // 3 seconds is rediculous ping, so consider it lost
		{
			m_State = EConnectionState::ConnectionTimedOut;
			cb();
		}
	}

	const EndPoint& Connection::getEndPoint() const
	{
		assert(m_Link);
		return m_Link->getEndPoint();
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