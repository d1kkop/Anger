#pragma once

#include "VariableGroupNode.h"
#include "Zerodelay.h"
#include "VariableGroup.h"
#include "Socket.h"
#include "SyncGroups.h"
#include "RUDPConnection.h"

#include <cassert>


namespace Zerodelay
{
	extern ZEndpoint toZpt( const EndPoint& r );
	extern EndPoint  toEtp( const ZEndpoint& z );


	VariableGroupNode::VariableGroupNode():
		m_ZNode(nullptr),
		m_ConnOwner(nullptr),
		m_UniqueIdCounter(1),
		m_IsNetworkIdProvider(false)
	{
		m_LastIdPackRequestTS = -1;
	}

	VariableGroupNode::~VariableGroupNode() 
	{
		for ( auto& kvp : m_VariableGroups )
		{
			delete kvp.second;
		}
		for ( auto& kvp : m_RemoteVariableGroups )
		{
			for ( auto& kvp2 : kvp.second )
			{
				delete kvp2.second;
			}
		}
	}

	void VariableGroupNode::update()
	{
		checkAndsendNewIdsRequest();
		resolvePendingGroups();
		sendVariableGroups();
	}

	bool VariableGroupNode::recvPacket(const Packet& pack, const IConnection* conn)
	{
		assert( conn && "invalid ptr" );
		EGameNodePacketType packType = (EGameNodePacketType)pack.data[0];
		auto etp = conn->getEndPoint();
		switch ( packType )
		{
		case EGameNodePacketType::IdPackRequest:
			recvIdRequest( etp );	
			break;
		case EGameNodePacketType::IdPackProvide:
			recvIdProvide( pack, etp );
			break;
		case EGameNodePacketType::VariableGroupCreate:
			recvVariableGroupCreate( pack, etp );
			break;
		case EGameNodePacketType::VariableGroupDestroy:
			recvVariableGroupDestroy( pack, etp );
			break;
		case EGameNodePacketType::VariableGroupUpdate:
			recvVariableGroupUpdate( pack, etp );
			break;
		default:
			return false;
		}
		return true;
	}

	void VariableGroupNode::beginGroup( const char* paramData, int paramDataLen, char channel )
	{
		assert( VariableGroup::Last == nullptr && "should be NULL" );
		VariableGroup::Last = new VariableGroup(channel);

		if ( paramDataLen >= PendingVariableGroup::MaxParamDataLength )
		{
			Platform::log( "param data too long for variable group in %s", __FUNCTION__ );
			paramDataLen = PendingVariableGroup::MaxParamDataLength;
			// TODO causee desync
		}

		// networkId will later be pushed in front as first param (but after rpcName of function)
		PendingVariableGroup pvg;
		Platform::memCpy( pvg.ParamData, PendingVariableGroup::MaxParamDataLength, paramData, paramDataLen );
		pvg.ParamDataLength = paramDataLen;
		pvg.Vg = VariableGroup::Last;
		
		m_PendingGroups.emplace_back( pvg );
	}

