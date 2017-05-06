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
		typedef std::function<void (const EndPoint&, unsigned char, const char*, int, unsigned char)>	CustomDataCallback;

	public:
		ConnectionNode(int sendThreadSleepTimeMs=10, int keepAliveIntervalSeconds=8, bool captureSocketErrors=true);
		virtual ~ConnectionNode();

	public:
		// Connect  to specific endpoint. 
		// A succesful call does not mean a connection is established.
		// To know if a connection is established, bindOnConnectResult.
		EConnectCallResult connect( const EndPoint& endPoint, const std::string& pw="", int timeoutSeconds=8 );
		EConnectCallResult connect( const std::string& name, int port, const std::string& pw="", int timeoutSeconds=8 );

		// Listen on a specific port for incoming connections.
		// Bind onNewConnection to do something with the new connections.
		EListenCallResult listenOn( int port, const std::string& pw="" );

		// Disconnect a specific endpoint.
		EDisconnectCallResult disconnect( const EndPoint& endPoint );

		// Calls disconnect on each connection in the node. 
		// A node has multiple connections in case of server-client, where it is the server or in p2p.
		// Individual results to disconnect() on a connection are ignored.
		void disconnectAll();

		// To be called every update loop. 
		// Calls all bound callback functions
		void update();

		void relayClientEvents(bool is);
		void setPassword( const std::string& pw );

		// Max number of connections of all incoming connection attempts. 
		// Thus if listening on multiple ports, this value is not per port!
		// Default is 32.
		void setMaxIncomingConnections(int maxNumConnections);

		// Callbacks ----------------------------------------------------------------------------------------------------------

		// For handling connect request results
		// Function signature: 
		// void (const EndPoint&, EConnectResult conResult)
		void bindOnConnectResult(ConnectResultCallback cb)		{ bindCallback(m_ConnectResultCallbacks, cb); }

		// For handling new incoming connections
		// Function signature:
		// void (const EndPoint&)
		void bindOnNewConnection(NewConnectionCallback cb)		{ bindCallback(m_NewConnectionCallbacks, cb); }

		// For when connection is closed or gets dropped.
		// In case of client-server, the endpoint is different than the g->getEndPoint() when a remote connection disconnect or got lost.
		// Function signature:
		// void (class Connection*, const Endpoint&, EDisconnect reason)
		void bindOnDisconnect(DisconnectCallback cb)			{ bindCallback(m_DisconnectCallbacks, cb); }

		// For all other data that is specific to the application
		// Function signature:
		// void (const EndPoint&, unsigned char, const char*, int, unsigned char)
		void bindOnCustomData(CustomDataCallback cb)			{ bindCallback(m_CustomDataCallbacks, cb); }

	private:
		// Called by recv thread
		virtual class IConnection* createNewConnection( const EndPoint& endPoint ) const override;
		void removeConnection( const class Connection* g, const char* frmtReason, ... );
		// sends
		void sendRemoteConnected( const class Connection* g );
		void sendRemoteDisconnected( const class Connection* g, EDisconnectReason reason );
		// recvs (Game thread)
		void recvPacket( struct Packet& pack, class Connection* g );
		void recvConnectPacket(const char* payload, int len, class Connection* g);
		void recvConnectAccept(class Connection* g);
		void recvDisconnectPacket( const char* payload, int len, class Connection* g );
		void recvRemoteConnected(class Connection* g, const char* payload, int payloadLen);
		void recvRemoteDisconnected(class Connection* g, const char* payload, int payloadLen);
		void recvInvalidPassword(class Connection* g, const char* payload, int payloadLen);
		void recvMaxConnectionsReached(class Connection* g, const char* payload, int payloadLen);
		void recvRpcPacket( const char* payload, int len, class Connection* g);
		void recvUserPacket(class Connection* g, const Packet& pack);
		// updating
		void updateConnecting( class Connection* g );
		void updateKeepAlive( class Connection* g );
		void updateDisconnecting( class Connection* g );
		// socket
		bool openSocket();
		bool bindSocket(unsigned short port);

		template <typename List, typename Callback>
		void bindCallback( List& list, Callback cb );
		template <typename List, typename Callback>
		void forEachCallback( List& list, Callback cb );

		bool m_SocketIsOpened;
		bool m_SocketIsBound;
		int	 m_KeepAliveIntervalSeconds;
		bool m_RelayClientEvents;
		int  m_MaxIncomingConnections;
		std::string m_Password;
		std::vector<ConnectResultCallback>	m_ConnectResultCallbacks;
		std::vector<DisconnectCallback>		m_DisconnectCallbacks;
		std::vector<NewConnectionCallback>	m_NewConnectionCallbacks;
		std::vector<CustomDataCallback>		m_CustomDataCallbacks;
		std::vector<IConnection*> m_TempConnections; // Copy of current connection list when doing callback functions
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