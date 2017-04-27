#pragma once

#include "RecvPoint.h"

#include <cassert>
#include <algorithm>
#include <vector>
#include <functional>
#include <memory>


namespace Supernet
{
	enum class EGameNodePacketType: unsigned char
	{
		ConnectRequest,
		ConnectAccept,
		Disconnect,
		RemoteConnected,
		RemoteDisconnected,
		KeepAliveRequest,
		KeepAliveAnswer,
		IncorrectPassword,
		MaxConnectionsReached,
		Rpc
	};

	#define  USER_ID_OFFSET (unsigned char)(EGameNodePacketType::Rpc)+1


	enum class EConnectCallResult
	{
		Succes,
		CannotResolveHost,
		CannotBind,
		AlreadyExists,
		SocketError
	};

	enum class EListenCallResult
	{
		Succes,
		CannotBind,
		SocketError
	};

	enum class EDisconnectCallResult
	{
		Succes,
		AlreadyCalled,
		UnknownEndpoint
	};

	enum class EConnectResult
	{
		Succes,
		Timedout,
		InvalidPassword,
		MaxConnectionsReached
	};

	enum class EDisconnectReason : unsigned char
	{
		Closed,
		Lost
	};


	class GameNode: public RecvPoint
	{
		typedef std::function<void (const EndPoint&, EConnectResult)>					ConnectResultCallback;
		typedef std::function<void (bool, const EndPoint&, EDisconnectReason)>			DisconnectCallback;
		typedef std::function<void (const EndPoint&)>									NewConnectionCallback;
		typedef std::function<void (const EndPoint&, unsigned char, const char*, int, unsigned char)>	CustomDataCallback;

	public:
		GameNode(int connectTimeoutSeconds=8, int sendThreadSleepTimeMs=2, int keepAliveIntervalSeconds=8, bool captureSocketErrors=true);
		virtual ~GameNode();

	public:
		// Connect  to specific endpoint. 
		// A succesful call does not mean a connection is established.
		// To know if a connection is established, bindOnConnectResult.
		EConnectCallResult connect( const EndPoint& endPoint, const std::string& pw="" );
		EConnectCallResult connect( const std::string& name, int port, const std::string& pw="" );

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

		void setIsTrueServer(bool is);
		void setServerPassword( const std::string& pw );

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
		// void (class GameConnection*, const Endpoint&, EDisconnect reason)
		void bindOnDisconnect(DisconnectCallback cb)			{ bindCallback(m_DisconnectCallbacks, cb); }

		// For all other data that is specific to the application
		// Function signature:
		// void (const EndPoint&, unsigned char, const char*, int, unsigned char)
		void bindOnCustomData(CustomDataCallback cb)			{ bindCallback(m_CustomDataCallbacks, cb); }

	private:
		// Called by recv thread
		virtual class IConnection* createNewConnection( const EndPoint& endPoint ) const override;
		void removeConnection( const class GameConnection* g, const char* frmtReason, ... );
		// sends
		void sendRemoteConnected( const class GameConnection* g );
		void sendRemoteDisconnected( const class GameConnection* g, EDisconnectReason reason );
		// recvs (Game thread)
		void recvPacket( struct Packet& pack, class GameConnection* g );
		void recvConnectPacket(const char* payload, int len, class GameConnection* g);
		void recvConnectAccept(class GameConnection* g);
		void recvDisconnectPacket( const char* payload, int len, class GameConnection* g );
		void recvRemoteConnected(class GameConnection* g, const char* payload, int payloadLen);
		void recvRemoteDisconnected(class GameConnection* g, const char* payload, int payloadLen);
		void recvInvalidPassword(class GameConnection* g, const char* payload, int payloadLen);
		void recvMaxConnectionsReached(class GameConnection* g, const char* payload, int payloadLen);
		void recvRpcPacket( const char* payload, int len, class GameConnection* g);
		void recvUserPacket(class GameConnection* g, const char* payload, int payloadLen, unsigned char channel);
		// updating
		void updateConnecting( class GameConnection* g );
		void updateKeepAlive( class GameConnection* g );
		void updateDisconnecting( class GameConnection* g );
		// socket
		bool openSocket();
		bool bindSocket(unsigned short port);

		template <typename List, typename Callback>
		void bindCallback( List& list, Callback cb );
		template <typename List, typename Callback>
		void forEachCallback( List& list, Callback cb );

		bool m_SocketIsOpened;
		bool m_SocketIsBound;
		int  m_ConnectTimeoutMs;
		int	 m_KeepAliveIntervalSeconds;
		bool m_IsServer;
		int  m_MaxIncomingConnections;
		std::string m_ServerPassword;
		std::vector<ConnectResultCallback>	m_ConnectResultCallbacks;
		std::vector<DisconnectCallback>		m_DisconnectCallbacks;
		std::vector<NewConnectionCallback>	m_NewConnectionCallbacks;
		std::vector<CustomDataCallback>		m_CustomDataCallbacks;
		std::vector<IConnection*> m_TempConnections; // Copy of current connection list when doing callback functions
		std::vector<IConnection*> m_TempConnections2; // For when calling disconnectAll from within a callback function
		std::vector<IConnection*> m_DeadConnections;
	};


	template <typename List, typename Callback>
	void GameNode::bindCallback(List& list, Callback cb)
	{
		list.emplace_back( cb );
	}

	template <typename List, typename Callback>
	void GameNode::forEachCallback(List& list, Callback cb)
	{
		for (auto it = list.begin(); it != list.end(); ++it)
		{
			cb( *it );
		}
	}
}