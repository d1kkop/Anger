#pragma once

#include "Zerodelay.h"
#include "EndPoint.h"

#include <deque>
#include <map>
#include <ctime>
//#include <thread>
//#include <mutex>


namespace Zerodelay
{
	class VariableGroupNode
	{
		friend class ZNode;

		static const int sm_AvailableIds = 25;

	public:
		VariableGroupNode();
		virtual ~VariableGroupNode();

		void update();
		bool recvPacket(const struct Packet& pack, const class IConnection* conn);

		void beginGroup(char channel);
		void endGroup();
		void setIsNetworkIdProvider( bool isProvider );

	private:
		/* recvs */
		void recvIdRequest(const EndPoint& etp);
		void recvIdProvide(const Packet& pack, const EndPoint& etp);
		void recvVariableGroup(const Packet& pack, const EndPoint& etp);
		/* flow */
		void checkAndsendNewIdsRequest();
		void resolvePendingGroups();
		void sendVariableGroups();
		
		//void syncVariablesThread();

		bool m_IsNetworkIdProvider; // Only 1 node is the owner of all id's, it provides id's on request.
		std::deque<unsigned int> m_UniqueIds;
		std::deque<class VariableGroup*> m_PendingGroups;
		std::map<unsigned int, class VariableGroup*> m_VariableGroups;
		std::map<EndPoint, std::map<unsigned int, class VariableGroup*>, EndPoint::STLCompare> m_RemoteVariableGroups;
		class ZNode* m_ZNode;
		class IConnection* m_ConnOwner;
		clock_t m_LastIdPackRequestTS;
		unsigned char m_UniqueIdCounter;
		//volatile bool m_Closing;
		//std::mutex m_VariableGroupsMutex;
		//std::thread* m_SyncVariablesThread;
	};
}