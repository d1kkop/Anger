#pragma once

#include "Zerodelay.h"
#include "EndPoint.h"
#include "Util.h"

#include <deque>
#include <map>
#include <ctime>


namespace Zerodelay
{
	using GroupCallback = std::function<void (const EndPoint*, u32_t)>;


	/** ---------------------------------------------------------------------------------------------------------------------------------
		A VariableGroup becomes a pending group when there are no more network ID's available.
		In such case, first new ID's have to be obtained, until then, the creation of the 
		group is suspended remote, but immediately created locally. */
	struct VariableGroupCreateData
	{
		static const i32_t MaxParamDataLength = 1024;
		i8_t  ParamData[MaxParamDataLength];
		i32_t ParamDataLength;
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
		void sendCreateVariableGroup( u32_t networkId, const i8_t* paramData, i32_t paramDataLen );
		void sendDestroyVariableGroup( u32_t networkId );
		void sendIdPackProvide(const EndPoint& etp, i32_t numIds);
		/* flow */
		void intervalSendIdRequest();
		void resolvePendingGroups();
		void sendUpdatedVariableGroups();
		void sendAllVariableCreateEventsTo(const ZEndpoint& to);
		/* support */
		void callCreateVariableGroup(i8_t* data, i32_t len, bool remote, const ZEndpoint* ztp);
		bool deserializeGroup(const i8_t*& data, i32_t& buffLen);
		class VariableGroup* findOrRemoveBrokenGroup( u32_t networkId, const EndPoint* etp = nullptr );
		// group callbacks
		void bindOnGroupUpdated(const GroupCallback& cb)				{ Util::bindCallback(m_GroupUpdateCallbacks, cb); }
		void bindOnGroupDestroyed(const GroupCallback& cb)				{ Util::bindCallback(m_GroupDestroyCallbacks, cb); }


		bool m_IsNetworkIdProvider; // Only 1 node is the owner of all id's, it provides id's on request.
		std::deque<u32_t> m_UniqueIds;
		std::deque<VariableGroupCreateData> m_PendingGroups;		// When creating a new one, it first becomes pending until network Id's are available.
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