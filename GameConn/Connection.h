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

	enum class EDataPacketType:u8_t;
	enum class EDisconnectReason:u8_t;

	class Connection: public RUDPConnection
	{
	public:
		Connection( bool wasConnector, const struct EndPoint& endPoint, i32_t timeoutSeconds=8, i32_t keepAliveIntervalSeconds=8, i32_t lingerTimeMs = 300 );
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
		bool onReceiveRemoteConnected(const i8_t* data, i32_t len, EndPoint& ept);
		bool onReceiveRemoteDisconnected(const i8_t* data, i32_t len, EndPoint& ept, EDisconnectReason& reason);
		bool onReceiveKeepAliveRequest();
		bool onReceiveKeepAliveAnswer();
		// -- updates
		bool updateConnecting();	// Returns true if there is a state change
		bool updateKeepAlive();		// same
		bool updateDisconnecting();	// same
		// -- getters
		i32_t getTimeSince(i32_t timestamp) const;  // in milliseconds
		EConnectionState getState() const { return m_State; }
		// -- IConnection interface
		virtual i32_t getLingerTimeMs() const override { return m_LingerTimeMs; }
		virtual bool isConnected() const override { return m_State == EConnectionState::Connected; }

	private:
		void sendSystemMessage( EDataPacketType type, const i8_t* payload=nullptr, i32_t payloadLen=0 );

		// timings
		i32_t m_ConnectTimeoutSeconMs;
		i32_t m_KeepAliveIntervalMs;
		i32_t m_LingerTimeMs;
		// timestamps
		clock_t m_StartConnectingTS;
		clock_t m_KeepAliveTS;
		clock_t m_DisconnectTS;
		clock_t m_MarkDeleteTS;
		// state
		bool m_IsWaitingForKeepAlive;
		EConnectionState m_State;
	};
}