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
	}

	bool Connection::disconnect()
	{
		if (!m_DisconnectCalled)
		{
			m_DisconnectCalled = true;
			m_DisconnectTS = ::clock();
			if ( m_State == EConnectionState::Connected ) sendSystemMessage( EDataPacketType::Disconnect );
			if ( m_Link )
			{
				m_Link->markPendingDelete();
				m_Link->blockAllUpcomingSends();
			}
			auto oldState = m_State;
			m_State = EConnectionState::Disconnecting;
			return (oldState == EConnectionState::Connected);
		}
		return true;
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

	bool Connection::updateConnecting()
	{
		Check_State( Connecting );
		if ( Util::getTimeSince( m_StartConnectingTS ) >= m_ConnectTimeoutSeconMs )
		{
			m_State = EConnectionState::InitiateTimedOut;
			return true;
		}
		return false;
	}

	bool Connection::updateKeepAlive()
	{
		Check_State( Connected );
		if ( m_KeepAliveIntervalMs <= 0 )
			return false; // discard update
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
			return true;
		}
		return false;
	}

	bool Connection::updateDisconnecting()
	{
		Check_State( Disconnecting );
		if ( Util::getTimeSince(m_DisconnectTS) > RUDPLink::sm_MaxLingerTimeMs )
		{
			// assume afrer this time, that the message was received, otherwise just unlucky
			m_State = EConnectionState::Disconnected;
			return true;
		}
		return false;
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

	void Connection::sendSystemMessage(EDataPacketType packType, const i8_t* payload, i32_t payloadLen)
	{
		assert(m_Link);
		if (!m_Link) return;
		m_Link->addToSendQueue( (u8_t)packType, payload, payloadLen, EHeaderPacketType::Reliable_Ordered );
	}
}