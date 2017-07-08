#pragma once

#include "Zerodelay.h"
#include "EndPoint.h"

#include <deque>
#include <map>
#include <ctime>


namespace Zerodelay
{
	struct PendingVariableGroup
	{
		static const i32_t MaxParamDataLength = 2048;
		i8_t ParamData[MaxParamDataLength];
		i32_t ParamDataLength;
		class VariableGroup* Vg;
	};


	class VariableGroupNode
	{
		friend class ZNode;

		static const i32_t sm_AvailableIds = 25;

	public:
		VariableGroupNode();
		virtual ~VariableGroupNode();

		void update();
		bool recvPacket(const struct Packet& pack, const class IConnection* conn);

		void beginGroup(const i8_t* paramData, i32_t paramDataLen, i8_t channel, EPacketType type);
		void beginGroupFromRemote(u32_t nid, const ZEndpoint& ztp, EPacketType type);
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
		void sendCreateVariableGroup( u32_t networkId, const i8_t* paramData, i32_t paramDataLen );
		void sendDestroyVariableGroup( u32_t networkId );
		void sendIdPackRequest();
		void sendIdPackProvide(const EndPoint& etp, i32_t numIds);
		/* flow */
		void intervalSendIdRequest();
		void resolvePendingGroups();
		void sendVariableGroups();

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