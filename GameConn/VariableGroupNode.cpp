#include "VariableGroupNode.h"
#include "Zerodelay.h"
#include "VariableGroup.h"
#include "Socket.h"
#include "SyncGroups.h"
#include "RUDPLink.h"
#include "CoreNode.h"
#include "Util.h"
#include "NetVariable.h"
#include "BinSerializer.h"

#include <cassert>


namespace Zerodelay
{
	constexpr i8_t VGChannel = 1;


	VariableGroupNode::VariableGroupNode():
		m_ZNode(nullptr)
	{
		reset(true);
	}

	VariableGroupNode::~VariableGroupNode() 
	{
		reset(false);
	}

	void VariableGroupNode::reset(bool isConstructorCall)
	{
		if (!isConstructorCall)
		{
			// destruct memory
			Platform::log("VariableGroupNode reset called, num local groups %d, num remote endpoints %d.", 
						  (i32_t)m_VariableGroups.size(), (i32_t)m_RemoteVariableGroups.size());
		}
		for ( auto& kvp : m_VariableGroups )
		{
			delete kvp.second;
		}
		m_VariableGroups.clear();
		for ( auto& kvp : m_RemoteVariableGroups )
		{
			for ( auto& kvp2 : kvp.second )
			{
				delete kvp2.second;
			}
		}
		m_RemoteVariableGroups.clear();
		// reset state
		m_UniqueIds.clear();
		m_PendingGroups.clear();
		for (auto& bg : m_BufferedGroups) delete [] bg.Data;
		m_BufferedGroups.clear();
		m_LastIdPackRequestTS = 0;
		m_UniqueIdCounter = 0;
		// 
		// !! leave ptrs to other managers and user specified settings (callbacks etc) in tact !!
		//
		// m_IsNetworkIdProvider = false;
		// m_RelayVariableGroupEvents = false;
		// class CoreNode* m_CoreNode;
		// class ZNode* m_ZNode;
		// class ConnectionNode* m_ConnectionNode;
	}

	void VariableGroupNode::postInitialize(CoreNode* coreNode)
	{
		assert( !m_CoreNode && !m_ZNode || !m_ConnectionNode );
		m_CoreNode = coreNode;
		m_ZNode = coreNode->zn();
		m_ConnectionNode = coreNode->cn();
	}

	void VariableGroupNode::setupConnectionCallbacks()
	{
		// on new connect, put variable group map (with empty set of groups) in list so that we know the set of known EndPoints
		m_ZNode->bindOnNewConnection( [this] (bool directLink, auto& ztp, auto& additionalData)
		{
			EndPoint etp = Util::toEtp( ztp );
			if ( m_RemoteVariableGroups.count(etp) != 1 )
			{
				std::map<u32_t, VariableGroup*> newMap;
				m_RemoteVariableGroups.insert( std::make_pair( etp, newMap ) );
				if ( directLink ) // for p2p and client-server this should be correct
				{
					sendAllVariableCreateEventsTo( ztp );
				}
			}
			else
			{
				assert(false);
				Platform::log("WARNING: Received on new connection multiple times from %s.", etp.toIpAndPort().c_str());
			}
		});

		// on disconnect, remove set of variable groups and do not allow new ones to be created if no longer in set of endpoints
		m_ZNode->bindOnDisconnect( [this] (auto thisConnection, auto& ztp, auto reason)
		{
			if ( thisConnection )
				return;
			EndPoint etp = Util::toEtp( ztp );
			auto it = m_RemoteVariableGroups.find( etp );
			if ( it != m_RemoteVariableGroups.end() )
			{
				for ( auto& kvp : it->second )
				{
					auto* vg = kvp.second;
					unBufferGroup( vg->getNetworkId() );
					delete vg;
				}
				m_RemoteVariableGroups.erase( it );
			}
			else
			{
				Platform::log("WARNING: Received disconnect multiple times from: %s.", etp.toIpAndPort().c_str());
			}
		});
	}

