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


#include <map>
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
		AlreadyConnected,
		InvalidConnectPacket,
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
		AlreadyExists,
		TooMuchAdditionalData,
		SocketError
	};

	enum class EListenCallResult
	{
		Succes,
		AlreadyStartedServer,
		SocketError
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
		MaxConnectionsReached,
		AlreadyConnected,
		InvalidConnectPacket
	};

	enum class EDisconnectReason : u8_t
	{
		Closed,
		Lost
	};


	/** ---------------------------------------------------------------------------------------------------------------------------------
		ZEndpoint is analogical to an address. It is either an Ipv4 or Ipv6 address.
		Do not keep a pointer to the endpoint but copy the structure instead.
		It works out of the box with std::map */
	struct ZDLL_DECLSPEC ZEndpoint
	{
		ZEndpoint();
		ZEndpoint( const std::string& name, u16_t port );


		bool operator==(const ZEndpoint& other) const;
		bool operator!=(const ZEndpoint& other) const { return !(*this == other); }


		struct STLCompare
		{
			bool operator() (const ZEndpoint& left, const ZEndpoint& right) const;
		};


		/*	Use to see if a host can be connected to. If false is erturned, use getLastError to obtain more info. */
		bool resolve( const std::string& name, u16_t port );


		/*	Returns canonical form of endpoint, eg: 73.123.24.18:12203 */
		std::string toIpAndPort() const;


		/*	Returns the last underlaying socket error. Returns 0 on no error. */
		i32_t getLastError() const;


		/* Returns true if instantiated but not yet resolved to any endpoint. */
		bool isZero() const;

	private:
		i8_t d[32];
	};


	/** ---------------------------------------------------------------------------------------------------------------------------------
		A node contains the network state of all connections and variables that
		are tied to higher level objects.*/
	class ZDLL_DECLSPEC ZNode
	{
	public:
		/*[resendIntervalMs]			Is the time to wait before retransmitting (reliable) packets in the send queue.
										New packets are always sent immediately.
		  [keepAliveIntervalSeconds]	Is the time between consequative requests to keep the connection alive. */
		ZNode( i32_t resendIntervalMs=50, i32_t keepAliveIntervalSeconds=8 );
		virtual ~ZNode();


		/*	Connect  to specific endpoint. 
			A succesful call does not mean a connection is established.
			To know if a connection is established, bindOnConnectResult. */
		EConnectCallResult connect( const ZEndpoint& endPoint, const std::string& pw="", i32_t timeoutSeconds=8, const std::map<std::string, std::string>& additionalData=std::map<std::string, std::string>(), bool sendConnectRequest=true );
		EConnectCallResult connect( const std::string& name, i32_t port, const std::string& pw="", i32_t timeoutSeconds=8, const std::map<std::string, std::string>& additionalData=std::map<std::string, std::string>(), bool sendConnectRequest=true );


		/*	Calls disconnect on each connection in the node and stops listening for incoming connections. 
			[lingerTimeMs]	Is time in millisecond to wait so that disconnect packets have a chance to dispatch. */
		void disconnect(u32_t lingerTimeMs = 300);


		/*	Disconnect a specific endpoint. */
		EDisconnectCallResult disconnect( const ZEndpoint& endPoint );


		/*	Host a new session. For client-server, only one peer should call host while the others should connect.
			For p2p, every peer should call host as well as connect.
			Bind onNewConnection to do something with the new connections. 
			[port]				The port in native machine order.
			[pw]				Password string.
			[maxConnections]	Maximum number of connections. 
			[relayEvents]		If true, events such as ´disconnect´ will be relayed to other connections. If not client-server architecture, the parameter is ignored. */
		EListenCallResult listen( i32_t port, const std::string& pw="", i32_t maxConnections=32 );


		/*	Returns the number of connected connections. Connections that are
			in connected state or disconnected are not counted. */
		i32_t getNumOpenConnections() const;


		/*	Fills the vector with all connected connections. Connection in connecting or disconnected state are not copied into the list. */
		void getConnectionListCopy( std::vector<ZEndpoint>& listOut );


		/*	Returns number of open links. A link is a connection in any state, not only 'Connected'.
			To obtain the number of links that are actually 'Connected' use: 'getNumOpenConnections()'.
			A link can be in many states such as a 'pending delete' or a 'timed out connection attempt'.  */
		i32_t getNumOpenLinks() const;


		/*	Returns true if the connection is in connected state only. Connecting or disconnected
			connections will return false. */
		bool isConnectedTo( const ZEndpoint& ztp ) const;


		/*	Returns true if a connection to this endpoint is known.
			This information is useful if you want to reconnect to the same service just after disconecting.
			When you disconnect and immediately try to reconnect it will fail because the
			previous connection to the same endpoint is still lingering to handle disconnect messages.
			To make sure a previous connection is completely forgotten call this function. */
		bool isConnectionKnown( const ZEndpoint& ztp ) const;


		/*	Returns true if there is at least one connection that has pending data to be processed. */
		bool hasPendingData() const;


		/*	This will invoke the bound callback functions when new data is available. 
			Usually called every frame update in a game. */
		void update();


		/*	The password that is required to connect to this node. 
			Default is empty string. */
		void setPassword( const std::string& pw );


		/*	Max number of incoming connections this node will accept. 
			Default is 32. */
		void setMaxIncomingConnections( i32_t maxNumConnections );


		/*	Returns ture if is server in client-server architecture or if is the authorative peer in a p2p network. */
		bool isAuthorative() const;


		/*	Simulate packet loss to test Quality of Service in game. 
			Precentage is value between 0 and 100. */
		void simulatePacketLoss( i32_t percentage );


		/*	Messages are guarenteed to arrive and also in the order they were sent. This applies per channel.
			[packId]	Id of message. Should start from USER_ID_OFFSET, see above.
			[data]		Actual payload of message.
			[len]		Length of payload (bytes).
			[specific]	- If nullptr, message is sent to all connections in node.
						- If not nullptr and exclude is false, then message is only sent to the specific address.
						- If not nullptr and exclude is true, then message is sent to all except the specific.
			[channel]	On what channel to sent the message. Packets are sequenced and ordered per channel. Max of 8 channels, 0 to 7.
			[relay]		Whether to relay the message to other connected clients when it arrives. 
			[requiresConnection] If false, the packet is sent regardless of whether the endpoint(s) are in connected state. Default true. */
		void sendReliableOrdered( u8_t packId, const i8_t* data, i32_t len, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true, bool requiresConnection=true );


		/*	The last message of a certain 'dataId' is guarenteed to arrive. That is, if twice data is sent with the same 'dataId' the first one may not arrive.
			[packId]	Id of message. Should start from USER_ID_OFFSET, see above.
			[data]		Actual payload of message.
			[len]		Length of payload.
			[groupId]	Identifies the data group we want to update.
			[groupBit]	Identifies the piece of data in the group we want to update. A maximum of 16 pieces can be set, bit [0 to 15].
			[specific]	- If nullptr, message is sent to all connections in node.
						- If not nullptr and exclude is false, then message is only sent to the specific address.
						- If not nullptr and exclude is true, then message is sent to all except the specific. 
			[requiresConnection] If false, the packet is sent regardless of whether the endpoint(s) are in connected state. Default true. */
		void sendReliableNewest( u8_t packId, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const ZEndpoint* specific=nullptr, bool exclude=false, bool requiresConnection=true );


		/*	Messages are unreliable sequenced. This means that packets may not arrive. However, older or duplicate packets are dropped. This applies per channel.
			[packId]	Id of message. Should start from USER_ID_OFFSET, see above.
			[data]		Actual payload of message.
			[len]		Length of payload (bytes).
			[specific]	- If nullptr, message is sent to all connections in node.
						- If not nullptr and exclude is false, then message is only sent to the specific address.
						- If not nullptr and exclude is true, then message is sent to all except the specific.
			[channel]	On what channel to sent the message. Packets are sequenced and ordered per channel. Max of 8 channels, 0 to 7.
			[relay]		Whether to relay the message to other connected clients when it arrives. 
			[requiresConnection] If false, the packet is sent regardless of whether the endpoint(s) are in connected state. Default is true. */
		void sendUnreliableSequenced( u8_t packId, const i8_t* data, i32_t len, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true, bool requiresConnection=true );

		/*	----- Callbacks ----------------------------------------------------------------------------------------------- */

		/*	For handling connect request results. This callback is invoked as a result of calling 'connect'. */
		void bindOnConnectResult( const std::function<void (const ZEndpoint&, EConnectResult)>& cb );


		/*	This event occurs when a remote endpoint called 'connect'.
			[directLink]	If true, a connection attempt was made to this Znode. 
							If false, a non directive link such as a remote connection that connected to the server and
							the endpoint will not be found when obtaining a list of open connections through getNumOpenConnections(). 
			[Zendpoint]		The endpoint, either of our direct link or relayed by eg. a server. */
		void bindOnNewConnection( const std::function<void (bool directLink, const ZEndpoint&, const std::map<std::string, std::string>&)>& cb );


		/*	This event occurs when we or a remote endpoint called 'disconnect'.
			[directLink]	If true, one of the endpoints that this Znode connected to disconnected to us our we disconnected to them.
							If false, the message is relayed by an authorative node such as the server and
							the endpoint will not be found when obtaining a list of open connections through getNumOpenConnections(). 
			[Zendpoint]		The endpoint, either of our direct link or relayed by eg. the server. */
		void bindOnDisconnect( const std::function<void (bool directLink, const ZEndpoint&, EDisconnectReason)>& cb );


		/*	For all application specific data that was transmitted with an id >= USER_ID_OFFSET. */
		void bindOnCustomData( const std::function<void (const ZEndpoint&, u8_t id, const i8_t* data, i32_t length, u8_t channel)>& cb );


		/*	If at least a single variable inside the group is updated, this callback is invoked.
			Note that varialbes can also have custom callbacks that are called per variable. 
			If called locally, ZEndpoint is a nullptr. */
		void bindOnGroupUpdated( const std::function<void (const ZEndpoint*, u8_t id)>& cb );


		/*	Called when group gets destroyed. If called locally, ZEndpoint is a nullptr. */
		void bindOnGroupDestroyed( const std::function<void (const ZEndpoint*, u8_t id)>& cb );


		/*	----- End Callbacks ----------------------------------------------------------------------------------------------- */


		/*	Custom ptr to provide a way to get from a 'global' variable group or rpc functions to application code. */
		void  setUserDataPtr( void* ptr );
		void* getUserDataPtr() const;


		/*	Deferred create variable group from serialized data.
			Causes 'createGroup_xxx' to be called when a network Id is available.
			Usually this function is only used by the system internally. 
			Use createGroup directly instead. */
		void deferredCreateVariableGroup( const i8_t* constructData=nullptr, i32_t constructDataLen=0, i8_t channel=1 );


	private:
		class CoreNode* C;
	};
}