#pragma once

#include "RecvPoint.h"

#include <cassert>
#include <algorithm>
#include <vector>
#include <functional>
#include <memory>


namespace Motor
{
	namespace Anger
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
			Rpc
		};

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
			Timedout
		};

		enum class EDisconnectReason : unsigned char
		{
			Closed,
			Lost
		};

		class GameNode: public RecvPoint
		{
			typedef std::function<void (class GameConnection*, EConnectResult)>					ConnectResultCallback;
			typedef std::function<void (class GameConnection*, const EndPoint&, EDisconnectReason)>	DisconnectCallback;
			typedef std::function<void (class GameConnection*, const EndPoint&)>					NewConnectionCallback;
			typedef std::function<void (class GameConnection*, unsigned char, const char*, int, unsigned char)>	CustomDataCallback;

		public:
			GameNode(int connectTimeoutSeconds=12, int sendThreadSleepTimeMs=2, bool captureSocketErrors=true);
			virtual ~GameNode();

		public:
			// Connect  to specific endpoint. 
			// A succesful call does not mean a connection is established.
			// To know if a connection is established, bindOnConnectResult.
			EConnectCallResult connect( const EndPoint& endPoint );
			EConnectCallResult connect( const std::string& name, int port );

			// Listen on a specific port for incoming connections.
			// Bind onNewConnection to do something with the new connections.
			EListenCallResult listenOn( int port );

			// Disconnect a specific endpoint.
			EDisconnectCallResult disconnect( const EndPoint& endPoint );

			// Calls disconnect on each connection in the node. 
			// A node has multiple connections in case of server-client, where it is the server or in p2p.
			// Individual results to disconnect() on a connection are ignored.
			void disconnectAll();

			// To be called every update loop. 
			// Calls all bound callback functions
			void update();

			// Callbacks ----------------------------------------------------------------------------------------------------------

			// For handling connect request results
			// Function signature: 
			// void (class GameConnection* conn, EConnectResult conResult)
			void bindOnConnectResult(ConnectResultCallback cb)		{ bindCallback(m_ConnectResultCallbacks, cb); }

			// For handling new incoming connections
			// Function signature:
			// void (class GameConnection* conn)
			void bindOnNewConnection(NewConnectionCallback cb)		{ bindCallback(m_NewConnectionCallbacks, cb); }

			// For when connection is closed or gets dropped.
			// In case of client-server, the endpoint is different than the g->getEndPoint() when a remote connection disconnect or got lost.
			// Function signature:
			// void (class GameConnection* conn, Endpoint etp, EDisconnect reason)
			void bindOnDisconnect(DisconnectCallback cb)			{ bindCallback(m_DisconnectCallbacks, cb); }

			// For all other data that is specific to the application
			void bindOnCustomData(CustomDataCallback cb)			{ bindCallback(m_CustomDataCallbacks, cb); }

		private:
			// Called by recv thread
			virtual class IConnection* createNewConnection( const EndPoint& endPoint ) const override;
			// sends
			void sendRemoteAccepted( const GameConnection* g );
			void sendRemoteDisconnected( const GameConnection* g, EDisconnectReason reason );
			// recvs (Game thread)
			void recvPacket( struct Packet& pack, class GameConnection* g );
			void recvRpcPacket( struct Packet& pack, class GameConnection* g );
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

			int  m_ConnectTimeoutMs;
			bool m_SocketIsOpened;
			bool m_SocketIsBound;
			std::vector<ConnectResultCallback>	m_ConnectResultCallbacks;
			std::vector<DisconnectCallback>		m_DisconnectCallbacks;
			std::vector<NewConnectionCallback>	m_NewConnectionCallbacks;
			std::vector<CustomDataCallback>		m_CustomDataCallbacks;
			std::vector<IConnection*> m_TempConnections;
			std::vector<IConnection*> m_DeadConnections;
		};


		template <typename List, typename Callback>
		void Motor::Anger::GameNode::bindCallback(List& list, Callback cb)
		{
			list.emplace_back( cb );
		}

		template <typename List, typename Callback>
		void Motor::Anger::GameNode::forEachCallback(List& list, Callback cb)
		{
			for (auto it = list.begin(); it != list.end(); ++it)
			{
				cb( *it );
			}
		}
	}
}