	void VariableGroupNode::update()
	{
		if (m_CoreNode->hasCriticalErrors()) return;
		intervalSendIdRequest();
		resolvePendingGroups();	// causes groups to be created
		sendUpdatedVariableGroups();	// syncs variables in the groups
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

	void VariableGroupNode::deferredCreateGroup( const i8_t* paramData, i32_t paramDataLen )
	{
		// networkId not know yet, therefore store memory and add as pending group to be solved later
		GroupPendingData pvg;
		Platform::memCpy( pvg.Data, sizeof(pvg.Data), paramData, paramDataLen );
		pvg.TotalDataLength = paramDataLen;
		m_PendingGroups.emplace_back( pvg );
	}

	void VariableGroupNode::beginNewGroup(u32_t networkId, const ZEndpoint* owner)
	{
		assert( VariableGroup::Last == nullptr && "should be NULL" );
		VariableGroup::Last = new VariableGroup(m_ZNode);
		VariableGroup::Last->setNetworkId( networkId );
		VariableGroup::Last->setControl( owner ? EVarControl::Remote : (m_ZNode->isAuthorative() ? EVarControl::Full : EVarControl::Semi) );
		VariableGroup::Last->setOwner( owner );
		// ----------------------------
		if (owner) // is remote group
		{
			EndPoint etp = Util::toEtp ( *owner );
			auto remoteGroupIt = m_RemoteVariableGroups.find( etp );
			if ( remoteGroupIt != m_RemoteVariableGroups.end() ) // see if remote endpoint is know
			{
				assert(remoteGroupIt->second.count(networkId)==0);
				remoteGroupIt->second.insert( std::make_pair( networkId, VariableGroup::Last ) );
			}
			else // discards any creation before connection was established or after was disconnected/lost
			{
				Platform::log("WARNING: Discarding remote group creation from %s, as it was not connected or already disconnected.", owner->toIpAndPort().c_str());
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
			Platform::log( "WARNING: Invalid sender or serialization in: %s" , __FUNCTION__ );
			return;
		}
		u32_t* ids = (u32_t*)(pack.data+1);
		for (i32_t i = 0; i < sm_AvailableIds ; i++)
		{
			m_UniqueIds.emplace_back( ids[i] );
		}
		Platform::log("Received %d new Ids from %s.", sm_AvailableIds, etp.toIpAndPort().c_str());
	}

	void VariableGroupNode::recvVariableGroupCreate(const Packet& pack, const EndPoint& etp)
	{
		BinSerializer bs(pack.data, pack.len, pack.len); 
		__CHECKED( bs.moveRead(1) ); // first byte is data hdr type

		// [functionName][paramData][netId][remote]<endpoint>
		i8_t name[RPC_NAME_MAX_LENGTH];
		i8_t paramData[RPC_DATA_MAX];
		i16_t paramDataLen;
		i32_t netId;
		bool remote;
		ZEndpoint ztp;
		__CHECKED( bs.readStr(name, RPC_NAME_MAX_LENGTH) );
		__CHECKED( bs.read16(paramDataLen) );
		__CHECKED( paramDataLen <= RPC_DATA_MAX );
		__CHECKED( bs.read(paramData, paramDataLen) );
		__CHECKED( bs.read(netId) );
		__CHECKED( bs.read(remote) );
		if ( remote )
		{
			__CHECKED( bs.read(ztp) );
		}
		else ztp = Util::toZpt(etp);

		// if 'true' server in client-server arch, dispatch to all
		if ( m_ZNode->isAuthorative() && !m_CoreNode->isP2P() )
		{
			// Copy binstream as we may need append data while the pack.len buffer may not be big enough
			BinSerializer bs2;
			__CHECKED( bs.getWrite() <= bs.getMaxSize() );
			__CHECKED( bs2.write(bs.data()+1, bs.length()-1) );
			if (!remote) // write remote etp
			{
				__CHECKED( bs2.write(etp) );
			}
			// Confusing, but take first byte of first stream (data packet hdr) and take payload stream2
			m_ZNode->sendReliableOrdered( bs.data()[0], bs2.data(), bs2.length(), &ztp, true, VGChannel, false );

			// Buffer the creation
			bufferGroup( netId, bs2 );
		}

		// as this is the receive func for creating a var group, it has always a remote endpoint owner
		callCreateVariableGroup(name, netId, paramData, paramDataLen, &ztp);
	}

	void VariableGroupNode::recvVariableGroupDestroy(const Packet& pack, const EndPoint& etp)
	{
		BinSerializer bs(pack.data, pack.len, pack.len);
		__CHECKED( bs.moveRead(1) ); // skip data hdr type

		// only network id attached
		u32_t netId;
		__CHECKED( bs.read(netId) );

		// if 'true' server in client-server arch, dispatch to all
		if ( m_ZNode->isAuthorative() || m_CoreNode->isP2P() )
		{
			ZEndpoint ztp = Util::toZpt( etp );
			m_ZNode->sendReliableOrdered( (u8_t)pack.data[0], pack.data+1, pack.len-1, &ztp, true, pack.channel, false );
			unBufferGroup( netId );
		}

		VariableGroup* vg = findGroup( netId );
		if ( vg )
		{
			removeGroup( netId );

		#if ZERODELAY_DEBUG
			vg =  findGroup( netId );
			assert( vg == nullptr && "vg still available" );
		#endif

			delete vg;

			// call callbacks after all is done
			Util::forEachCallback(m_GroupDestroyCallbacks, [&] (const GroupCallback& gc)
			{
				(gc)(&etp, netId);
			});
		}
		else
		{
			Platform::log( "WARNING: Tried to remove variable group (id = %d) which was already destroyed or never created.", netId );
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
				Platform::log( "CRITICAL: Deserialization of variable group failed dataLen %d.", buffLen );
				m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
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
		if (buffLen != 0)
		{
			Platform::log( "CRITICAL: Deserialization of %d variable groups was not correct.", numGroups );
			m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE);
		}				
	}

	void VariableGroupNode::sendCreateVariableGroup(BinSerializer& bs, const ZEndpoint* target, std::vector<ZAckTicket>* traceTickets)
	{
		m_ZNode->sendReliableOrdered( (u8_t)EDataPacketType::VariableGroupCreate, bs.data(), bs.getWrite(), target, false, VGChannel, true, true, traceTickets );
	}

	void VariableGroupNode::sendCreateVariableGroup(const i8_t* funcName, const i8_t* paramData, i32_t paramDataLen, 
													u32_t netId, const ZEndpoint* owner, bool buffer,
													std::vector<ZAckTicket>* traceTickets)
	{
		// [funcName][paramData][netId][remote]<endpoint>
		BinSerializer bs;
		__CHECKED( bs.writeStr(funcName) );
		__CHECKED( bs.write16((i16_t)paramDataLen) );
		__CHECKED( bs.write(paramData, paramDataLen) );
		__CHECKED( bs.write(netId) );
		__CHECKED( bs.write(owner!=nullptr) );
		if (owner)
		{
			__CHECKED( bs.write(*owner) );
		}
		sendCreateVariableGroup( bs, nullptr, traceTickets );
		if ( buffer )
		{
			bufferGroup(netId, bs);
		}
	}

	void VariableGroupNode::sendDestroyVariableGroup(u32_t netId)
	{
		BinSerializer bs;
		bs.write(netId);
		m_ZNode->sendReliableOrdered( (u8_t)EDataPacketType::VariableGroupDestroy, bs.data(), bs.length(), nullptr, false, VGChannel, true, true );
		return;
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
		ZEndpoint ztp = Util::toZpt(etp);
		m_ZNode->sendReliableOrdered((u8_t)EDataPacketType::IdPackProvide,
									(const i8_t*)idPack, sizeof(u32_t)*numIds,
									&ztp, false, 0, false, true);
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
				m_ZNode->sendUnreliableSequenced((u8_t)EDataPacketType::IdPackRequest, nullptr, 0, nullptr, false, 0, false, true);
				Platform::log("Sending IdPack request...");
			}
		}
	}

