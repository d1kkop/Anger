#include "VariableGroupNode.h"
#include "Zerodelay.h"
#include "VariableGroup.h"
#include "Socket.h"
#include "SyncGroups.h"
#include "RUDPLink.h"
#include "ConnectionNode.h"
#include "Util.h"

#include <cassert>


namespace Zerodelay
{
	extern ZEndpoint toZpt( const EndPoint& r );
	extern EndPoint  toEtp( const ZEndpoint& z );


	VariableGroupNode::VariableGroupNode():
		m_ZNode(nullptr),
		m_UniqueIdCounter(1), // Zero is initially not used, it means no valid ID for now.
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

	void VariableGroupNode::postInitialize(ZNode* zNode, ConnectionNode* connNode)
	{
		assert( !m_ZNode || !m_ConnectionNode );
		m_ZNode = zNode;
		m_ConnectionNode = connNode;

		// on new connect, put variable group map (with empty set of groups) in list so that we know the set of known EndPoints
		m_ZNode->bindOnNewConnection( [this] (auto& ztp)
		{
			EndPoint etp = toEtp( ztp );
			if ( m_RemoteVariableGroups.count(etp) != 1 )
			{
				std::map<u32_t, VariableGroup*> newMap;
				m_RemoteVariableGroups.insert( std::make_pair( etp, newMap ) );
			}
			else
			{
				Platform::log("WARNING: received on new connection multiple times from %s", etp.asString().c_str());
			}
		});

		// on disconnect, remove set of variable groups and do not allow new ones to be created if no longer in set of endpoints
		m_ZNode->bindOnDisconnect( [this] (auto thisConnection, auto& ztp, auto reason)
		{
			if ( thisConnection )
				return;
			EndPoint etp = toEtp( ztp );
			auto it = m_RemoteVariableGroups.find( etp );
			if ( it != m_RemoteVariableGroups.end() )
			{
				for ( auto& kvp : it->second )
				{
					auto* vg = kvp.second;
					vg->unrefGroup();
					delete vg;
				}
				m_RemoteVariableGroups.erase( it );
			}
			else
			{
				Platform::log("WARNING: received disconnect multiple times from: %s", etp.asString().c_str());
			}
		});
	}

	void VariableGroupNode::update()
	{
		intervalSendIdRequest();
		resolvePendingGroups();	// causes groups to be created
		sendVariableGroups();	// syncs variables in the groups
	}

	bool VariableGroupNode::processPacket(const Packet& pack, const EndPoint& etp)
	{
		if ( pack.type == EHeaderPacketType::Reliable_Ordered || pack.type == EHeaderPacketType::Unreliable_Sequenced )
		{
			EDataPacketType packType = (EDataPacketType)pack.data[0];
			switch ( packType )
			{
			case EDataPacketType::IdPackRequest:
				recvIdRequest( etp );	
				break;
			case EDataPacketType::IdPackProvide:
				recvIdProvide( pack, etp );
				break;
			case EDataPacketType::VariableGroupCreate:
				recvVariableGroupCreate( pack, etp );
				break;
			case EDataPacketType::VariableGroupDestroy:
				recvVariableGroupDestroy( pack, etp );
				break;
			default:
				// unhandled packet
				return false;
			}
			// packet was handled
			return true;
		}
		else if ( pack.type == EHeaderPacketType::Reliable_Newest )
		{
			recvVariableGroupUpdate( pack, etp );
			return true;
		}
		// unhandled packet
		return false;
	}

	void VariableGroupNode::deferredCreateGroup( const i8_t* paramData, i32_t paramDataLen, i8_t channel)
	{
		if ( paramDataLen >= PendingVariableGroup::MaxParamDataLength )
		{
			Platform::log( "CRITICAL param data too long for variable group in %s, variable group was not created", __FUNCTION__ );
			return;
		}

		// networkId will later be pushed in front as first param (but after rpcName of function)
		PendingVariableGroup pvg;
		Platform::memCpy( pvg.ParamData, PendingVariableGroup::MaxParamDataLength, paramData, paramDataLen );
		pvg.ParamDataLength = paramDataLen;
		pvg.Channel = channel;
		m_PendingGroups.emplace_back( pvg );
	}

