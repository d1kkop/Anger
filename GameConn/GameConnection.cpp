#include "GameConnection.h"
#include "GameNode.h"

namespace Motor
{
	namespace Anger
	{
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
			if ( m_State != EConnectionState::Connected )
				return false;
			m_State = EConnectionState::Disconnecting;
			m_DisconnectTS = ::clock();
			sendSystemMessage( EGameNodePacketType::Disconnect );
			return true;
		}

		bool GameConnection::sendConnectRequest()
		{
			if ( m_State != EConnectionState::Idle )
				return false;
			m_State = EConnectionState::Connecting;
			m_StartConnectingTS = ::clock();
			sendSystemMessage( EGameNodePacketType::ConnectRequest );
			return true;
		}

		bool GameConnection::sendConnectAccept()
		{
			if ( m_State != EConnectionState::Idle )
				return false;
			m_State = EConnectionState::Connected;
			sendSystemMessage( EGameNodePacketType::ConnectAccept );
			return true;
		}

		bool GameConnection::sendKeepAliveRequest()
		{
			if ( m_State != EConnectionState::Connected || m_KeepAliveIntervalMs <= 0 )
				return false;
			m_KeepAliveRequestTS = ::clock();
			m_IsWaitingForKeepAlive = true;
			sendSystemMessage( EGameNodePacketType::KeepAliveRequest );
			return true;
		}

		bool GameConnection::sendKeepAliveAnswer()
		{
			if ( m_State != EConnectionState::Connected )
				return false;
			sendSystemMessage( EGameNodePacketType::KeepAliveAnswer );
			return true;
		}

		bool GameConnection::onReceiveConnectAccept()
		{
			if ( m_State != EConnectionState::Connecting )
				return false;
			m_State = EConnectionState::Connected;
			m_KeepAliveRequestTS = ::clock();
			return true;
		}

		bool GameConnection::onReceiveRemoteConnected(const char* data, int len, EndPoint& ept)
		{
			if ( m_State != EConnectionState::Connected )
				return false;
			return ept.read( data, len ) > 0;
		}

		bool GameConnection::onReceiveRemoteDisconnected(const char* data, int len, EndPoint& etp, EDisconnectReason& reason)
		{
			if ( m_State != EConnectionState::Connected )
				return false;
			int offs = etp.read(data, len);
			if ( offs >= 0 )
			{
				reason = (EDisconnectReason)data[offs];
				return true;
			}
			return false;
		}

		bool GameConnection::onReceiveKeepAliveAnswer()
		{
			if ( m_State != EConnectionState::Connected )
				return false;
			m_IsWaitingForKeepAlive = false;
			m_KeepAliveAnswerTS = ::clock();
			return true;
		}

		bool GameConnection::updateConnecting( int maxConnectTimeMs )
		{
			if ( m_State != EConnectionState::Connecting )
				return false;
			if ( getTimeSince( m_StartConnectingTS ) >= maxConnectTimeMs )
				m_State = EConnectionState::ConnectingTimedOut;
			return false;
		}

		bool GameConnection::updateKeepAlive()
		{
			if ( m_State != EConnectionState::Connected )
				return false;
			if ( !m_IsWaitingForKeepAlive )
			{
				if ( getTimeSince( m_KeepAliveAnswerTS ) > m_KeepAliveIntervalMs )
				{
					sendKeepAliveRequest();
				}
			}
			else if ( getTimeSince( m_KeepAliveRequestTS ) > 3000 ) // 3 seconds is rediculous ping, so considere it lost
			{
				m_State = EConnectionState::ConnectionTimedOut;
			}
			return true;
		}

		bool GameConnection::updateDisconnecting()
		{
			if ( m_State != EConnectionState::Disconnecting )
				return false;
			if ( getTimeSince(m_DisconnectTS) > m_LingerTimeMs )
			{
				// assume afrer this time, that the message was received, otherwise just unlucky
				m_State = EConnectionState::Disconnected;
			}
			return true;
		}

		int GameConnection::getTimeSince(int timestamp) const
		{
			clock_t now = ::clock();
			float elapsedSeconds = float(now - m_StartConnectingTS) / (float)CLOCKS_PER_SEC;
			return int(elapsedSeconds * 1000.f);
		}

		void GameConnection::sendSystemMessage( EGameNodePacketType packType )
		{
			//	static_assert( sizeof(EGameNodePacketType)==1 );
			beginAddToSendQueue();
			addToSendQueue( (unsigned char)packType, nullptr, 0, EPacketType::Reliable_Ordered );
			endAddToSendQueue();
		}
	}
}
