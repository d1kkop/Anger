#pragma once

#include "Zerodelay.h"
#include "EndPoint.h"
#include "RpcMacros.h"
#include "Util.h"

#include <deque>
#include <map>
#include <ctime>


namespace Zerodelay
{
	class BinSerializer;

	using GroupCallback = std::function<void (const EndPoint*, u32_t)>;


	/** ---------------------------------------------------------------------------------------------------------------------------------
		A VariableGroup becomes a pending group when there are no more network ID's available.
		In such case, first new ID's have to be obtained, until then, the creation of the 
		group is suspended remote, but immediately created locally. */
	struct GroupPendingData
	{
		i8_t  Data[RPC_DATA_MAX];
		i32_t TotalDataLength;

		const i8_t* funcName() const  { return Data; }
		const i8_t* paramData() const { return Data + nameLen()+1; }
		i32_t nameLen() const { return (i32_t)strlen(Data); }
		i32_t paramDataLen() const { return TotalDataLength-(nameLen()+1); }
	};


	/*	Group Create data already serialized such that it can be send through to anyone. */
	struct GroupCreateData
	{
		i8_t* Data;
		i32_t Len;
		u32_t netId;
	};


	/** ---------------------------------------------------------------------------------------------------------------------------------
		The VariableGroupNode maintains variable groups by id. Every group has a unique ID network wide.
		The VariableGroupNode deliberately knows nothing about connections. 
		Variable groups that are created from a remote endpoint are put in a remote map where
		the remote endpoint maps to a list of created variable-groups for that endpoint.
		There should always only be one ID provider in the network, however because the VariableGroupNode
		does not know who the provider is, the message is just sent to all connections every so often
		until it gets a reply with a list of free available networkID's. Before it runs out of ID's, it will
		restart sending network ID requests to everyone. */
	class VariableGroupNode
	{
		friend class ZNode;

		static const i32_t sm_AvailableIds = 25;

	public:
		VariableGroupNode();
		~VariableGroupNode();
		void reset(bool isConstructorCall);

		void postInitialize(class CoreNode* coreNode);
		void setupConnectionCallbacks();

		void update();
		bool processPacket(const struct Packet& pack, const EndPoint& etp);

		void deferredCreateGroup(const i8_t* paramData, i32_t paramDataLen);
		void beginNewGroup(u32_t nid, const ZEndpoint* ztp);
		void endNewGroup();
		void setIsNetworkIdProvider( bool isProvider );

	private:
		/* recvs */
		void recvIdRequest(const EndPoint& etp);
		void recvIdProvide(const Packet& pack, const EndPoint& etp);
		void recvVariableGroupCreate(const Packet& pack, const EndPoint& etp);
		void recvVariableGroupDestroy(const Packet& pack, const EndPoint& etp);
		void recvVariableGroupUpdate(const Packet& pack, const EndPoint& etp);
		/* sends */
		void sendCreateVariableGroup( BinSerializer& bs, const ZEndpoint* target );
		void sendCreateVariableGroup( const i8_t* funcName, const i8_t* paramData, i32_t paramDataLen, u32_t netId, const ZEndpoint* owner );
		void sendDestroyVariableGroup( u32_t networkId );
		void sendIdPackProvide(const EndPoint& etp, i32_t numIds);
		/* flow */
		void intervalSendIdRequest();
		void resolvePendingGroups();
		void sendUpdatedVariableGroups();
		void sendAllVariableCreateEventsTo(const ZEndpoint& to);
		/* support */
		void callCreateVariableGroup(const i8_t* name, u32_t id, const i8_t* paramData, i32_t paramDataLen, const ZEndpoint* ztp);
		void bufferCreateVariableGroup(const i8_t* name, u32_t id, const i8_t* paramData, i32_t paramDataLen, const ZEndpoint* ztp);
		bool deserializeGroup(const i8_t*& data, i32_t& buffLen);
		void unBufferGroup(u32_t netId);
		class VariableGroup* findOrRemoveBrokenGroup( u32_t networkId, const EndPoint* etp = nullptr );
		// group callbacks
		void bindOnGroupUpdated(const GroupCallback& cb)				{ Util::bindCallback(m_GroupUpdateCallbacks, cb); }
		void bindOnGroupDestroyed(const GroupCallback& cb)				{ Util::bindCallback(m_GroupDestroyCallbacks, cb); }


		bool m_IsNetworkIdProvider; // Only 1 node is the owner of all id's, it provides id's on request.
		std::deque<u32_t> m_UniqueIds;
		std::deque<GroupPendingData> m_PendingGroups;				// When creating a new one, it first becomes pending until network Id's are available.
		std::vector<GroupCreateData> m_BufferedGroups;				// When created, keep list so that new incoming connections can get the till then created buffered list of variable groups.
		std::map<u32_t, class VariableGroup*> m_VariableGroups;		// Variable groups on this local endpoint.
		std::map<EndPoint, std::map<u32_t, class VariableGroup*>, EndPoint::STLCompare> m_RemoteVariableGroups; // variable groups per connection of remote machines
		clock_t m_LastIdPackRequestTS;
		u32_t   m_UniqueIdCounter;
		std::vector<GroupCallback> m_GroupUpdateCallbacks;
		std::vector<GroupCallback> m_GroupDestroyCallbacks;
		// --- ptrs to other managers
		class CoreNode* m_CoreNode;
		class ZNode* m_ZNode;
		class ConnectionNode* m_ConnectionNode;
	};
}