	void VariableGroupNode::beginNewGroup(u32_t networkId, const ZEndpoint* ztp)
	{
		assert( VariableGroup::Last == nullptr && "should be NULL" );
		VariableGroup::Last = new VariableGroup();
		VariableGroup::Last->setNetworkId( networkId );
		VariableGroup::Last->setControl( ztp ? EVarControl::Remote : (m_ZNode->getRoutingMethod() == ERoutingMethod::ClientServer ? EVarControl::Semi : EVarControl::Full) );
		// ----------------------------
		if (ztp) // is remote group
		{
			EndPoint etp = toEtp ( *ztp );
			auto remoteGroupIt = m_RemoteVariableGroups.find( etp );
			if ( remoteGroupIt != m_RemoteVariableGroups.end() ) // see if remote endpoint is know
			{
				assert(remoteGroupIt->second.count(networkId)==0);
				remoteGroupIt->second.insert( std::make_pair( networkId, VariableGroup::Last ) );
			}
			else // discards any creation before connection was established or after was disconnected/lost
			{
				Platform::log("INFO: Discarding remote group creation from %s, as it was not connected or already disconnected\n", ztp->asString().c_str());
			}
		}
		else
		{
			assert(m_VariableGroups.count(networkId)==0);
			m_VariableGroups.insert( std::make_pair(networkId, VariableGroup::Last) );
		}
	}

