#include "Connection.h"
#include "ConnectionNode.h"
#include "Platform.h"


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


	Connection::Connection(bool wasConnector, const EndPoint& endPoint, i32_t timeoutSeconds, i32_t keepAliveIntervalSeconds, i32_t lingerTimeMs):
		RUDPConnection(endPoint),
		m_ConnectTimeoutSeconMs(timeoutSeconds*1000),
		m_KeepAliveIntervalMs(keepAliveIntervalSeconds*1000),
		m_LingerTimeMs(lingerTimeMs),
		m_StartConnectingTS(-1),
		m_KeepAliveTS(-1),
		m_WasConnector(wasConnector),
		m_IsWaitingForKeepAlive(false),
		m_IsDisconnectInvokedHere(false),
		m_State(EConnectionState::Idle)
	{
		if ( m_LingerTimeMs > sm_MaxLingerTimeMs )
			m_LingerTimeMs = sm_MaxLingerTimeMs;
	}

	Connection::~Connection()
	{
	}

	bool Connection::disconnect()
	{
		Ensure_State( Connected )
		m_State = EConnectionState::Disconnecting;
		m_DisconnectTS = ::clock();
		m_IsDisconnectInvokedHere = true;
		sendSystemMessage( EDataPacketType::Disconnect );
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

	bool Connection::sendIncorrectPassword()
	{
		Ensure_State( Idle );
		sendSystemMessage( EDataPacketType::IncorrectPassword );
		return true;
	}

	bool Connection::sendMaxConnectionsReached()
	{
		Ensure_State( Idle );
		sendSystemMessage( EDataPacketType::MaxConnectionsReached );
		return true;
	}

	bool Connection::onReceiveConnectAccept()
	{
		Ensure_State( Connecting );
		m_State = EConnectionState::Connected;
		m_KeepAliveTS = ::clock();
		return true;
	}

	bool Connection::onReceiveRemoteConnected(const i8_t* data, i32_t len, EndPoint& ept)
	{
		// Deliberately no state ensurance
		if (ept.read( data, len ) < 0)
		{
			Platform::log("serialization fail in: %s\n", (__FUNCTION__));
			return false;
		}
		return true;
	}

	bool Connection::onReceiveRemoteDisconnected(const i8_t* data, i32_t len, EndPoint& etp, EDisconnectReason& reason)
	{
		// Deliberately no state ensurance
		i32_t offs = etp.read(data, len);
		if ( offs >= 0 )
		{
			if ( etp == this->getEndPoint() ) // Remote endpoint can only disconnect once
			{
				Ensure_State( Connected );
			}
			reason = (EDisconnectReason)data[offs];
			return true;
		}
		Platform::log("serialization fail in: %s\n", (__FUNCTION__));
		return false;
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
		if ( getTimeSince( m_StartConnectingTS ) >= m_ConnectTimeoutSeconMs )
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
			if ( getTimeSince( m_KeepAliveTS ) > m_KeepAliveIntervalMs )
			{
				sendKeepAliveRequest();
				m_IsWaitingForKeepAlive = true;
				// printf("alive request..\n"); // dbg
			}
		}
		else if ( getTimeSince( m_KeepAliveTS ) > 3000 ) // 3 seconds is rediculous ping, so consider it lost
		{
			m_State = EConnectionState::ConnectionTimedOut;
			return true;
		}
		return false;
	}

	bool Connection::updateDisconnecting()
	{
		Check_State( Disconnecting );
		if ( getTimeSince(m_DisconnectTS) > m_LingerTimeMs )
		{
			// assume afrer this time, that the message was received, otherwise just unlucky
			m_State = EConnectionState::Disconnected;
			return true;
		}
		return false;
	}

	i32_t Connection::getTimeSince(i32_t timestamp) const
	{
		clock_t now = ::clock();
		float elapsedSeconds = float(now - timestamp) / (float)CLOCKS_PER_SEC;
		return i32_t(elapsedSeconds * 1000.f);
	}

	void Connection::sendSystemMessage( EDataPacketType packType, const i8_t* payload, i32_t payloadLen )
	{
		//	static_assert( sizeof(EGameNodePacketType)==1 );
		addToSendQueue( (u8_t)packType, payload, payloadLen, EHeaderPacketType::Reliable_Ordered );
	}
}