	void VariableGroupNode::resolvePendingGroups()
	{
		// If is id provider, simulate the same behaviour as for requesters by pushing id's in the IdDeck
		if ( m_IsNetworkIdProvider )
		{
			for (i32_t i=0; i<(i32_t)m_PendingGroups.size(); i++)
			{
				m_UniqueIds.emplace_back( m_UniqueIdCounter++ );
			}
		}
		// If has pending groups to be resolved and unique network wide id's are availalbe, resolve them!
		while (!m_PendingGroups.empty() && !m_UniqueIds.empty())
		{
			auto& pvg   = m_PendingGroups.front();
			u32_t netId = m_UniqueIds.front();
			m_UniqueIds.pop_front();
			m_PendingGroups.pop_front();
			i32_t paramDataLen = pvg.paramDataLen();
			std::vector<ZAckTicket> traceTickets;
			sendCreateVariableGroup( pvg.funcName(), pvg.paramData(), paramDataLen, netId, nullptr, true, &traceTickets );
			VariableGroup* vg = callCreateVariableGroup( pvg.funcName(), netId, pvg.paramData(), paramDataLen, nullptr );
			if (!vg) return; // this is critical
			__CHECKED(!traceTickets.empty() && traceTickets[0].traceCallResult == ETraceCallResult::Tracking);
			vg->m_RemoteCreatedTicked = traceTickets[0];
		}
	}

