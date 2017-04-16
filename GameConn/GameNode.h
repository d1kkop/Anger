#pragma once

#include "RecvPoint.h"

#include <cassert>
#include <algorithm>
#include <vector>
#include <functional>
#include <memory>


//#define RPC_UNPACK_3( name, a, b, c ) \
//	void sync_##name( const char* data, int len ) \
//	{ \
//		struct temp { \
//			a _a; \
//			b _b; \
//			c _c; \	
//		}; \
//		name( temp._a, temp._b, temp._c ); \
//	} \
//	void name(  a _a, b _b, c _c )
//
//
//RPC_UNPACK_3( name, int, char, bool );
//{
//
//}


namespace Motor
{
	namespace Anger
	{
		enum class EGameNodePacketType: unsigned char
		{
			ConnectRequest,
			ConnectAccept,
			KeepAliveRequest,
			KeepAliveAnswer
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

		enum class EConnectResult
		{
			Succes,
			Timedout
		};

		enum class EDisconnectReason
		{
			Closed,
			Lost
		};

		class GameNode: public RecvPoint
		{
			typedef std::function<void (class GameConnection*, EConnectResult)>		ConnectResultCallback;
			typedef std::function<void (class GameConnection*, EDisconnectReason)>	DisconnectCallback;
			typedef std::function<void (class GameConnection*)>						NewConnectionCallback;
			typedef std::function<void (class GameConnection*, unsigned char, const char*, int, unsigned char)>	CustomDataCallback;

		public:
			GameNode(int connectTimeoutSeconds=12, int sendThreadSleepTimeMs=2, bool captureSocketErrors=true);
			virtual ~GameNode();

		private:
			// Called by recv thread
			virtual class IConnection* createNewConnection( const EndPoint& endPoint ) const override;

		public:
			template<class... Args>
			void rpc( unsigned char id, const EndPoint* specific, bool exclude, char channel, Args... args );

			EConnectCallResult connect( const EndPoint& endPoint );
			EConnectCallResult connect( const std::string& name, int port );
			EListenCallResult listenOn( int port );
			void update();

			// For handling connect request results
			// Function signature: void (class GameConnection* conn, EConnectResult conResult)
			void bindOnConnectResult(ConnectResultCallback cb)		{ bindCallback(m_ConnectResultCallbacks, cb); }
			// For handling new incoming connections
			// Function signature: void (class GameConnection* conn)
			void bindOnNewConnection(NewConnectionCallback cb)		{ bindCallback(m_NewConnectionCallbacks, cb); }
			// For when connection is closed or gets dropped
			// Function signature: void (class GameConnection* conn, EDisconnect reason)
			void bindOnDisconnect(DisconnectCallback cb)			{ bindCallback(m_DisconnectCallbacks, cb); }
			// For all other data that is specific to the application
			void bindOnCustomData(CustomDataCallback cb)			{ bindCallback(m_CustomDataCallbacks, cb); }

		private:
			// Game thread
			void handlePacket( struct Packet& pack, class GameConnection* g );
			void updateConnecting( class GameConnection* g );
			void updateKeepAlive( class GameConnection* g );
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

	#pragma pack(push, 1) 
		template<class ...Args> struct value_holder
		{
			unsigned char rpcId;
			template<Args... Values> // expands to a non-type template parameter 
			struct apply { };     // list, such as <int, char, int(&)[5]>
		};
	#pragma pack(pop)

		template<class ...Args>
		void Motor::Anger::GameNode::rpc(unsigned char id, const EndPoint* specific, bool exclude, char channel, Args... args)
		{
			value_holder<Args...> vh; 
			vh.rpcId = id;
			int kSize = sizeof(vh);
			this->beginSend( specific, exclude );
			this->send( (const char*)&vh, sizeof(vh), EPacketType::Reliable_Ordered, channel );
			this->endSend();
		}

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