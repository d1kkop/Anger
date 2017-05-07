#pragma once

#include "Zerodelay.h"

#include <deque>
#include <map>
#include <ctime>


namespace Zerodelay
{
	class VariableGroupNode
	{
		static const int sm_AvailableIds = 25;

	public:
		VariableGroupNode();
		virtual ~VariableGroupNode();

		void update();
		bool recvPacket( const struct Packet& pack, const class IConnection* conn );

		void beginGroup();
		void endGroup();

	private:
		void recvIdRequest(const IConnection* sender);
		void recvIdProvide(const Packet& pack, const IConnection* sender);
		void checkAndsendNewIdsRequest();

		std::deque<unsigned int> m_UniqueIds;
		std::deque<class VariableGroup*> m_PendingGroups;
		std::map<unsigned int, class VariableGroup*> m_VariableGroups;
		class ZNode* m_ZNode;
		class IConnection* m_ConnOwner;
		clock_t m_LastIdPackRequestTS;
		unsigned char m_UniqueIdCounter;
	};
}