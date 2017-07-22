#pragma once

#include "Zerodelay.h"
#include "EndPoint.h"

#include <deque>
#include <map>
#include <ctime>


namespace Zerodelay
{
	/** ---------------------------------------------------------------------------------------------------------------------------------
		A VariableGroup becomes a pending group when there are no more network ID's available.
		In such case, first new ID's have to be obtained, until then, the creation of the 
		group is suspended remote, but immediately created locally. */
	struct PendingVariableGroup
	{
		static const i32_t MaxParamDataLength = 2048;
		i8_t  ParamData[MaxParamDataLength];
		i32_t ParamDataLength;
		i8_t  Channel;
		class VariableGroup* Vg;
	};


	/** ---------------------------------------------------------------------------------------------------------------------------------
		The VariableGroupNode maintains variable groups by id. Every group has a unique ID network wide.
		The VariableGroupNode deliberately knows nothing about connections. 
		Variable groups that are created from a remote machine are put in a remote map where
		the endpoint of the remote machine maps to a list of created variable groups for that machine,
		but it is just for convinience of deleting all groups from a specific endpoint. 
		No list of connections is bookkept. 
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
		virtual ~VariableGroupNode();
		void postInitialize(); // called when all ptrs to others managers are set

		void update();
		bool recvPacket(const struct Packet& pack, const class IConnection* conn);

		void beginGroup(const i8_t* paramData, i32_t paramDataLen, i8_t channel);
		void beginGroupFromRemote(u32_t nid, const ZEndpoint& ztp);
		void endGroup();
		void setIsNetworkIdProvider( bool isProvider );

	private:
		/* recvs */
		void recvIdRequest(const EndPoint& etp);
		void recvIdProvide(const Packet& pack, const EndPoint& etp);
		void recvVariableGroupCreate(const Packet& pack, const EndPoint& etp);
		void recvVariableGroupDestroy(const Packet& pack, const EndPoint& etp);
		void recvVariableGroupUpdate(const Packet& pack, const EndPoint& etp);
		/* sends */
		void sendCreateVariableGroup( u32_t networkId, const i8_t* paramData, i32_t paramDataLen, i8_t channel );
		void sendDestroyVariableGroup( u32_t networkId );
		void sendIdPackRequest();
		void sendIdPackProvide(const EndPoint& etp, i32_t numIds);
		/* flow */
		void intervalSendIdRequest();
		void resolvePendingGroups();
		void sendVariableGroups();
		/* support */
		bool deserializeGroup(const i8_t*& data, int32_t& buffLen);
		VariableGroup* findRemoteGroup( u32_t networkId, const EndPoint* etp = nullptr, bool removeOnFind = false );

		bool m_IsNetworkIdProvider; // Only 1 node is the owner of all id's, it provides id's on request.
		std::deque<u32_t> m_UniqueIds;
		std::deque<PendingVariableGroup> m_PendingGroups;
		std::map<u32_t, class VariableGroup*> m_VariableGroups; // variable groups on this machine
		std::map<EndPoint, std::map<u32_t, class VariableGroup*>, EndPoint::STLCompare> m_RemoteVariableGroups; // variable groups per connection of remote machines
		class IConnection* m_ConnOwner;
		clock_t m_LastIdPackRequestTS;
		u32_t   m_UniqueIdCounter;
		// --- ptrs to other managers
		class ZNode* m_ZNode;
		class ZNodePrivate* m_PrivZ;
	};
}