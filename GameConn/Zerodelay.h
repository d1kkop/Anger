#pragma once


#ifdef _WIN32
	#ifdef ZDLL_EXPORTING
		#define ZDLL_DECLSPEC __declspec(dllexport)
	#else
		#define ZDLL_DECLSPEC __declspec(dllimport)
	#endif
#else
	#define ZDLL_DECLSPEC
#endif


#include <functional>
#include <string>
#include <vector>


namespace Zerodelay
{
	using i8_t  = char;
	using u8_t  = unsigned char;
	using i16_t = short;
	using u16_t = unsigned short;
	using i32_t = int;
	using u32_t = unsigned int;

	enum class EDataPacketType: u8_t
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
		Rpc,
		IdPackRequest,
		IdPackProvide,
		VariableGroupCreate,
		VariableGroupDestroy,
		VariableGroupUpdate,
		UserOffset
	};

	#define  USER_ID_OFFSET (u8_t)(EDataPacketType::UserOffset)

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
		SocketError,
		AlreadyStartedServer
	};

	enum class EDisconnectCallResult
	{
		Succes,
		NotConnected,
		UnknownEndpoint
	};

	enum class EConnectResult
	{
		Succes,
		Timedout,
		InvalidPassword,
		MaxConnectionsReached
	};

	enum class EDisconnectReason : u8_t
	{
		Closed,
		Lost
	};

	// Not be confused with EDataPacketType
	enum class EHeaderPacketType : u8_t
	{
		Reliable_Ordered,
		Unreliable_Sequenced,
		Reliable_Newest,
		Ack,
		Ack_Reliable_Newest
	};


	enum class ERoutingMethod 
	{
		ClientServer,
		Peer2Peer
	};


	/** ---------------------------------------------------------------------------------------------------------------------------------
		Endpoint is analogical to an address. It is either an Ipv4 or Ipv6 address.
		Do not keep a pointer to the endpoint but copy the structure instead.
		It works out of the box with std::map and std::unordered_map. */
	struct ZDLL_DECLSPEC ZEndpoint
	{
		ZEndpoint();


		/*	Use to see if a host can be connected to. If false is erturned, use getLastError to obtain more info. */
		bool resolve( const std::string& name, u16_t port );


		/*	Returns canonical form of endpoint, eg: 73.123.24,18. */
		std::string asString() const;


		/*	Returns the last underlaying socket error. Returns 0 on no error. */
		i32_t getLastError() const;


	private:
		i8_t pod[256];
	};


	/** ---------------------------------------------------------------------------------------------------------------------------------
		A node is the main class for creating and maintaining connections.
		A connection is nothing more than a state machine built on top of Reliable UDP. 
		Furthermore, the Node class provides functions for sending data the easy way. */
	class ZDLL_DECLSPEC ZNode
	{
	public:
		/*	If routingMethod is set to ClientServer, only one ZNode should call 'listenOn', all clients should call: 'connect'.
			In case of Peer2Peer, clients first connect to a server (channel) from which the server will set up the 
			peer to peer connections. */
		ZNode(ERoutingMethod routingMethod = ERoutingMethod::ClientServer,
			  i32_t sendThreadSleepTimeMs=20, i32_t keepAliveIntervalSeconds=8, bool captureSocketErrors=true);
		virtual ~ZNode();


		/*	Connect  to specific endpoint. 
			A succesful call does not mean a connection is established.
			To know if a connection is established, bindOnConnectResult. 
			Port must be in native machine order. No conversion to network order necessary. */
		EConnectCallResult connect( const ZEndpoint& endPoint, const std::string& pw="", i32_t timeoutSeconds=8 );
		EConnectCallResult connect( const std::string& name, i32_t port, const std::string& pw="", i32_t timeoutSeconds=8 );


		/*	Listen on a specific port for incoming connections.
			Bind onNewConnection to do something with the new connections. 
			[port]				The port in native machine order.
			[pw]				Password string.
			[maxConnections]	Maximum number of connections. 
			[relayEvents]		If true, events such as ´disconnect´ will be relayed to other connections. If not client-server architecture, the parameter is ignored. */
		EListenCallResult listenOn( i32_t port, const std::string& pw="", i32_t maxConnections=32, bool relayEvents=true );


		/*	Disconnect a specific endpoint. */
		EDisconnectCallResult disconnect( const ZEndpoint& endPoint );


		/*	Calls disconnect on each connection in the node. 
			A node has multiple connections in case of server-client, where it is the server or in p2p.
			Individual results to disconnect() on a connection are ignored. */
		void disconnectAll();


		/*	Returns the number of connections that are not in a connecting or disconnecting state,
			that is, connections that are fully operatable. 
			If only a single connection is used to connect to a server, this function
			can be queried to see if the connection attempt was succesful.
			However, the recommended way is to bind a callback to: 'bindOnConnectResult', see below. */
		i32_t getNumOpenConnections() const;


		/*	This will invoke the bound callback functions when new data is available. */
		void update();


		/*	The password that is required to connect to this node. 
			Default is none. */
		void setPassword( const std::string& pw );


		/*	Max number of incoming connections this node will accept. 
			This excludes connections created with 'connect'. 
			Default is 32. */
		void setMaxIncomingConnections( i32_t maxNumConnections );


		/*	Fills the vector with all fully connected connections, that is all connections which are not in an any other state than
			connected. Therefore, connections in connecting state or disconnecting state are discareded. */
		void getConnectionListCopy( std::vector<ZEndpoint>& listOut );


		/*	Returns initially specified routing method. */
		ERoutingMethod getRoutingMethod() const;


		/*	Simulate packet loss to test Quality of Service in game. 
			Precentage is value between 0 and 100. */
		void simulatePacketLoss( i32_t percentage );


		/*	Messages are guarenteed to arrive and also in the order they were sent. This applies per channel.
			[packId]	Id of message. Should start from USER_ID_OFFSET, see above.
			[data]		Actual payload of message.
			[len]		Length of payload.
			[specific]	- If nullptr, message is sent to all connections in node.
						- If not nullptr and exclude is false, then message is only sent to the specific address.
						- If not nullptr and exclude is true, then message is sent to all except the specific.
			[channel]	On what channel to sent the message. Packets are sequenced and ordered per channel. Max of 8 channels.
			[relay]		Whether to relay the message to other connected clients when it arrives. 
						Returns true if was added to a dispatch queue, false otherwise. */
		bool sendReliableOrdered( u8_t packId, const i8_t* data, i32_t len, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true );


		/*	The last message of a certain 'dataId' is guarenteed to arrive. That is, if twice data is send with the same 'dataId' the first one may not arrive.
			[packId]	Id of message. Should start from USER_ID_OFFSET, see above.
			[data]		Actual payload of message.
			[len]		Length of payload.
			[groupId]	Identifies the data group we want to update.
			[groupBit]	Identifies the piece of data in the group we want to update. A maximum of 16 pieces can be set, so bit [0 to 15].
			[specific]	- If nullptr, message is sent to all connections in node.
			- If not nullptr and exclude is false, then message is only sent to the specific address.
			- If not nullptr and exclude is true, then message is sent to all except the specific. */
		void sendReliableNewest( u8_t packId, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const ZEndpoint* specific=nullptr, bool exclude=false );


		/*	Messages are unreliable (they may not arrive) but older or duplicate packets are ignored. This applies per channel.
			[packId]	Id of message. Should start from USER_ID_OFFSET, see above.
			[data]		Actual payload of message.
			[len]		Length of payload.
			[specific]	- If nullptr, message is sent to all connections in node.
						- If not nullptr and exclude is false, then message is only sent to the specific address.
						- If not nullptr and exclude is true, then message is sent to all except the specific.
			[channel]	On what channel to sent the message. Packets are sequenced and ordered per channel. Max of 8 channels.
			[relay]		Whether to relay the message to other connected clients when it arrives. */
		void sendUnreliableSequenced( u8_t packId, const i8_t* data, i32_t len, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true, bool discardSendIfNotConnected=true );


		/*	Only one node in the network provides new network id's on request.
			Typically, this is the Server in a client-server model or the 'Super-Peer' in a p2p network. */
		void setIsNetworkIdProvider( bool isProvider );


		/*	----- Callbacks ----------------------------------------------------------------------------------------------- */

		/*	For handling connect request results. This callback is invoked as a result of calling 'connect'. */
		void bindOnConnectResult( const std::function<void (const ZEndpoint&, EConnectResult)>& cb );


		/*	For handling new incoming connections. 
			In a client-server achitecture, this event is relayed to all other clients by the server. */
		void bindOnNewConnection( const std::function<void (const ZEndpoint&)>& cb );


		/*	For when connection is closed or gets dropped.
			In a client-server architecture, disconnect events are relayed by the server. 
			[isThisConnection]	Is true, if a client-server architecture and server closed connection or if
								is p2p connection and one of the peers closed connection.
								All other cases false, eg. if a remote client disconnected in a client-server architecture. */
		void bindOnDisconnect( const std::function<void (bool isThisConnection, const ZEndpoint&, EDisconnectReason)>& cb );


		/*	For all other data that is specific to the application. */
		void bindOnCustomData( const std::function<void (const ZEndpoint&, u8_t id, const i8_t* data, i32_t length, u8_t channel)>& cb );


		/*	If at least a single variable inside the group is updated, this callback is invoked.
			Note that varialbes can also have custom callbacks that are called per variable. 
			If called locally, ZEndpoint is a null ptr. */
		void bindOnGroupUpdated( const std::function<void (const ZEndpoint*, u8_t id)>& cb );


		/*	Called when group gets destroyed. If called locally, ZEndpoint is a null ptr. */
		void bindOnGroupDestroyed( const std::function<void (const ZEndpoint*, u8_t id)>& cb );


		/*	----- End Callbacks ----------------------------------------------------------------------------------------------- */


		/*	Custom ptr to provide a way to get from a 'global' variable group or rpc functions to application code. */
		void  setUserDataPtr( void* ptr );
		void* getUserDataPtr() const;


		/*	Custom handle to provide a way to get from a 'global' variable group or rpc functions to application code. */
		void setUserDataIdx( i32_t idx );
		i32_t  gtUserDataIdx() const;


		/*	Deferred create variable group from serialized data.
			Causes 'createGroup_xxx' to be called when a network Id is available.
			Usually this function is only used by the system internally. 
			Use createGroup directly instead. */
		void deferredCreateVariableGroup( const i8_t* constructData=nullptr, i32_t constructDataLen=0, i8_t channel=1 );


	private:
		class ConnectionNode* p;
		class VariableGroupNode* vgn;

		friend class VariableGroupNode;
	};
}