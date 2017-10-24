#pragma once

#include "Zerodelay.h"
#include "EndPoint.h"
#include "Util.h"

#include <cassert>
#include <algorithm>
#include <vector>
#include <functional>
#include <memory>


namespace Zerodelay
{
	class ConnectionNode
	{
		typedef std::function<void (const EndPoint&, EConnectResult)>					ConnectResultCallback;
		typedef std::function<void (bool, const EndPoint&, EDisconnectReason)>			DisconnectCallback;
		typedef std::function<void (const EndPoint&)>									NewConnectionCallback;
		typedef std::function<void (const EndPoint&, u8_t, const i8_t*, i32_t, u8_t)>	CustomDataCallback;

	public:
		ConnectionNode(i32_t keepAliveIntervalSeconds=8);
		~ConnectionNode();
		void postInitialize(class CoreNode* coreNode);

		// state
		EConnectCallResult connect( const EndPoint& endPoint, const std::string& pw="", i32_t timeoutSeconds=8 );
		EConnectCallResult connect( const std::string& name, i32_t port, const std::string& pw="", i32_t timeoutSeconds=8 );
		EListenCallResult listenOn( i32_t port, const std::string& pw="" );
		EDisconnectCallResult disconnect( const EndPoint& endPoint, bool sendDisconnect);
		void disconnectAll(bool sendDisconnect);
		i32_t getNumOpenConnections();
		// flow
		void update();
		bool beginProcessPacketsFor(const EndPoint& endPoint);	// returns true if is known connection
		bool processPacket(const struct Packet& pack);			// returns false if packet was not processed (consumed)
		void endProcessPackets();
		//void update( std::function<void (const Packet&, IConnection*)> unhandledPacketCb );
		// setters
		void setPassword( const std::string& pw );
		void setMaxIncomingConnections(i32_t maxNumConnections);
		void getConnectionListCopy(std::vector<ZEndpoint>& endpoints);
		// getters
		bool isServer() const { return m_IsServer; } // TODO must be here?
		// callbacks
		void bindOnConnectResult(const ConnectResultCallback& cb)		{ Util::bindCallback(m_ConnectResultCallbacks, cb); }
		void bindOnNewConnection(const NewConnectionCallback& cb)		{ Util::bindCallback(m_NewConnectionCallbacks, cb); }
		void bindOnDisconnect(const DisconnectCallback& cb)				{ Util::bindCallback(m_DisconnectCallbacks, cb); }
		void bindOnCustomData(const CustomDataCallback& cb)				{ Util::bindCallback(m_CustomDataCallbacks, cb); }

	private:
		void prepareConnectionForDelete(class Connection* g, const i8_t* fmt, ...);
		void prepareConnectionForDelete(class Connection* g );
		// sends (relay)
		void sendRemoteConnected( const class Connection* g );
		void sendRemoteDisconnected( const class Connection* g, EDisconnectReason reason );
		// recvs (Game thread)
		bool recvPacket( const struct Packet& pack, class Connection* g );
		void recvConnectPacket(const i8_t* payload, i32_t len, class Connection* g);
		void recvConnectAccept(class Connection* g);
		void recvDisconnectPacket( const i8_t* payload, i32_t len, class Connection* g );
		void recvRemoteConnected(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvRemoteDisconnected(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvInvalidPassword(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvMaxConnectionsReached(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvAlreadyConnected(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvRpcPacket( const i8_t* payload, i32_t len, class Connection* g);
		void recvUserPacket(class Connection* g, const Packet& pack);
		// updating
		void updateConnecting( class Connection* g );
		void updateKeepAlive( class Connection* g );
		void updateDisconnecting( class Connection* g );
		
		bool m_IsServer; // TODO must be here?
		i32_t m_KeepAliveIntervalSeconds;
		i32_t m_MaxIncomingConnections;
		std::string m_Password;
		class Connection* m_ProcessingConnection;
		std::map<EndPoint, class Connection*, EndPoint::STLCompare> m_Connections;
		std::vector<ConnectResultCallback>	m_ConnectResultCallbacks;
		std::vector<DisconnectCallback>		m_DisconnectCallbacks;
		std::vector<NewConnectionCallback>	m_NewConnectionCallbacks;
		std::vector<CustomDataCallback>		m_CustomDataCallbacks;
		// --- ptrs to other managers
		class CoreNode* m_CoreNode;
		class RecvNode* m_DispatchNode;
	};
}