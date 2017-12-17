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
		InvalidConnectPacket,
		Connected,
		ConnectionTimedOut,
		Disconnected
	};

	enum class EDataPacketType:u8_t;
	enum class EDisconnectReason:u8_t;

	class Connection
	{
	public:
		Connection( class ConnectionNode* connectionNode, bool wasConnector, class RUDPLink* link, i32_t timeoutSeconds=8, i32_t keepAliveIntervalSeconds=8 );
		~Connection();
		void cleanLink();
		void disconnect(bool isDirectLink, const EndPoint& directOrRemoteEndpoint, EDisconnectReason reason, EConnectionState newState, bool sendMsg);
		// -- connect result events --
		void setInvalidPassword();
		void setMaxConnectionsReached();
		void setInvalidConnectPacket();
		// -- sends
		void sendConnectRequest(const std::string& pw);
		void sendConnectAccept();
		void sendKeepAliveRequest();
		void sendKeepAliveAnswer();
		// -- receives
		void onReceiveConnectAccept();
		void onReceiveDisconnect();
		void onReceiveKeepAliveRequest();
		void onReceiveKeepAliveAnswer();
		// -- updates
		void updateConnecting();
		void updateKeepAlive();
		// -- getters
		const EndPoint& getEndPoint() const;
		EConnectionState getState() const;
		bool isConnected() const;
		class RUDPLink* getLink() const; // returns nullptr if connection disconnected

	private:
		void sendSystemMessage( EDataPacketType type, const i8_t* payload=nullptr, i32_t payloadLen=0 );

		class ConnectionNode* m_ConnectionNode;
		class RUDPLink* m_Link;
		struct EndPoint m_Endpoint;
		// how was initialized (from connect accept, or started connected)
		bool m_WasConnector;
		bool m_DisconnectCalled;
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