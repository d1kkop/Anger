#include "GameConnection.h"
#include "GameNode.h"

namespace Motor
{
	#define Check_State( state, notify ) \
		if ( m_State != EConnectionState::##state ) \
		{\
			if ( (notify) ) { \
				printf("WARNING state mismatch in %s, wanted state %s, but is %d\n", (__FUNCTION__), #state, m_State ); \
			} \
			return false;\
		}	

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
			Check_State( Connected, true )
			m_State = EConnectionState::Disconnecting;
			m_DisconnectTS = ::clock();
			sendSystemMessage( EGameNodePacketType::Disconnect );
			return true;
		}

		bool GameConnection::acceptDisconnect()
		{
			Check_State( Connected, true )
			m_State = EConnectionState::Disconnected;
			return true;
		}

		bool GameConnection::setInvalidPassword()
		{
			Check_State( Connecting, true );
			m_State = EConnectionState::InvalidPassword;
			return true;
		}

		bool GameConnection::sendConnectRequest(const std::string& pw)
		{
			Check_State( Idle, true );
			m_State = EConnectionState::Connecting;
			m_StartConnectingTS = ::clock();
			sendSystemMessage( EGameNodePacketType::ConnectRequest, pw.c_str(), (int)pw.size()+1 );
			return true;
		}

		bool GameConnection::sendConnectAccept()
		{
			Check_State( Idle, true )
			m_State = EConnectionState::Connected;
			sendSystemMessage( EGameNodePacketType::ConnectAccept );
			return true;
		}

		bool GameConnection::sendKeepAliveRequest()
		{
			if ( m_KeepAliveIntervalMs <= 0 ) // dont do keep alive requests
				return false;
			Check_State( Connected, true );
			m_KeepAliveRequestTS = ::clock();
			m_IsWaitingForKeepAlive = true;
			sendSystemMessage( EGameNodePacketType::KeepAliveRequest );
			return true;
		}

		bool GameConnection::sendKeepAliveAnswer()
		{
			Check_State( Connected, true );
			sendSystemMessage( EGameNodePacketType::KeepAliveAnswer );
			return true;
		}

		bool GameConnection::sendIncorrectPassword()
		{
			Check_State( Idle, true );
			sendSystemMessage( EGameNodePacketType::IncorrectPassword );
			return true;
		}

		bool GameConnection::onReceiveConnectAccept()
		{
			Check_State( Connecting, true );
			m_State = EConnectionState::Connected;
			m_KeepAliveRequestTS = ::clock();
			return true;
		}

		bool GameConnection::onReceiveRemoteConnected(const char* data, int len, EndPoint& ept)
		{
		 	// Check_State( Connected ); // from remote endpoints (not our direct endpoint) this can be received after disconnect
			if (ept.read( data, len ) > 0)
			{
				printf("serialization fail in: %s\n", (__FUNCTION__));
				return false;
			}
			return true;
		}

		bool GameConnection::onReceiveRemoteDisconnected(const char* data, int len, EndPoint& etp, EDisconnectReason& reason)
		{
		//	Check_State( Connected ); // from remote endpoints (not our direct endpoint) this can be received after disconnect
			int offs = etp.read(data, len);
			if ( offs >= 0 )
			{
				if ( etp == this->getEndPoint() ) // Remote endpoint can only disconnect once
				{
					Check_State( Connected, true );
				}
				reason = (EDisconnectReason)data[offs];
				return true;
			}
			printf("serialization fail in: %s\n", (__FUNCTION__));
			return false;
		}

		bool GameConnection::onReceiveKeepAliveAnswer()
		{
		//	Check_State( Connected ); // can be received after state switch to disconnect
			m_IsWaitingForKeepAlive = false;
			m_KeepAliveAnswerTS = ::clock();
			return true;
		}

		bool GameConnection::updateConnecting( int maxConnectTimeMs )
		{
			Check_State( Connecting, false );
			if ( getTimeSince( m_StartConnectingTS ) >= maxConnectTimeMs )
			{
				m_State = EConnectionState::InitiateTimedOut;
			}
			return true;
		}

		bool GameConnection::updateKeepAlive()
		{
			if ( m_KeepAliveIntervalMs <= 0 )
				return false;
			Check_State( Connected, false );
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
			return true;
		}

		bool GameConnection::updateDisconnecting()
		{
			Check_State( Disconnecting, false );
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
}
