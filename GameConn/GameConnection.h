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
			Disconnecting,
			Disconnected
		};

		enum class EGameNodePacketType:unsigned char;
		enum class EDisconnectReason:unsigned char;

		class GameConnection: public RUDPConnection
		{
		public:
			GameConnection( const struct EndPoint& endPoint, int keepAliveIntervalSeconds=-1, int lingerTimeMs = 300 );
			virtual ~GameConnection();
			bool disconnect();

			// -- sends
			bool sendConnectRequest();
			bool sendConnectAccept();
			bool sendKeepAliveRequest();
			bool sendKeepAliveAnswer();
			// -- receives
			bool onReceiveConnectAccept();
			bool onReceiveRemoteConnected(const char* data, int len, EndPoint& ept);
			bool onReceiveRemoteDisconnected(const char* data, int len, EndPoint& ept, EDisconnectReason& reason);
			bool onReceiveKeepAliveAnswer();
			// -- updates
			bool updateConnecting( int connectingTimeoutMs );
			bool updateKeepAlive();
			bool updateDisconnecting();
			// -- getters
			int getTimeSince(int timestamp) const;  // in milliseconds
			EConnectionState getState() const { return m_State; }

		private:
			void sendSystemMessage(EGameNodePacketType type);

			int m_KeepAliveIntervalMs;
			int m_LingerTimeMs;
			clock_t m_StartConnectingTS;
			clock_t m_KeepAliveRequestTS;
			clock_t m_KeepAliveAnswerTS;
			clock_t m_DisconnectTS;
			bool m_IsWaitingForKeepAlive;
			EConnectionState m_State;
		};
	}
}