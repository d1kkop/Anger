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
		static const int MaxParamDataLength = 2048;
		char ParamData[MaxParamDataLength];
		int ParamDataLength;
		class VariableGroup* Vg;
	};


	class VariableGroupNode
	{
		friend class ZNode;

		static const int sm_AvailableIds = 25;

	public:
		VariableGroupNode();
		virtual ~VariableGroupNode();

		void update();
		bool recvPacket(const struct Packet& pack, const class IConnection* conn);

		void beginGroup(const char* paramData, int paramDataLen, char channel);
		void beginGroupFromRemote();
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
		void sendCreateVariableGroup( unsigned int networkId, const char* paramData, int paramDataLen );
		void sendDestroyVariableGroup( unsigned int networkId );
		/* flow */
		void checkAndsendNewIdsRequest();
		void resolvePendingGroups();
		void sendVariableGroups();
		//void syncVariablesThread();
		/* helper */
		VariableGroup* findRemoteGroup( unsigned int networkId, const EndPoint* etp = nullptr, bool removeOnFind = false );

		bool m_IsNetworkIdProvider; // Only 1 node is the owner of all id's, it provides id's on request.
		std::deque<unsigned int> m_UniqueIds;
		std::deque<PendingVariableGroup> m_PendingGroups;
		std::map<unsigned int, class VariableGroup*> m_VariableGroups;
		std::map<EndPoint, std::map<unsigned int, class VariableGroup*>, EndPoint::STLCompare> m_RemoteVariableGroups;
		class IConnection* m_ConnOwner;
		clock_t m_LastIdPackRequestTS;
		unsigned char m_UniqueIdCounter;
		// --- ptrs to other managers
		class ZNode* m_ZNode;
		class ZNodePrivate* m_PrivZ;
	};
}