	void VariableGroupNode::endNewGroup()
	{
		assert( VariableGroup::Last != nullptr && "should not be NULL" );
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
			Platform::log( "NetworkId requested on node that is not a network id provider. If there is no network id provider the network, no variable groups can be created. Usually the server or super peer (in peer2peer) is a network id provider. Use Znode->setNetworkIdProvider(true) on server or super peer." );
			return;
		}
		sendIdPackProvide(etp, sm_AvailableIds);
	}

	void VariableGroupNode::recvIdProvide(const Packet& pack, const EndPoint& etp)
	{
		const i32_t numIds = sm_AvailableIds;
		if ( pack.len-1 != sizeof(u32_t)*sm_AvailableIds )
		{
			Platform::log( "CRITIAL invalid sender or serialization in: %s" , __FUNCTION__ );
			return;
		}
		u32_t* ids = (u32_t*)(pack.data+1);
		for (i32_t i = 0; i < sm_AvailableIds ; i++)
		{
			m_UniqueIds.emplace_back( ids[i] );
		}
		Platform::log("Received %d new Ids from %s", sm_AvailableIds, etp.asString().c_str());
	}

	void VariableGroupNode::recvVariableGroupCreate(const Packet& pack, const EndPoint& etp)
	{
		i8_t* payload = pack.data + 1; // first byte is hdr type
		i32_t len = pack.len-1; // minus hdr type byte
		ZEndpoint ztp = toZpt( etp );

		// If is client server, relay the message to other clients
		if ( m_ZNode->getRoutingMethod() == ERoutingMethod::ClientServer )
		{
			m_ZNode->sendReliableOrdered( (u8_t)pack.type, payload, len, &ztp, true, pack.channel, false );
		}

		callCreateVariableGroup(payload, len, true, &ztp);
	}

	void VariableGroupNode::recvVariableGroupDestroy(const Packet& pack, const EndPoint& etp)
	{
		u32_t id = *(u32_t*)(pack.data+1);		
		VariableGroup* vg = findOrRemoveBrokenGroup( id, nullptr );
		if ( vg )
		{
			vg->unrefGroup();
		#if _DEBUG
			vg =  findOrRemoveBrokenGroup( id, nullptr );
			assert( vg == nullptr && "vg still available" );
		#endif
			// call callbacks after all is done
			Util::forEachCallback(m_GroupUpdateCallbacks, [&] (const GroupCallback& gc)
			{
				(gc)(&etp, id);
			});
		}
		else
		{
			Platform::log( "WARNING: tried to remove variable group (id = %d) which was already destroyed or never created", id );
		}
	}

	void VariableGroupNode::recvVariableGroupUpdate(const Packet& pack, const EndPoint& etp)
	{
		// Data in packet is now offsetted at 'off_RelNew_Num' from zero already.
		// So first data at data[0] is stored value at 'off_RelNew_Num'
		const auto* data = pack.data;
		int32_t buffLen  = pack.len;
		i32_t numGroups  = *(i32_t*)(data);
		data += 4;
		buffLen -= 4;
		for (i32_t i=0; i<numGroups; ++i)
		{
			// returns ptr to next group or null if has reached end
			u32_t groupId = *(u32_t*)data;
			if (!deserializeGroup(data, buffLen))
			{
				Platform::log( "ERROR deserialization of variable group failed dataLen %d", buffLen );
				break;
			}
			else
			{
				// call callbacks after all is done
				for ( auto& cb : m_GroupUpdateCallbacks )
				{
					if ( cb ) cb(&etp, groupId);
				}
			}
		}
		// If all data is exactly read, then buffLen should be zero, if groups are skipped, then this is subtracted from BuffLen, so should still be zero.
		assert( buffLen == 0 );
		// if ( buffLen != 0 ) // TODO set critical error
						
	}

	void VariableGroupNode::sendCreateVariableGroup(u32_t networkId, const i8_t* paramData, i32_t paramDataLen, i8_t channel)
	{
		// now that networkId is known, push it in front as first parameter of paramData
		*(u32_t*)(paramData + RPC_NAME_MAX_LENGTH) = networkId;
		if ( !m_ZNode->sendReliableOrdered( (u8_t)EDataPacketType::VariableGroupCreate, paramData, paramDataLen, nullptr, false, channel, true )) 
		{
			Platform::log("FAILED: to dispatch create variable group with network id %d", networkId);
		}
	}

	void VariableGroupNode::sendDestroyVariableGroup(u32_t networkId)
	{
		if ( !m_ZNode->sendReliableOrdered( (u8_t)EDataPacketType::VariableGroupDestroy, (const i8_t*)&networkId, sizeof(networkId) ))
		{
			Platform::log("FAILED: sending destroy variable group with id %d...", networkId);
		}
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
		// Send reliable ordered. If connection is dropped just after sending, the connection is removed and no retransmission will take place.
		m_ZNode->sendReliableOrdered((u8_t)EDataPacketType::IdPackProvide,
									(const i8_t*)idPack, sizeof(u32_t)*numIds,
									&toZpt(etp), false, 0, false);
	}

	void VariableGroupNode::intervalSendIdRequest()
	{
		// The network id provides will never need to request Id's because it owns them.
		if ( m_IsNetworkIdProvider )
			return;

		// Request new id's when necessary and only if at least a single connection is connected.
		if ( (m_ZNode->getNumOpenConnections() > 0) && ((i32_t)m_UniqueIds.size() < sm_AvailableIds) )
		{
			clock_t tNow = ::clock();
			float dt = (float)(tNow - m_LastIdPackRequestTS) / (float)CLOCKS_PER_SEC;
			if ( dt >= .5f )
			{
				m_LastIdPackRequestTS = tNow;
				// Send unreliable sequenced, because it is possible that at the time of sending the data, no connections
				// are fully connected anymore in which case the reliable ordered packet becomes unreliable.
				m_ZNode->sendUnreliableSequenced((u8_t)EDataPacketType::IdPackRequest, nullptr, 0, nullptr, false, 0, false);
				Platform::log("Sending IdPack request...");
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
			sendCreateVariableGroup( id, pvg.ParamData, pvg.ParamDataLength, pvg.Channel );
			callCreateVariableGroup( pvg.ParamData, pvg.ParamDataLength, false, nullptr );
		}
	}

	void VariableGroupNode::sendVariableGroups()
	{
		for ( auto vgIt = m_VariableGroups.begin(); vgIt!= m_VariableGroups.end();  )
		//for ( auto& kvp : m_VariableGroups )
		{
			VariableGroup* vg = vgIt->second;
			if ( vg->getNetworkId() == 0 ) // network id not set yet (the case when no free id's available, have to wait)
			{
				vgIt++;
				continue;
			}
			// see if group is dirty and not broken from variables
			if ( vg->isDirty() && !vg->isBroken() )
			{
				vg->sendGroup(m_ZNode);
				Util::forEachCallback(m_GroupUpdateCallbacks, [&] (const GroupCallback& gc)
				{
					(gc)(nullptr, vg->getNetworkId());
				});
				vgIt++;
			}
			// if broken but this info is not yet transmitted, do so now
			else if ( vg->isBroken() && !vg->isDestroySent() )
			{
				vg->markDestroySent();
				sendDestroyVariableGroup( vg->getNetworkId() );
				Util::forEachCallback(m_GroupDestroyCallbacks, [&] (const GroupCallback& gc)
				{
					(gc)(nullptr, vg->getNetworkId());
				});
				delete vg;
				vgIt = m_VariableGroups.erase(vgIt);
			}
			else // group not dirty and not broken, skip
			{
				vgIt++;
			}
		}
	}

	void VariableGroupNode::callCreateVariableGroup(i8_t* data, i32_t len, bool remote, const ZEndpoint* ztp)
	{
		i8_t name[RPC_NAME_MAX_LENGTH];
		if (!Util::readFixed(name, RPC_NAME_MAX_LENGTH, data, (RPC_NAME_MAX_LENGTH < len ? RPC_NAME_MAX_LENGTH : len)))
		{
			Platform::log("CRITICAL serialization error in %s, trying to read function name %s, remote variable group was not created!", __FUNCTION__, name);
			return;
		}
		u32_t nId = *(u32_t*)(data + RPC_NAME_MAX_LENGTH);
		i8_t fname[RPC_NAME_MAX_LENGTH * 2];
		i8_t* ptNxt = Util::appendString(fname, RPC_NAME_MAX_LENGTH * 2, "__sgp_deserialize_");
		Util::appendString(ptNxt, RPC_NAME_MAX_LENGTH, name);
		void* ptrUserCb = Platform::getPtrFromName(fname);
		if (ptrUserCb)
		{
			beginNewGroup( nId, ztp );
			// Call user code for variable group
			void(*userCallback)(ZNode*, const i8_t*, i32_t);
			userCallback = (decltype(userCallback))ptrUserCb;
			userCallback(m_ZNode, data, len);
			endNewGroup();
		}
		else
		{
			Platform::log("CRITICAL: serialize group function: %s not found, from: %s, no remote variable group was created!", fname, __FUNCTION__);
		}
	}

	bool VariableGroupNode::deserializeGroup(const i8_t*& data, i32_t& buffLen)
	{
		// remember old ptr and buffLen in case the group is not available and must be skipped
		auto oldDataPtr = data;
		auto oldBuffLen = buffLen;
		// deserialize group
		u32_t groupId = *(u32_t*)data;
		data += 4;	buffLen -= 4;
		u16_t groupBits = *(u16_t*)data;
		data += 2;	buffLen -= 2;
		u16_t skipBytes = *(u16_t*)data; 
		data += 2;	buffLen -= 2;
		VariableGroup* vg = findOrRemoveBrokenGroup( groupId, nullptr );
		assert( !vg || (vg && !vg->isBroken()) );
		bool bSkip   = vg == nullptr;
		bool bResult = true;
		if ( !bSkip )
		{
			i32_t oldBuffLen = buffLen;
			if ( !vg->read(data, buffLen, groupBits) )
			{
				bSkip   = true;
				bResult = false; // deserialize failure
			}
		}
		if (bSkip)
		{
			// skip entire group
			data    = oldDataPtr + skipBytes;
			buffLen = oldBuffLen - skipBytes;
		}
		return bResult;
	}

	VariableGroup* VariableGroupNode::findOrRemoveBrokenGroup(u32_t networkId, const EndPoint* etp)
	{
		auto fnCheckBrokenAndReturn = [] (auto& it, auto& nVars)
		{
			if ( it->second->isBroken() )
			{
				delete it->second;
				nVars.erase(it);
				return (VariableGroup*)nullptr;
			}
			return it->second;
		};

		if ( etp )
		{
			auto remoteGroupIt = m_RemoteVariableGroups.find( *etp );
			if ( remoteGroupIt != m_RemoteVariableGroups.end() )
			{
				auto& networkVariables = remoteGroupIt->second;
				auto& groupIt = networkVariables.find( networkId );
				if ( groupIt != networkVariables.end() )
				{
					return fnCheckBrokenAndReturn( groupIt, networkVariables );
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
					return fnCheckBrokenAndReturn( groupIt, networkVariables );
				}
			}
		}
		return nullptr;
	}
}