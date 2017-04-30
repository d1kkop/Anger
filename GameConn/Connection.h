#pragma once

#include "RUDPConnection.h"
#include <ctime>


namespace Zerodelay
{
	enum class EConnectionState
	{
		Idle,
		Connecting,
		InitiateTimedOut,
		InvalidPassword,
		MaxConnectionsReached,
		Connected,
		ConnectionTimedOut,
		Disconnecting,
		Disconnected
	};

	enum class EGameNodePacketType:unsigned char;
	enum class EDisconnectReason:unsigned char;

	class Connection: public RUDPConnection
	{
	public:
		Connection( const struct EndPoint& endPoint, int keepAliveIntervalSeconds=8, int lingerTimeMs = 300 );
		virtual ~Connection();
		bool disconnect();
		bool acceptDisconnect();
		bool setInvalidPassword();
		bool setMaxConnectionsReached();

		// -- sends
		bool sendConnectRequest(const std::string& pw);
		bool sendConnectAccept();
		bool sendKeepAliveRequest();
		bool sendKeepAliveAnswer();
		bool sendIncorrectPassword();
		bool sendMaxConnectionsReached();
		// -- receives
		bool onReceiveConnectAccept();
		bool onReceiveRemoteConnected(const char* data, int len, EndPoint& ept);
		bool onReceiveRemoteDisconnected(const char* data, int len, EndPoint& ept, EDisconnectReason& reason);
		bool onReceiveKeepAliveAnswer();
		// -- updates
		bool updateConnecting( int connectingTimeoutMs );	// Returns true if there is a state change
		bool updateKeepAlive();								// same
		bool updateDisconnecting();							// same
		// -- getters
		int getTimeSince(int timestamp) const;  // in milliseconds
		EConnectionState getState() const { return m_State; }

	private:
		void sendSystemMessage( EGameNodePacketType type, const char* payload=nullptr, int payloadLen=0 );

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