	void VariableGroupNode::beginGroupFromRemote(unsigned int nid, const ZEndpoint& ztp)
	{
		assert( VariableGroup::Last == nullptr && "should be NULL" );
		VariableGroup::Last = new VariableGroup(-1);
		VariableGroup::Last->setNetworkId( nid );
		VariableGroup::Last->setControl( EVarControl::Remote );
		// ----------------------------
		EndPoint etp = toEtp ( ztp );
		auto remoteGroupIt = m_RemoteVariableGroups.find( etp );
		if ( remoteGroupIt == m_RemoteVariableGroups.end() )
		{
			std::map<unsigned int, VariableGroup*> newMap;
			newMap.insert( std::make_pair( nid, VariableGroup::Last ) );
			m_RemoteVariableGroups.insert( std::make_pair( etp, newMap ) );
		}
		else
		{
			remoteGroupIt->second.insert( std::make_pair( nid, VariableGroup::Last ) );
		}
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

	void VariableGroupNode::recvIdRequest(const EndPoint& etp)
	{
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
		m_ZNode->sendSingle( (unsigned char)EGameNodePacketType::IdPackProvide, 
							 (const char*)idPack, sizeof(unsigned int)*sm_AvailableIds,
							 &toZpt(etp), false, EPacketType::Reliable_Ordered, 0, false );
	}

	void VariableGroupNode::recvIdProvide(const Packet& pack, const EndPoint& etp)
	{
		const int numIds = sm_AvailableIds;
		if ( pack.len-1 != sizeof(unsigned int)*sm_AvailableIds )
		{
			Platform::log( "invalid sender or serialization in: %s" , __FUNCTION__ );
			return;
		}
		unsigned int* ids = (unsigned int*)(pack.data+1);
		for (int i = 0; i < sm_AvailableIds ; i++)
		{
			m_UniqueIds.emplace_back( ids[i] );
		}
	}

	void VariableGroupNode::recvVariableGroupCreate(const Packet& pack, const EndPoint& etp)
	{
		char* payload = pack.data + 1;
		int len = pack.len-1;
		char name[RPC_NAME_MAX_LENGTH];
		if ( !ISocket::readFixed( name, RPC_NAME_MAX_LENGTH, payload, (RPC_NAME_MAX_LENGTH<len?RPC_NAME_MAX_LENGTH:len)) )
		{
			Platform::log( "serialization error in %s", __FUNCTION__ );
			/// TODO critical as no group can be created, cause desync
			return;
		}
		char fname[RPC_NAME_MAX_LENGTH*2];
		Platform::formatPrint( fname, RPC_NAME_MAX_LENGTH*2, "__sgp_deserialize_%s", name );
		void* pf = Platform::getPtrFromName( fname );
		if ( pf )
		{
			// function signature
			ZEndpoint ztp = toZpt( etp );
			void (*pfunc)(ZNodePrivate*, const char*, int, const ZEndpoint&);
			pfunc = (decltype(pfunc)) pf;
			pfunc( m_PrivZ, payload, len, ztp );
		}
		else
		{
			Platform::log( "removing conn, serialize group function: %s not found, trying from: %s", fname, __FUNCTION__ );
			/// TODO remove connection
		}
	}

	void VariableGroupNode::recvVariableGroupDestroy(const Packet& pack, const EndPoint& etp)
	{
		unsigned int id = *(unsigned int*)(pack.data+1);		
		VariableGroup* vg = findRemoteGroup( id, nullptr, true );
		delete vg;
	}

	void VariableGroupNode::recvVariableGroupUpdate(const Packet& pack, const EndPoint& etp)
	{
		if ( pack.len < 5 )
		{
			Platform::log( "serialization error in: %s", __FUNCTION__ );
			return;
		}
		unsigned int networkId = *(unsigned int*)(pack.data+1);
		VariableGroup* vg = findRemoteGroup(networkId, nullptr);
		if ( vg )
		{
			int buffLen = pack.len-1;
			vg->sync( false, pack.data+1, buffLen );
		}
	}

	void VariableGroupNode::sendCreateVariableGroup(unsigned int networkId, const char* paramData, int paramDataLen)
	{
		// now that networkId is known, push it in front as first parameter of paramData
		*(unsigned int*)(paramData + RPC_NAME_MAX_LENGTH) = networkId;
		m_ZNode->sendSingle( (unsigned char)EGameNodePacketType::VariableGroupCreate, paramData, paramDataLen );
	}

	void VariableGroupNode::sendDestroyVariableGroup(unsigned int networkId)
	{
		m_ZNode->sendSingle( (unsigned char)EGameNodePacketType::VariableGroupDestroy, (const char*)&networkId, sizeof(networkId) );
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
		while (m_PendingGroups.size()>0 && m_UniqueIds.size()>0)
		{
			auto& pvg = m_PendingGroups.front();
			unsigned int id = m_UniqueIds.front();
			m_PendingGroups.pop_front();
			m_UniqueIds.pop_front();
			pvg.Vg->setNetworkId(id); // now that the id is set to something not 0, it will be automatically sync itself
			m_VariableGroups.insert( std::make_pair( id, pvg.Vg ) );
			sendCreateVariableGroup( id, pvg.ParamData, pvg.ParamDataLength );
		}
	}

	void VariableGroupNode::sendVariableGroups()
	{
		m_ZNode->beginSend();
		for ( auto& kvp : m_VariableGroups )
		{
			VariableGroup* vg = kvp.second;
			if ( vg->getNetworkId() == 0 )
				continue;

			if ( /*vg->isDirty() &&*/ !vg->isBroken() )
			{
				char groupData[2048];
				int buffLen = 2000; // leave room for hdr size
				int oldBuffLen = buffLen;
				if ( !vg->sync( true, groupData, buffLen ) )
				{
					Platform::log("cannot sync variable group, because exceeding %d buff size", buffLen);
					m_ZNode->endSend(); // unlock
					return;
				}
				/// QQQ / TODO revise this because this makes it unreliable 
				int bytesWritten = oldBuffLen - buffLen;
				m_ZNode->send( (unsigned char)EGameNodePacketType::VariableGroupUpdate, groupData, bytesWritten, EPacketType::Unreliable_Sequenced, vg->getChannel(), true );
			}
			else if ( vg->isBroken() && !vg->isDestroySent() )
			{
				vg->markDestroySent();
				sendDestroyVariableGroup( vg->getNetworkId() );
			}
		}
		m_ZNode->endSend();
	}

	VariableGroup* VariableGroupNode::findRemoteGroup(unsigned int networkId, const EndPoint* etp, bool removeOnFind)
	{
		if ( etp )
		{
			auto remoteGroupIt = m_RemoteVariableGroups.find( *etp );
			if ( remoteGroupIt != m_RemoteVariableGroups.end() )
			{
				auto& networkVariables = remoteGroupIt->second;
				auto groupIt = networkVariables.find( networkId );
				if ( groupIt != networkVariables.end() )
				{
					if ( removeOnFind )
					{
						networkVariables.erase( groupIt );
					}
					return groupIt->second;
				}
			}
		}
		else
		{
			for ( auto kvp : m_RemoteVariableGroups )
			{
				auto& networkVariables = kvp.second;
				auto groupIt = networkVariables.find( networkId );
				if ( groupIt != kvp.second.end() )
				{
					if ( removeOnFind )
					{
						networkVariables.erase( groupIt );
					}
					return groupIt->second;
				}
			}
		}
		return nullptr;
	}
}