	void VariableGroupNode::sendUpdatedVariableGroups()
	{
		for ( auto vgIt = m_VariableGroups.begin(); vgIt!= m_VariableGroups.end();  )
		{
			VariableGroup* vg = vgIt->second;
			if ( vg->getNetworkId() == -1 || !vg->isRemoteCreated() ) // is the case when no free id's available yet
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
				unBufferGroup( vg->getNetworkId() );
				Util::forEachCallback(m_GroupDestroyCallbacks, [&] (const GroupCallback& gc)
				{
					(gc)(nullptr, vg->getNetworkId());
				});
				vgIt = m_VariableGroups.erase(vgIt);
				delete vg;
			}
			else // group not dirty and not broken, skip
			{
				vgIt++;
			}
		}
	}

	void VariableGroupNode::sendAllVariableCreateEventsTo(const ZEndpoint& to)
	{
		BinSerializer bs;
		for ( auto& bg : m_BufferedGroups )
		{
			bs.resetTo( bg.Data, bg.Len, bg.Len );
			sendCreateVariableGroup( bs, &to );
		}
	}

	VariableGroup* VariableGroupNode::callCreateVariableGroup(const i8_t* name, u32_t id, const i8_t* paramData, i32_t paramDataLen, const ZEndpoint* ztp)
	{
		// append name to prefix
		i8_t fname[RPC_NAME_MAX_LENGTH * 2];
		i8_t* ptNxt = Util::appendString(fname, RPC_NAME_MAX_LENGTH * 2, "__sgp_deserialize_");
		Util::appendString(ptNxt, RPC_NAME_MAX_LENGTH, name);

		// try find function from name
		VariableGroup* lastCreatedGroup = nullptr;
		void* ptrUserCb = Platform::getPtrFromName(fname);
		if (ptrUserCb)
		{
			beginNewGroup( id, ztp );
			lastCreatedGroup = VariableGroup::Last;
			// Call user code for variable group
			void(*userCallback)(ZNode*, const i8_t*, i32_t);
			userCallback = (decltype(userCallback))ptrUserCb;
			userCallback(m_ZNode, paramData, paramDataLen);
			endNewGroup();
		}
		else
		{
			Platform::log("CRITICAL: Serialize group function: %s not found, from: %s, no remote variable group was created!", fname, ZERODELAY_FUNCTION_LINE);
			m_CoreNode->setCriticalError(ECriticalError::CannotFindExternalCFunction, ZERODELAY_FUNCTION_LINE);
		}
		return lastCreatedGroup;
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
		VariableGroup* vg = findGroup( groupId );
		bool bSkip   = vg == nullptr || vg->isBroken();
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

	void VariableGroupNode::bufferGroup(u32_t netId, BinSerializer& bs)
	{
		if (m_CoreNode->isP2P() || m_ZNode->isAuthorative())
		{
			GroupCreateData gcd;
			gcd.netId = netId;
			bs.toRaw(gcd.Data, gcd.Len);
			m_BufferedGroups.emplace_back(gcd);
		}
	}

	void VariableGroupNode::unBufferGroup(u32_t netId)
	{
		if (m_CoreNode->isP2P() || m_ZNode->isAuthorative())
		{
			for (auto it = m_BufferedGroups.begin(); it != m_BufferedGroups.end(); it++)
			{
				GroupCreateData& gcd = *it;
				if (gcd.netId == netId)
				{
					delete[] gcd.Data;
					m_BufferedGroups.erase(it);
					return;
				}
			}
			Platform::log("WARNING: Did not unbuffer any variable groups on destroy while this was expected in %s.", ZERODELAY_FUNCTION);
		}
	}

	VariableGroup* VariableGroupNode::findGroup(u32_t networkId) const
	{
		for (auto& kvp : m_RemoteVariableGroups)
		{
			const std::map<u32_t, VariableGroup*>& group = kvp.second;
			auto it = group.find(networkId);
			if (it != group.end())
			{
				return it->second;
			}
		}

		auto it = m_VariableGroups.find(networkId);
		if (it != m_VariableGroups.end())
		{
			return it->second;
		}

		return nullptr;
	}

	bool VariableGroupNode::removeGroup(u32_t networkId)
	{
		for (auto& kvp : m_RemoteVariableGroups)
		{
			std::map<u32_t, VariableGroup*>& group = kvp.second;
			auto it = group.find(networkId);
			if (it != group.end())
			{
				group.erase(it);
				return true;
			}
		}

		auto it = m_VariableGroups.find(networkId);
		if (it != m_VariableGroups.end())
		{
			m_VariableGroups.erase(it);
			return true;
		}

		Platform::log("WARNING: Did not remove a group with id %d while this was expected.", networkId);
		return false;
	}

}