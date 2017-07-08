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
		intervalSendIdRequest();
		resolvePendingGroups();	// causes groups to be created
		sendVariableGroups();	// syncs variables in the groups
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

	void VariableGroupNode::beginGroup( const i8_t* paramData, i32_t paramDataLen, i8_t channel, EPacketType type)
	{
		assert( VariableGroup::Last == nullptr && "should be NULL" );
		VariableGroup::Last = new VariableGroup(channel, type);

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

	void VariableGroupNode::beginGroupFromRemote(u32_t networkId, const ZEndpoint& ztp, EPacketType type)
	{
		assert( VariableGroup::Last == nullptr && "should be NULL" );
		VariableGroup::Last = new VariableGroup(-1, type);
		VariableGroup::Last->setNetworkId( networkId );
		VariableGroup::Last->setControl( EVarControl::Remote );
		// ----------------------------
		EndPoint etp = toEtp ( ztp );
		auto remoteGroupIt = m_RemoteVariableGroups.find( etp );
		if ( remoteGroupIt == m_RemoteVariableGroups.end() )
		{
			std::map<u32_t, VariableGroup*> newMap;
			newMap.insert( std::make_pair( networkId, VariableGroup::Last ) );
			m_RemoteVariableGroups.insert( std::make_pair( etp, newMap ) );
		}
		else
		{
			remoteGroupIt->second.insert( std::make_pair( networkId, VariableGroup::Last ) );
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
		sendIdPackProvide(etp, sm_AvailableIds);
	}

	void VariableGroupNode::recvIdProvide(const Packet& pack, const EndPoint& etp)
	{
		const i32_t numIds = sm_AvailableIds;
		if ( pack.len-1 != sizeof(u32_t)*sm_AvailableIds )
		{
			Platform::log( "invalid sender or serialization in: %s" , __FUNCTION__ );
			return;
		}
		u32_t* ids = (u32_t*)(pack.data+1);
		for (i32_t i = 0; i < sm_AvailableIds ; i++)
		{
			m_UniqueIds.emplace_back( ids[i] );
		}
	}

	void VariableGroupNode::recvVariableGroupCreate(const Packet& pack, const EndPoint& etp)
	{
		i8_t* payload = pack.data + 1;
		i32_t len = pack.len-1;
		i8_t name[RPC_NAME_MAX_LENGTH];
		if ( !ISocket::readFixed( name, RPC_NAME_MAX_LENGTH, payload, (RPC_NAME_MAX_LENGTH<len?RPC_NAME_MAX_LENGTH:len)) )
		{
			Platform::log( "serialization error in %s", __FUNCTION__ );
			/// TODO critical as no group can be created, cause desync
			return;
		}
		i8_t fname[RPC_NAME_MAX_LENGTH*2];
		Platform::formatPrint( fname, RPC_NAME_MAX_LENGTH*2, "__sgp_deserialize_%s", name );
		void* pf = Platform::getPtrFromName( fname );
		if ( pf )
		{
			// function signature
			ZEndpoint ztp = toZpt( etp );
			void (*pfunc)(ZNodePrivate*, const i8_t*, i32_t, const ZEndpoint&, EPacketType);
			pfunc = (decltype(pfunc)) pf;
			pfunc( m_PrivZ, payload, len, ztp, pack.type );
		}
		else
		{
			Platform::log( "removing conn, serialize group function: %s not found, trying from: %s", fname, __FUNCTION__ );
			/// TODO remove connection
		}
	}

	void VariableGroupNode::recvVariableGroupDestroy(const Packet& pack, const EndPoint& etp)
	{
		u32_t id = *(u32_t*)(pack.data+1);		
		VariableGroup* vg = findRemoteGroup( id, nullptr, true );
		if ( vg )
		{
			vg->unrefGroup();
			delete vg;
		#if _DEBUG
			vg =  findRemoteGroup( id, nullptr, false );
			assert( vg == nullptr && "vg still available" );
		#endif
		}
	}

	void VariableGroupNode::recvVariableGroupUpdate(const Packet& pack, const EndPoint& etp)
	{
		// try find group and update its contents with the latest data
		VariableGroup* vg = findRemoteGroup(pack.numGroups, nullptr);
		if ( vg && !vg->isBroken() )
		{
			vg->read(pack.data, pack.len, pack.groupBits);
		}
		else if ( vg && vg->isBroken() )
		{
			// cleanup this group as it will never be used again
			findRemoteGroup(pack.numGroups, nullptr, true);
			vg->unrefGroup();
			delete vg;
		}
	}

	void VariableGroupNode::sendCreateVariableGroup(u32_t networkId, const i8_t* paramData, i32_t paramDataLen)
	{
		// now that networkId is known, push it in front as first parameter of paramData
		*(u32_t*)(paramData + RPC_NAME_MAX_LENGTH) = networkId;
		m_ZNode->sendReliableOrdered( (u8_t)EGameNodePacketType::VariableGroupCreate, paramData, paramDataLen );
	}

	void VariableGroupNode::sendDestroyVariableGroup(u32_t networkId)
	{
		m_ZNode->sendReliableOrdered( (u8_t)EGameNodePacketType::VariableGroupDestroy, (const i8_t*)&networkId, sizeof(networkId) );
	}

	void VariableGroupNode::sendIdPackRequest()
	{
		m_ZNode->sendReliableOrdered((u8_t)EGameNodePacketType::IdPackRequest, nullptr, 0, nullptr, false, 0, false);
	}

	void VariableGroupNode::sendIdPackProvide(const EndPoint& etp, i32_t numIds)
	{
		assert( numIds >= 0 && "numIds invalid count" );
		if ( numIds > sm_AvailableIds )
			numIds = sm_AvailableIds;
		u32_t idPack[sm_AvailableIds];
		for (i32_t i = 0; i < numIds ; i++)
		{
			idPack[i] = m_UniqueIdCounter++;			
		}
		m_ZNode->sendReliableOrdered((u8_t)EGameNodePacketType::IdPackProvide,
									(const i8_t*)idPack, sizeof(u32_t)*numIds,
									&toZpt(etp), false, 0, false);
	}

	void VariableGroupNode::intervalSendIdRequest()
	{
		// The network id provides will never need to request Id's because it owns them.
		if ( m_IsNetworkIdProvider )
			return;

		// Request new id's when necessary
		if ( (i32_t)m_UniqueIds.size() < sm_AvailableIds )
		{
			clock_t tNow = ::clock();
			float dt = (float)(tNow - m_LastIdPackRequestTS) / (float)CLOCKS_PER_SEC;
			if ( dt >= .5f )
			{
				m_LastIdPackRequestTS = tNow;
				sendIdPackRequest();
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
				for (i32_t i = 0; i < (i32_t)m_PendingGroups.size() ; i++)
				{
					m_UniqueIds.emplace_back( m_UniqueIdCounter++ );
				}
			}
		}
		// If has pending groups to be resolved and unique network wide id's are availalbe, resolve them!
		while (m_PendingGroups.size()>0 && m_UniqueIds.size()>0)
		{
			auto& pvg = m_PendingGroups.front();
			u32_t id = m_UniqueIds.front();
			m_PendingGroups.pop_front();
			m_UniqueIds.pop_front();
			pvg.Vg->setNetworkId(id); // now that the id is set to something not 0, it will be automatically sync itself
			m_VariableGroups.insert( std::make_pair( id, pvg.Vg ) );
			sendCreateVariableGroup( id, pvg.ParamData, pvg.ParamDataLength );
		}
	}

	void VariableGroupNode::sendVariableGroups()
	{
		for ( auto vgIt = m_VariableGroups.cbegin(); vgIt!= m_VariableGroups.end();  )
		//for ( auto& kvp : m_VariableGroups )
		{
			VariableGroup* vg = vgIt->second;
			if ( vg->getNetworkId() == 0 )
			{
				vgIt++;
				continue;
			}
			// see if group is dirty and not broken from variables
			if ( vg->isDirty() && !vg->isBroken() )
			{
				vg->sendGroup(m_ZNode);
				vgIt++;
			}
			// if broken but this info is not yet transmitted, do so now
			else if ( vg->isBroken() && !vg->isDestroySent() )
			{
				vg->markDestroySent();
				sendDestroyVariableGroup( vg->getNetworkId() );
				delete vg;
				vgIt = m_VariableGroups.erase(vgIt);
			}
		}
	}

	VariableGroup* VariableGroupNode::findRemoteGroup(u32_t networkId, const EndPoint* etp, bool removeOnFind)
	{
		if ( etp )
		{
			auto remoteGroupIt = m_RemoteVariableGroups.find( *etp );
			if ( remoteGroupIt != m_RemoteVariableGroups.end() )
			{
				auto& networkVariables = remoteGroupIt->second;
				auto& groupIt = networkVariables.find( networkId );
				if ( groupIt != networkVariables.end() )
				{
					VariableGroup* vg = groupIt->second;
					if ( removeOnFind )
					{
						networkVariables.erase( groupIt );
					}
					return vg;
				}
			}
		}
		else
		{
			for ( auto& kvp : m_RemoteVariableGroups )
			{
				auto& networkVariables = kvp.second;
				auto& groupIt = networkVariables.find( networkId );
				if ( groupIt != networkVariables.end() )
				{
					VariableGroup* vg = groupIt->second;
					if ( removeOnFind )
					{
						networkVariables.erase( groupIt );
					}
					return vg;
				}
			}
		}
		return nullptr;
	}

}