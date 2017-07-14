#pragma once

#include "Zerodelay.h"
#include "RecvPoint.h"

#include <cassert>
#include <algorithm>
#include <vector>
#include <functional>
#include <memory>


namespace Zerodelay
{
	class ConnectionNode: public RecvPoint
	{
		typedef std::function<void (const EndPoint&, EConnectResult)>					ConnectResultCallback;
		typedef std::function<void (bool, const EndPoint&, EDisconnectReason)>			DisconnectCallback;
		typedef std::function<void (const EndPoint&)>									NewConnectionCallback;
		typedef std::function<void (const EndPoint&, u8_t, const i8_t*, i32_t, u8_t)>	CustomDataCallback;

	public:
		ConnectionNode(i32_t sendThreadSleepTimeMs=10, i32_t keepAliveIntervalSeconds=8, bool captureSocketErrors=true);
		virtual ~ConnectionNode();

	public:
		// state
		EConnectCallResult connect( const EndPoint& endPoint, const std::string& pw="", i32_t timeoutSeconds=8 );
		EConnectCallResult connect( const std::string& name, i32_t port, const std::string& pw="", i32_t timeoutSeconds=8 );
		EListenCallResult listenOn( i32_t port, const std::string& pw="" );
		EDisconnectCallResult disconnect( const EndPoint& endPoint );
		void disconnectAll();
		// flow
		void update( std::function<void (const Packet&, IConnection*)> unhandledPacketCb );
		// setters
		void setPassword( const std::string& pw );
		void setMaxIncomingConnections(i32_t maxNumConnections);
		void getConnectionListCopy(std::vector<ZEndpoint>& endpoints);
		// getters
		ERoutingMethod getRoutingMethod() const;
		// callbacks
		void bindOnConnectResult(ConnectResultCallback cb)		{ bindCallback(m_ConnectResultCallbacks, cb); }
		void bindOnNewConnection(NewConnectionCallback cb)		{ bindCallback(m_NewConnectionCallbacks, cb); }
		void bindOnDisconnect(DisconnectCallback cb)			{ bindCallback(m_DisconnectCallbacks, cb); }
		void bindOnCustomData(CustomDataCallback cb)			{ bindCallback(m_CustomDataCallbacks, cb); }

	private:
		// called by recv thread
		virtual class IConnection* createNewConnection( const EndPoint& endPoint ) const override;
		void removeConnection( const class Connection* g, const i8_t* frmtReason, ... );
		// sends (relay)
		void sendRemoteConnected( const class Connection* g );
		void sendRemoteDisconnected( const class Connection* g, EDisconnectReason reason );
		// recvs (Game thread)
		bool recvPacket( struct Packet& pack, class Connection* g );
		void recvConnectPacket(const i8_t* payload, i32_t len, class Connection* g);
		void recvConnectAccept(class Connection* g);
		void recvDisconnectPacket( const i8_t* payload, i32_t len, class Connection* g );
		void recvRemoteConnected(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvRemoteDisconnected(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvInvalidPassword(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvMaxConnectionsReached(class Connection* g, const i8_t* payload, i32_t payloadLen);
		void recvRpcPacket( const i8_t* payload, i32_t len, class Connection* g);
		void recvUserPacket(class Connection* g, const Packet& pack);
		// updating
		void updateConnecting( class Connection* g );
		void updateKeepAlive( class Connection* g );
		void updateDisconnecting( class Connection* g );
		// socket
		bool openSocket();
		bool bindSocket(u16_t port);

		template <typename List, typename Callback>
		void bindCallback( List& list, Callback cb );
		template <typename List, typename Callback>
		void forEachCallback( List& list, Callback cb );

		bool m_SocketIsOpened;
		bool m_SocketIsBound;
		i32_t m_KeepAliveIntervalSeconds;
		i32_t m_MaxIncomingConnections;
		std::string m_Password;
		ERoutingMethod m_RoutingMethod;
		std::vector<ConnectResultCallback>	m_ConnectResultCallbacks;
		std::vector<DisconnectCallback>		m_DisconnectCallbacks;
		std::vector<NewConnectionCallback>	m_NewConnectionCallbacks;
		std::vector<CustomDataCallback>		m_CustomDataCallbacks;
		std::vector<IConnection*> m_TempConnections;  // Copy of current connection list when doing callback functions
		std::vector<IConnection*> m_TempConnections2; // For when calling disconnectAll from within a callback function
		std::vector<IConnection*> m_DeadConnections;
	};


	template <typename List, typename Callback>
	void ConnectionNode::bindCallback(List& list, Callback cb)
	{
		list.emplace_back( cb );
	}

	template <typename List, typename Callback>
	void ConnectionNode::forEachCallback(List& list, Callback cb)
	{
		for (auto it = list.begin(); it != list.end(); ++it)
		{
			cb( *it );
		}
	}
}