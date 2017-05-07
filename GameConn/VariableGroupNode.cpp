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
		m_UniqueIdCounter(1)
	{
		m_LastIdPackRequestTS = -1;
	}

	VariableGroupNode::~VariableGroupNode() = default;

	void VariableGroupNode::update()
	{
		checkAndsendNewIdsRequest();

		// If has pending groups to be resolved and unique network wide id's are availalbe, resolve them!
		while ( m_PendingGroups.size() && m_UniqueIds.size() )
		{
			VariableGroup* vg = m_PendingGroups.front();
			unsigned int id   = m_UniqueIds.front();
			m_PendingGroups.pop_front();
			m_UniqueIds.pop_front();
			vg->setNetworkId( id ); // now that the id is set to something not 0, it will be automatically sync itself
		}
	}

	bool VariableGroupNode::recvPacket(const Packet& pack, const IConnection* conn)
	{
		EGameNodePacketType packType = (EGameNodePacketType)pack.data[0];
		const char* payload  = pack.data+1; // first byte is PacketType
		int payloadLen = pack.len-1;  // len includes the packetType byte
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

	void VariableGroupNode::recvIdRequest(const IConnection* sender)
	{
		assert ( sender && "invalid sender" );
		int numIds = sm_AvailableIds*2;
		unsigned int* idPack = new unsigned int[numIds];
		for (int i = 0; i < sm_AvailableIds ; i++)
		{
			idPack[i] = m_UniqueIdCounter++;			
		}
		m_ZNode->sendSingle( (unsigned char)EGameNodePacketType::IdPackProvide, (const char*)idPack, sizeof(unsigned int)*numIds,
							 &toZpt( sender->getEndPoint() ), false, EPacketType::Reliable_Ordered, 0, false );
		delete [] idPack;
	}

	void VariableGroupNode::recvIdProvide(const Packet& pack, const IConnection* sender)
	{
		int numIds = sm_AvailableIds*2;
		assert ( sender && pack.len-1 == sizeof(unsigned int)*numIds && "invalid sender or data" );
		unsigned int* ids = (unsigned int*)pack.data+1;
		for (int i = 0; i < sm_AvailableIds*2 ; i++)
		{
			m_UniqueIds.emplace_back( ids[i] );
		}
	}

	void VariableGroupNode::checkAndsendNewIdsRequest()
	{
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
}