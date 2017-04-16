#pragma once

#include "RUDPConnection.h"
#include <ctime>


namespace Motor
{
	namespace Anger
	{
		enum class EConnectionState
		{
			Idle,
			Connecting,
			ConnectingTimedOut,
			Connected,
			ConnectionTimedOut,
			Disconnected
		};

		enum class EGameNodePacketType:unsigned char;

		class GameConnection: public RUDPConnection
		{
		public:
			GameConnection( const struct EndPoint& endPoint, int keepAliveIntervalSeconds=-1 );
			virtual ~GameConnection();

			// -- sends
			bool sendConnectRequest();
			bool sendConnectAccept();
			bool sendKeepAliveRequest();
			bool sendKeepAliveAnswer();
			// -- receives
			bool onReceiveConnectAccept();
			bool onReceiveKeepAliveAnswer();
			// -- updates
			bool updateConnecting( int connectingTimeoutMs );
			bool updateKeepAlive();
			// -- getters
			int getTimeSince(int timestamp) const;  // in milliseconds
			EConnectionState getState() const { return m_State; }

		private:
			void sendSystemMessage(EGameNodePacketType type);

			int m_KeepAliveIntervalMs;
			clock_t m_StartConnectingTS;
			clock_t m_KeepAliveRequestTS;
			clock_t m_KeepAliveAnswerTS;
			bool m_IsWaitingForKeepAlive;
			EConnectionState m_State;
		};
	}
}