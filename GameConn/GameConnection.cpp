#include "GameConnection.h"
#include "GameNode.h"
#include "Platform.h"


namespace Zeroone
{
#define Check_State( state ) \
	if ( m_State != EConnectionState::##state ) \
	{\
		return;\
	}

#define Ensure_State( state ) \
	if ( m_State != EConnectionState::##state ) \
	{\
		Platform::log("WARNING state mismatch in %s, wanted state %s, but is %d\n", (__FUNCTION__), #state, m_State); \
		return false;\
	}


	GameConnection::GameConnection(const EndPoint& endPoint, int keepAliveIntervalSeconds, int lingerTimeMs):
		RUDPConnection(endPoint),
		m_KeepAliveIntervalMs(keepAliveIntervalSeconds*1000),
		m_LingerTimeMs(lingerTimeMs),
		m_StartConnectingTS(-1),
		m_KeepAliveRequestTS(-1),
		m_KeepAliveAnswerTS(-1),
		m_IsWaitingForKeepAlive(false),
		m_State(EConnectionState::Idle)
	{
	}

	GameConnection::~GameConnection()
	{
	}

	bool GameConnection::disconnect()
	{
		Ensure_State( Connected )
		m_State = EConnectionState::Disconnecting;
		m_DisconnectTS = ::clock();
		sendSystemMessage( EGameNodePacketType::Disconnect );
		return true;
	}

	bool GameConnection::acceptDisconnect()
	{
		Ensure_State( Connected )
		m_State = EConnectionState::Disconnected;
		return true;
	}

	bool GameConnection::setInvalidPassword()
	{
		Ensure_State( Connecting );
		m_State = EConnectionState::InvalidPassword;
		return true;
	}

	bool GameConnection::sendConnectRequest(const std::string& pw)
	{
		Ensure_State( Idle );
		m_State = EConnectionState::Connecting;
		m_StartConnectingTS = ::clock();
		sendSystemMessage( EGameNodePacketType::ConnectRequest, pw.c_str(), (int)pw.size()+1 );
		return true;
	}

	bool GameConnection::sendConnectAccept()
	{
		Ensure_State( Idle )
		m_State = EConnectionState::Connected;
		sendSystemMessage( EGameNodePacketType::ConnectAccept );
		return true;
	}

	bool GameConnection::sendKeepAliveRequest()
	{
		if ( m_KeepAliveIntervalMs <= 0 ) // dont do keep alive requests
			return false;
		Ensure_State( Connected );
		m_KeepAliveRequestTS = ::clock();
		m_IsWaitingForKeepAlive = true;
		sendSystemMessage( EGameNodePacketType::KeepAliveRequest );
		return true;
	}

	bool GameConnection::sendKeepAliveAnswer()
	{
		Ensure_State( Connected );
		sendSystemMessage( EGameNodePacketType::KeepAliveAnswer );
		return true;
	}

	bool GameConnection::sendIncorrectPassword()
	{
		Ensure_State( Idle );
		sendSystemMessage( EGameNodePacketType::IncorrectPassword );
		return true;
	}

	bool GameConnection::onReceiveConnectAccept()
	{
		Ensure_State( Connecting );
		m_State = EConnectionState::Connected;
		m_KeepAliveRequestTS = ::clock();
		return true;
	}

	bool GameConnection::onReceiveRemoteConnected(const char* data, int len, EndPoint& ept)
	{
		// Deliberately no state ensurance
		if (ept.read( data, len ) > 0)
		{
			Platform::log("serialization fail in: %s\n", (__FUNCTION__));
			return false;
		}
		return true;
	}

	bool GameConnection::onReceiveRemoteDisconnected(const char* data, int len, EndPoint& etp, EDisconnectReason& reason)
	{
		// Deliberately no state ensurance
		int offs = etp.read(data, len);
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

	bool GameConnection::onReceiveKeepAliveAnswer()
	{
	//	Check_State( Connected ); // can be received after state switch to disconnect
		m_IsWaitingForKeepAlive = false;
		m_KeepAliveAnswerTS = ::clock();
		return true;
	}

	void GameConnection::updateConnecting( int maxConnectTimeMs )
	{
		Check_State( Connecting );
		if ( getTimeSince( m_StartConnectingTS ) >= maxConnectTimeMs )
		{
			m_State = EConnectionState::InitiateTimedOut;
		}
	}

	void GameConnection::updateKeepAlive()
	{
		Check_State( Connected );
		if ( m_KeepAliveIntervalMs <= 0 )
			return;
		if ( !m_IsWaitingForKeepAlive )
		{
			if ( getTimeSince( m_KeepAliveAnswerTS ) > m_KeepAliveIntervalMs )
			{
				sendKeepAliveRequest();
			}
		}
		else if ( getTimeSince( m_KeepAliveRequestTS ) > 3000 ) // 3 seconds is rediculous ping, so consider it lost
		{
			m_State = EConnectionState::ConnectionTimedOut;
		}
	}

	void GameConnection::updateDisconnecting()
	{
		Check_State( Disconnecting );
		if ( getTimeSince(m_DisconnectTS) > m_LingerTimeMs )
		{
			// assume afrer this time, that the message was received, otherwise just unlucky
			m_State = EConnectionState::Disconnected;
		}
	}

	int GameConnection::getTimeSince(int timestamp) const
	{
		clock_t now = ::clock();
		float elapsedSeconds = float(now - timestamp) / (float)CLOCKS_PER_SEC;
		return int(elapsedSeconds * 1000.f);
	}

	void GameConnection::sendSystemMessage( EGameNodePacketType packType, const char* payload, int payloadLen )
	{
		//	static_assert( sizeof(EGameNodePacketType)==1 );
		beginAddToSendQueue();
		addToSendQueue( (unsigned char)packType, payload, payloadLen, EPacketType::Reliable_Ordered );
		endAddToSendQueue();
	}
}