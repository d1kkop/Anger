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
		using ConnectResultCallback = std::function<void (const ZEndpoint&, EConnectResult)>;
		using DisconnectCallback	= std::function<void (bool, const ZEndpoint&, EDisconnectReason)>;
		using NewConnectionCallback = std::function<void (bool, const ZEndpoint&)>;

	public:
		ConnectionNode(i32_t keepAliveIntervalSeconds=8);
		~ConnectionNode();
		void postInitialize(class CoreNode* coreNode);

		// state
		EConnectCallResult connect( const EndPoint& endPoint, const std::string& pw="", i32_t timeoutSeconds=8, bool sendRequest=true );
		EConnectCallResult connect( const std::string& name, i32_t port, const std::string& pw="", i32_t timeoutSeconds=8, bool sendRequest=true );
		EListenCallResult listenOn( i32_t port );
		EDisconnectCallResult disconnect( const EndPoint& endPoint );
		void disconnectAll();
		void deleteConnections();
		i32_t getNumOpenConnections() const;
		bool isInConnectionList(const ZEndpoint& ztp) const;
		// flow
		void update();
		void beginProcessPacketsFor(const EndPoint& endPoint);					// returns true if is known connection
		bool processPacket(const struct Packet& pack, class RUDPLink& link);	// returns false if packet was not processed (consumed)
		void endProcessPackets();
		class Connection* getProcessingConnection() const { return m_ProcessingConnection; }
		// setters
		void setPassword( const std::string& pw );
		void setMaxIncomingConnections(i32_t maxNumConnections);
		void setRelayConnectAndDisconnectEvents(bool relay);
		// getters
		bool getRelayConnectAndDisconnect() const { return m_RelayConnectAndDisconnect; }
		void getConnectionListCopy(std::vector<ZEndpoint>& endpoints); // only puts connected connections in list
		// callbacks
		void bindOnConnectResult(const ConnectResultCallback& cb)		{ Util::bindCallback(m_ConnectResultCallbacks, cb); }
		void bindOnNewConnection(const NewConnectionCallback& cb)		{ Util::bindCallback(m_NewConnectionCallbacks, cb); }
		void bindOnDisconnect(const DisconnectCallback& cb)				{ Util::bindCallback(m_DisconnectCallbacks, cb); }
		// iterating
		void forConnections(const EndPoint* specific, bool exclude, const std::function<void (Connection&)>& cb);
		// call callbacks
		void doConnectResultCallbacks(const EndPoint& remote, EConnectResult result);
		void doDisconnectCallbacks(bool directLink, const EndPoint& remote, EDisconnectReason reason);
		void doNewIncomingConnectionCallbacks(bool directLink, const EndPoint& remote);

	private:
		// sends (relay)
		void sendRemoteConnected( const class Connection* g );
		void sendRemoteDisconnected( const class Connection* g, EDisconnectReason reason );
		void sendSystemMessage( class RUDPLink& link, EDataPacketType state, const i8_t* payLoad=nullptr, i32_t len=0 );
		// recvs (Game thread)
		bool recvPacket( const struct Packet& pack, class Connection* g, class RUDPLink& link );
		void handleInvalidConnectAttempt( EDataPacketType responseType, class RUDPLink& link );
		void recvConnectPacket(const i8_t* payload, i32_t len, class RUDPLink& link);
		void recvConnectAccept(class Connection* g);
		void recvDisconnectPacket( const i8_t* payload, i32_t len, class Connection* g );
		void recvRemoteConnected(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvRemoteDisconnected(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvAlreadyConnected(class Connection* g, const i8_t* payload, i32_t payloadLen);
		// updating
		void updateConnecting( class Connection* g );
		void updateKeepAlive( class Connection* g );
		
		bool m_RelayConnectAndDisconnect;
		i32_t m_KeepAliveIntervalSeconds;
		i32_t m_MaxIncomingConnections;
		std::string m_Password;
		class Connection* m_ProcessingConnection;
		std::map<EndPoint, class Connection*, EndPoint::STLCompare> m_Connections;
		std::vector<ConnectResultCallback>	m_ConnectResultCallbacks;
		std::vector<DisconnectCallback>		m_DisconnectCallbacks;
		std::vector<NewConnectionCallback>	m_NewConnectionCallbacks;
		// --- ptrs to other managers
		class CoreNode* m_CoreNode;
		class RecvNode* m_DispatchNode;
	};
}