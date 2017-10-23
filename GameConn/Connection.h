#pragma once

#include <ctime>

#include "Zerodelay.h"
#include "EndPoint.h"


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

	class Connection
	{
	public:
		Connection( class ConnectionNode* connectionNode, bool wasConnector, class RUDPLink* link, i32_t timeoutSeconds=8, i32_t keepAliveIntervalSeconds=8 );
		~Connection();
		bool disconnect(bool sendDisconnect);
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
		bool sendAlreadyConnected();
		// -- receives
		bool onReceiveConnectAccept();
		bool onReceiveKeepAliveRequest();
		bool onReceiveKeepAliveAnswer();
		// -- updates
		bool updateConnecting();	// Returns true if there is a state change
		bool updateKeepAlive();		// same
		bool updateDisconnecting();	// same
		// -- getters
		const EndPoint& getEndPoint() const;
		EConnectionState getState() const;
		bool isConnected() const;

	private:
		void sendSystemMessage( EDataPacketType type, const i8_t* payload=nullptr, i32_t payloadLen=0 );

		class ConnectionNode* m_ConnectionNode;
		class RUDPLink* m_Link;
		// how was initialized (from connect accept, or started connected)
		bool m_WasConnector;
		// timings
		i32_t m_ConnectTimeoutSeconMs;
		i32_t m_KeepAliveIntervalMs;
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