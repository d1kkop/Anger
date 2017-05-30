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


namespace Zerodelay
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
		Rpc,
		IdPackRequest,
		IdPackProvide,
		VariableGroupCreate,
		VariableGroupDestroy,
		VariableGroupUpdate,
		UserOffset
	};

	#define  USER_ID_OFFSET (unsigned char)(EGameNodePacketType::UserOffset)

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

	enum class EPacketType : unsigned char
	{
		Reliable_Ordered,
		Unreliable_Sequenced,
		Reliable_Newest,
		Ack,
		Ack_Reliable_Newest
	};


	/** ---------------------------------------------------------------------------------------------------------------------------------
		Endpoint is analogical to an address. It is either an Ipv4 or Ipv6 address.
		Do not keep a pointer to the endpoint but copy the structure instead.
		It works fine with std::map and std::unordered_map. */
	struct ZDLL_DECLSPEC ZEndpoint
	{
		ZEndpoint();


		/*	Use to see if a host can be connected to. If false is erturned, use getLastError to obtain more info. */
		bool resolve( const std::string& name, unsigned short port );


		/*	Formats to common ipv4 notation or ipv6 notation and adds port to it with a double dot in between.
			eg: 255.173.28.53:19234. */
		std::string asString() const;


		/*	Returns the last underlaying socket error. Returns 0 on no error. */
		int getLastError() const;

	private:
		char pod[256];
	};


	/** ---------------------------------------------------------------------------------------------------------------------------------
		A node is the main class for creating and maintaining connections.
		A connection is nothing more than a state machine built on top of Reliable UDP. 
		Furthermore, the Node class provides functions for sending data the easy way. */
	class ZDLL_DECLSPEC ZNode
	{
	public:
		ZNode(int sendThreadSleepTimeMs=2, int keepAliveIntervalSeconds=8, bool captureSocketErrors=true);
		virtual ~ZNode();


		/*	Connect  to specific endpoint. 
			A succesful call does not mean a connection is established.
			To know if a connection is established, bindOnConnectResult. */
		EConnectCallResult connect( const ZEndpoint& endPoint, const std::string& pw="", int timeoutSeconds=8 );
		EConnectCallResult connect( const std::string& name, int port, const std::string& pw="", int timeoutSeconds=8 );


		/*	Listen on a specific port for incoming connections.
			Bind onNewConnection to do something with the new connections. 
			[port]				The port.
			[pw]				Password string.
			[maxConnections]	Maximum number of connections. 
			[relayEvents]		If true, events such as ´disconnect´ will be relayed to other connections. */
		EListenCallResult listenOn( int port, const std::string& pw="", int maxConnections=32, bool relayEvents=true );


		/*	Disconnect a specific endpoint. */
		EDisconnectCallResult disconnect( const ZEndpoint& endPoint );


		/*	Calls disconnect on each connection in the node. 
			A node has multiple connections in case of server-client, where it is the server or in p2p.
			Individual results to disconnect() on a connection are ignored. */
		void disconnectAll();


		/*	To be called every update loop. 
			Calls all bound callback functions. */
		void update();


		/*	The password that is required to connect to this node. 
			Default is none. */
		void setPassword( const std::string& pw );


		/*	Max number of incoming connections this node will accept. 
			This excludes connections created with 'connect'. 
			Default is 32. */
		void setMaxIncomingConnections( int maxNumConnections );


		/*	If true, client events such as disconnect are relayed to other clients.
			This is particularly useful if the node should function as a 'true server'.
			The server should have relayClientEvents set to true. */
		void relayClientEvents( bool relay );


		/*	Simulate packet loss to test Quality of Service in game. 
			Precentage is value between 0 and 100. */
		void simulatePacketLoss( int percentage );


		/*	Messages are guarenteed to arrive and also in the order they were sent. This applies per channel.
			[packId]	Id of message. Should start from USER_ID_OFFSET, see above.
			[data]		Actual payload of message.
			[len]		Length of payload.
			[specific]	- If nullptr, message is sent to all connections in node.
						- If not nullptr and exclude is false, then message is only sent to the specific address.
						- If not nullptr and exclude is true, then message is sent to all except the specific.
			[channel]	On what channel to sent the message. Packets are sequenced and ordered per channel. Max of 8 channels.
			[relay]		Whether to relay the message to other connected clients when it arrives. */
		void sendReliableOrdered( unsigned char packId, const char* data, int len, const ZEndpoint* specific=nullptr, bool exclude=false, unsigned char channel=0, bool relay=true );


		/*	The last message of a certain 'dataId' is guarenteed to arrive. That is, if twice data is send with the same 'dataId' the first one may not arrive.
			[packId]	Id of message. Should start from USER_ID_OFFSET, see above.
			[data]		Actual payload of message.
			[len]		Length of payload.
			[groupId]	Identifies the data group we want to update.
			[groupBit]	Identifies the piece of data in the group we want to update. A maximum of 16 pieces can be set, so bit [0 to 15].
			[specific]	- If nullptr, message is sent to all connections in node.
			- If not nullptr and exclude is false, then message is only sent to the specific address.
			- If not nullptr and exclude is true, then message is sent to all except the specific.
			[relay]		Whether to relay the message to other connected clients when it arrives. */
		void sendReliableNewest( unsigned char packId, unsigned int groupId, char groupBit, const char* data, int len, const ZEndpoint* specific=nullptr, bool exclude=false, bool relay=true );


		/*	Messages are unreliable (they may not arrive) but older or duplicate packets are ignored. This applies per channel.
			[packId]	Id of message. Should start from USER_ID_OFFSET, see above.
			[data]		Actual payload of message.
			[len]		Length of payload.
			[specific]	- If nullptr, message is sent to all connections in node.
			- If not nullptr and exclude is false, then message is only sent to the specific address.
			- If not nullptr and exclude is true, then message is sent to all except the specific.
			[channel]	On what channel to sent the message. Packets are sequenced and ordered per channel. Max of 8 channels.
			[relay]		Whether to relay the message to other connected clients when it arrives. */
		void sendUnreliableSequenced( unsigned char packId, const char* data, int len, const ZEndpoint* specific=nullptr, bool exclude=false, unsigned char channel=0, bool relay=true );


		/*	Only one node in the network provides new network id's on request.
			Typically, this is the Server in a client-server model or the 'Super-Peer' in a p2p network. */
		void setIsNetworkIdProvider(bool isProvider);


		/*	----- Callbacks ----------------------------------------------------------------------------------------------- */

		/*	For handling connect request results. */
		void bindOnConnectResult( std::function<void (const ZEndpoint&, EConnectResult)> cb );


		/*	For handling new incoming connections. 
			In a client-server achitecture, new clients are also relayed to existing clients from the server. */
		void bindOnNewConnection( std::function<void (const ZEndpoint&)> cb );


		/*	For when connection is closed or gets dropped.
			In a client-server architecture, disconnects are releyed so that all clients from the server. */
		void bindOnDisconnect( std::function<void (bool isThisConnection, const ZEndpoint&, EDisconnectReason)> cb );


		/*	For all other data that is specific to the application. 
			The 'length' parameter is the size of the 'data' parameter. */
		void bindOnCustomData( std::function<void (const ZEndpoint&, unsigned char id, const char* data, int length, unsigned char channel)> cb );


		/*	----- End Callbacks ----------------------------------------------------------------------------------------------- */


		/*	Custom ptr to provide a way to come from a 'global' variable group or rpc functions to application code. */
		void  setUserDataPtr( void* ptr );
		void* getUserDataPtr() const;


		/*	Custom handle to provide a way to come from a 'global' variable group or rpc functions to application code. */
		void setUserDataIdx( int idx );
		int  gtUserDataIdx() const;


		/*	Begin constructing a new variable group.
			All GenericNetVar declared between this call end EndVariable group are 
			put in a group with a unique ID.
			The variables synchronize when they change. */
		void beginVariableGroup( const char* constructData=nullptr, int constructDataLen=0, char channel=1, EPacketType syncType=EPacketType::Unreliable_Sequenced );
		void endVariableGroup();


	private:
		class ConnectionNode* p;
		class VariableGroupNode* vgn;
		class ZNodePrivate* zp;
	};



	/** ---------------------------------------------------------------------------------------------------------------------------------
	/*	FOR PRIVATE USE, DO NOT USE! */
	class ZDLL_DECLSPEC ZNodePrivate
	{
		friend class ZNode;

	public:
		void priv_beginVarialbeGroupRemote(unsigned int nid, const ZEndpoint& ztp, EPacketType type);
		void priv_endVariableGroup();
		ZNode* priv_getUserNode() const;

	private:
		ZNode* m_ZNode;
		VariableGroupNode* vgn;
	};
}