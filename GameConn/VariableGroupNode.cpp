#pragma once

#include "VariableGroupNode.h"
#include "Zerodelay.h"
#include "VariableGroup.h"

#include <cassert>


namespace Zerodelay
{
	extern ZEndpoint toZpt( const EndPoint& r );


	VariableGroupNode::VariableGroupNode():
		m_ZNode(nullptr),
		m_ConnOwner(nullptr),
		m_UniqueIdCounter(1),
		m_IsNetworkIdProvider(false)
		//m_SyncVariablesThread(nullptr),
		//m_Closing(false)
	{
		m_LastIdPackRequestTS = -1;
		//m_SyncVariablesThread = new std::thread( [this] () { syncVariablesThread(); } );
	}

	VariableGroupNode::~VariableGroupNode() 
	{
		//m_Closing = true;
		//if ( m_SyncVariablesThread && m_SyncVariablesThread->joinable() )
		//{
		//	m_SyncVariablesThread->join();
		//}
		//delete m_SyncVariablesThread;
	}

	void VariableGroupNode::update()
	{
		checkAndsendNewIdsRequest();
		resolvePendingGroups();
		queueVariableGroupsIfDirty();
	}

	bool VariableGroupNode::recvPacket(const Packet& pack, const IConnection* conn)
	{
		EGameNodePacketType packType = (EGameNodePacketType)pack.data[0];
		switch ( packType )
		{
		case EGameNodePacketType::IdPackRequest:
			recvIdRequest( conn );			
			break;
		case EGameNodePacketType::IdPackProvide:
			recvIdProvide( pack, conn );
			break;
		default:
			return false;
		}
		return true;
	}

	void VariableGroupNode::beginGroup()
	{
		assert( VariableGroup::Last == nullptr && "should be NULL" );
		VariableGroup::Last = new VariableGroup();
		m_PendingGroups.emplace_back( VariableGroup::Last );
	}

	void VariableGroupNode::endGroup()
	{
		assert ( VariableGroup::Last != nullptr && "should be not NULL" );
		VariableGroup::Last = nullptr;
	}

	void VariableGroupNode::setIsNetworkIdProvider(bool isProvider)
	{
		m_IsNetworkIdProvider = isProvider;
	}

	void VariableGroupNode::recvIdRequest(const IConnection* sender)
	{
		assert ( sender && "invalid sender" );
		if ( !sender )
			return;
		if ( !m_IsNetworkIdProvider )
		{
			Platform::log( "id requested on node that is not a network id provider" );
			return;
		}
		unsigned int idPack[sm_AvailableIds];
		for (int i = 0; i < sm_AvailableIds ; i++)
		{
			idPack[i] = m_UniqueIdCounter++;			
		}
		m_ZNode->sendSingle( (unsigned char)EGameNodePacketType::IdPackProvide, (const char*)idPack, sizeof(unsigned int)*sm_AvailableIds,
							 &toZpt( sender->getEndPoint() ), false, EPacketType::Reliable_Ordered, 0, false );
	}

	void VariableGroupNode::recvIdProvide(const Packet& pack, const IConnection* sender)
	{
		const int numIds = sm_AvailableIds;
		assert ( sender && pack.len-1 == sizeof(unsigned int)*sm_AvailableIds && "invalid sender or data" );
		if ( !sender )
			return;
		unsigned int* ids = (unsigned int*)(pack.data+1);
		for (int i = 0; i < sm_AvailableIds ; i++)
		{
			m_UniqueIds.emplace_back( ids[i] );
		}
	}

	void VariableGroupNode::checkAndsendNewIdsRequest()
	{
		if ( m_IsNetworkIdProvider )
			return;

		// Request new id's when necessary
		if ( (int)m_UniqueIds.size() < sm_AvailableIds )
		{
			clock_t tNow = ::clock();
			float dt = (float)(tNow - m_LastIdPackRequestTS) / (float)CLOCKS_PER_SEC;
			if ( dt >= .5f )
			{
				m_LastIdPackRequestTS = tNow;
				m_ZNode->sendSingle( (unsigned char)EGameNodePacketType::IdPackRequest, nullptr, 0, nullptr, false, EPacketType::Reliable_Ordered, 0, false );
			}
		}
	}

	void VariableGroupNode::resolvePendingGroups()
	{
		// If is id provider, simulate the same behaviour as for requesters by pushing id's in the IdDeck
		if ( m_IsNetworkIdProvider )
		{
			if ( m_PendingGroups.size() )
			{
				for (int i = 0; i < (int)m_PendingGroups.size() ; i++)
				{
					m_UniqueIds.emplace_back( m_UniqueIdCounter++ );
				}
			}
		}
		// If has pending groups to be resolved and unique network wide id's are availalbe, resolve them!
		while (m_PendingGroups.size() && m_UniqueIds.size())
		{
			VariableGroup* vg = m_PendingGroups.front();
			unsigned int id = m_UniqueIds.front();
			m_PendingGroups.pop_front();
			m_UniqueIds.pop_front();
			vg->setNetworkId(id); // now that the id is set to something not 0, it will be automatically sync itself
			// this map is accessed by sync thread
		//	std::lock_guard<std::mutex> lock(m_VariableGroupsMutex);
			m_VariableGroups.insert( std::make_pair( id, vg ) );
		}
	}

	void VariableGroupNode::queueVariableGroupsIfDirty()
	{
		const int buffLen = 2048;
		char groupData[buffLen];

		m_ZNode->beginSend();

		for ( auto& kvp : m_VariableGroups )
		{
			VariableGroup* vg = kvp.second;
			if ( vg->isDirty() && vg->getNetworkId() != 0 && !vg->isBroken() )
			{
				int nOperations = 0; // bytes written or read
				if ( !vg->sync( true, groupData, buffLen, nOperations ) )
				{
					Platform::log("cannot sync variable group, because exceeding %d buff size", buffLen);
					return;
				}
				/// QQQ / TODO revise this because this makes it unreliable 
				m_ZNode->send( (unsigned char)EGameNodePacketType::VariableGroup, groupData, nOperations, EPacketType::Unreliable_Sequenced, 0, true );
				vg->setDirty( false );
			}
		}

		m_ZNode->endSend();
	}

	//void VariableGroupNode::syncVariablesThread()
	//{
	//	while ( !m_Closing )
	//	{
	//		{
	//			std::lock_guard<std::mutex> lock(m_VariableGroupsMutex);
	//			for (auto& kvp : m_VariableGroups)
	//			{
	//				auto* vg = kvp.second;
	//				if ( vg->isDirty() )
	//				{
	//					vg->startSync();
	//					vg->
	//					vg->endSync();
	//				}
	//			}
	//		}
	//		std::this_thread::sleep_for( std::chrono::milliseconds(3) );
	//	}
	//}

}