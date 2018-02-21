#include "RUDPLink.h"
#include "Socket.h"
#include "RecvNode.h"
#include "Util.h"
#include "CoreNode.h"

#include <algorithm>
#include <cassert>


namespace Zerodelay
{
	RUDPLink::RUDPLink(RecvNode* recvNode, const EndPoint& endPoint, u32_t linkId):
		m_RecvNode(recvNode),
		m_LinkId(linkId),
		m_EndPoint(endPoint),
		m_BlockNewSends(false),
		m_RetransmitWaitingTime(0),
		m_PacketLossPercentage(0),
		m_FragmentSize(ZERODELAY_INITALFRAGSIZE),
		m_IsPendingDelete(false),
		m_MarkDeleteTS(0)
	{
		m_SendSeq_reliable_newest = 0;
		m_RecvSeq_reliable_newest_recvThread = 0;
		m_RecvSeq_reliable_newest_sendThread = 1; // set to 1 to avoid immediate sending an ack as, (0 >= 0) == true, so is newer
		m_RecvSeq_reliable_newest_ack = 0;
		m_RecvSeq_reliable_newest_recvThread = 0;
		for (i32_t i=0; i<sm_NumChannels; ++i)
		{
			m_SendSeq_reliable[i] = 0;
			m_SendSeq_unreliable[i] = 0;
			m_RecvSeq_reliable_recvThread[i] = 0;
			m_RecvSeq_reliable_gameThread[i] = 0;
		}
	}

	RUDPLink::~RUDPLink()
	{
		for (auto & queue : m_RetransmitQueue_reliable) for (auto& pack : queue) delete [] pack.data;
		for (auto & queue : m_RecvQueue_reliable_order) for (auto& seqPacketPair : queue) delete [] seqPacketPair.second.first.data;
		for (auto & queue : m_RecvQueue_unreliable_sequenced ) for (auto& pack : queue) delete [] pack.data;
		for (auto & groupIdGroupPair : m_SendQueue_reliable_newest ) for (auto & groupItem : groupIdGroupPair.second.groupItems) delete [] groupItem.data;
		for (auto & buffer : m_Ureliable_fragments) for (auto& pair : buffer) delete [] pair.first.data;
		for (auto & buffer : m_Reliable_fragments) for (auto& pair : buffer) delete [] pair.second.data;
	}

	ESendCallResult RUDPLink::addToSendQueue(u8_t id, const i8_t* data, i32_t len, EHeaderPacketType packetType, u8_t channel, bool relay,
											 u32_t* sequence, u32_t* numFragments)
	{
		if ( m_BlockNewSends ) // discard new packets in this case
		{
			Platform::log("WARNING: Trying to send id %d with sendType %d while send is blocked.", id, (u32_t)packetType);
			return ESendCallResult::NotSent;
		}
		// user not allowed to send acks
		if ( packetType == EHeaderPacketType::Ack || packetType == EHeaderPacketType::Reliable_Newest )
		{
			assert( false && "Invalid packet type" );
			m_RecvNode->getCoreNode()->setCriticalError( ECriticalError::InvalidLogic, ZERODELAY_FUNCTION_LINE );
			return ESendCallResult::InternalError;
		}
		std::vector<Packet> packs;
		serializeNormalPacket( packs, m_LinkId, packetType, id, data, len, m_FragmentSize, channel, relay );
		if ( packetType == EHeaderPacketType::Reliable_Ordered )
		{
			// add to resend queue (reliable)
			{
				std::lock_guard<std::mutex> lock(m_ReliableOrderedQueueMutex);
				if (sequence)
				{
					*sequence = m_SendSeq_reliable[channel];
					*numFragments = (u32_t)packs.size();
				}
				for (auto& fragment : packs)
				{
					*(u32_t*)&fragment.data[off_Norm_Seq] = m_SendSeq_reliable[channel]++;
					m_RetransmitQueue_reliable[channel].emplace_back( fragment );
				}
			}
			// immediate send after adding to resend queue as we need the sequence printed in the data
			for (auto& fragment : packs)
			{
				m_RecvNode->getSocket()->send( m_EndPoint, fragment.data, fragment.len );
			}
		}
		else
		{
			for (auto& fragment : packs)
			{
				*(u32_t*)&fragment.data[off_Norm_Seq] = m_SendSeq_unreliable[channel]++;
				m_RecvNode->getSocket()->send( m_EndPoint, fragment.data, fragment.len );
			}
		}
		return ESendCallResult::Succes;
	}

	void RUDPLink::addReliableNewest(u8_t id, const i8_t* data, i32_t len, u32_t groupId, i8_t groupBit)
	{
		if ( m_BlockNewSends ) // discard new packets
		{
			Platform::log("WARNING: Trying to add Reliable Newest while send is blocked.");
			return; 
		}
		assert( groupBit >= 0 && groupBit < sm_MaxItemsPerGroup );
		if ( !( groupBit >= 0 && groupBit < sm_MaxItemsPerGroup ) )
		{
			Platform::log("ERROR: GroupBit must be >= 0 and less than %d.", sm_MaxItemsPerGroup);
			m_RecvNode->getCoreNode()->setCriticalError( ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE );
			return;
		}
		std::lock_guard<std::mutex> lock(m_ReliableNewestQueueMutex);
		auto it = m_SendQueue_reliable_newest.find( groupId );
		if ( it == m_SendQueue_reliable_newest.end() )
		{
			// item in group
			reliableNewestItem item;
			item.localRevision  = m_SendSeq_reliable_newest;
			item.remoteRevision = m_SendSeq_reliable_newest-1;
			item.dataLen = len;
			item.dataCapacity = len;
			item.data = new i8_t[len];
			Platform::memCpy(item.data, len, data, len);
			// for the other items, set the correct local/remote revision, so that the group can be popped when an item acked (even for not used items)
			reliableNewestDataGroup rd;
			rd.dataId = id;
			for (auto & groupItem : rd.groupItems)
			{
				// initialize items in group
				groupItem.data = nullptr;
				groupItem.dataLen = groupItem.dataCapacity = 0;
				groupItem.localRevision  = m_SendSeq_reliable_newest-1; // both -1 is correct, the variable that is actually changed (see above) has a diff of -1 between local and remote
				groupItem.remoteRevision = m_SendSeq_reliable_newest-1;
			}
			// copy new item
			Platform::memCpy(&rd.groupItems[groupBit], sizeof(item), &item, sizeof(item));			
			m_SendQueue_reliable_newest.insert( std::make_pair( groupId, rd ) ); // put in map
		}
		else
		{
			reliableNewestDataGroup& rd = it->second;
			reliableNewestItem& item = rd.groupItems[groupBit];
			item.localRevision = m_SendSeq_reliable_newest; // update to what it will be sent with
			if ( item.dataCapacity < len )
			{
				// cannot recycle this data, delete and reallocate
				delete [] item.data;
				item.data = new i8_t[len];
				item.dataCapacity = len;
			}
			Platform::memCpy( item.data, len, data, len );
			item.dataLen = len;
		}
	}

	void RUDPLink::blockAllUpcomingSends()
	{
		m_BlockNewSends = true;
	}

	void RUDPLink::beginPoll()
	{
		return m_RecvQueuesMutex.lock();
	}

	bool RUDPLink::poll(Packet& pack)
	{
		// try reliable ordered packets
		for (i32_t chn=0; chn<sm_NumChannels; ++chn)
		{
			std::map<u32_t, std::pair<Packet, u32_t>>& queue = m_RecvQueue_reliable_order[chn];
			if ( !queue.empty() )
			{
				auto it = queue.find( m_RecvSeq_reliable_gameThread[chn] );
				if ( it != queue.end() )
				{
					pack = it->second.first;
					m_RecvSeq_reliable_gameThread[chn] += it->second.second; // for unfragmented packets 1, numFragments otherwise
					queue.erase( it );
					return true;
				}
			}
		}
		// try unreliable sequenced packets
		for (auto& queue : m_RecvQueue_unreliable_sequenced)
		{
			if ( !queue.empty() )
			{
				pack = queue.front();
				queue.pop_front();
				return true;
			}
		}
		// try reliable newest packets
		if ( !m_RecvQueue_reliable_newest.empty() )
		{
			pack = m_RecvQueue_reliable_newest.front();
			m_RecvQueue_reliable_newest.pop_front();
			return true;
		}
		// no packets
		return false;
	}

	void RUDPLink::endPoll()
	{
		m_RecvQueuesMutex.unlock();
	}

	bool RUDPLink::areAllQueuesEmpty() const
	{
		std::unique_lock<std::mutex> lock(m_ReliableOrderedQueueMutex);
		std::unique_lock<std::mutex> lock2(m_ReliableNewestQueueMutex);
		std::unique_lock<std::mutex> lock3(m_RecvQueuesMutex);
		std::unique_lock<std::mutex> lock4(m_AckMutex);
		for ( i32_t i=0; i<sm_NumChannels; ++i )
		{
			if ( !m_RetransmitQueue_reliable[i].empty() ) return false;
			if ( !m_RecvQueue_unreliable_sequenced[i].empty() ) return false;
			if ( !m_RecvQueue_reliable_order[i].empty() ) return false;
			if ( !m_AckQueue[i].empty() ) return false;
		}
		if ( !m_SendQueue_reliable_newest.empty() ) return false;
		if ( !m_RecvQueue_reliable_newest.empty() ) return false;
		return true;
	}

	bool RUDPLink::isSequenceDelivered(u32_t sequence, i8_t channel) const
	{
		std::lock_guard<std::mutex> lock(m_ReliableOrderedQueueMutex);
		auto& queue = m_RetransmitQueue_reliable[channel];
		auto it = std::find_if(queue.begin(), queue.end(), [&](auto& pack)
		{
			return *(u32_t*)&pack.data[off_Norm_Seq] == sequence;
		});
		bool bDelivered = it == queue.end();
		//if ( bDelivered )
		//{
		//	Platform::log("Delivered seq: %d chan: %d\n", sequence, channel);
		//}
		return bDelivered;
	}

	void RUDPLink::simulatePacketLoss(u8_t percentage)
	{
		m_PacketLossPercentage = percentage;
	}

	i32_t RUDPLink::getTimeSincePendingDelete() const
	{
		return Util::getTimeSince(m_MarkDeleteTS);
	}

	void RUDPLink::markPendingDelete()
	{
		std::lock_guard<std::mutex> lock(m_PendingDeleteMutex);
		if ( m_IsPendingDelete )
			return;
		m_IsPendingDelete = true; 
		m_MarkDeleteTS = ::clock();
	}

	void RUDPLink::pin()
	{
		m_PinnedCount++;
	}

	bool RUDPLink::isPinned() const
	{
		return m_PinnedCount > 0;
	}

	void RUDPLink::unpin()
	{
		m_PinnedCount--;
	}

	// ----------------- Called from send thread -----------------------------------------------

	void RUDPLink::dispatchRelOrderedQueueIfLatencyTimePassed(u32_t deltaTime, ISocket* socket)
	{
		m_RetransmitWaitingTime += deltaTime;
		u32_t latencyTime = (u32_t)( 1.3f*getLatency() );
		if ( m_RetransmitWaitingTime >= latencyTime )
		{
			m_RetransmitWaitingTime -= latencyTime;
			dispatchReliableOrderedQueue(socket);
		}
	}

	void RUDPLink::dispatchReliableOrderedQueue(ISocket* socket)
	{
		std::unique_lock<std::mutex> lock(m_ReliableOrderedQueueMutex);
		for (auto& queue : m_RetransmitQueue_reliable)
		{
			for (auto& pack : queue)
			{
				// reliable pack.data is deleted when it gets acked
				socket->send(m_EndPoint, pack.data, pack.len);
			}
		}
	}

	void RUDPLink::dispatchReliableNewestQueue(ISocket* socket)
	{
		// format per entry: groupId(4bytes) | groupBits( (items.size()+7)/8 bytes ) | n x groupData (sum( item_data_size, n ) bytes )
		i8_t dataBuffer[ZERODELAY_BUFF_RECV_SIZE]; // recv buffer size correct!
		*(u32_t*)dataBuffer  = m_LinkId;
		dataBuffer[off_Type] = (i8_t)EHeaderPacketType::Reliable_Newest; // type after linkId
		*(u32_t*)(dataBuffer + off_RelNew_Seq) = m_SendSeq_reliable_newest; // followed by sequence number
		i32_t kNumGroupsWritten = 0;  // keeps track of num groups as is not know yet
		i32_t kBytesWritten = off_RelNew_GroupId; // skip 4 bytes, as num groups is not yet known
		std::unique_lock<std::mutex> lock(m_ReliableNewestQueueMutex);
		for (auto it = m_SendQueue_reliable_newest.begin(); it != m_SendQueue_reliable_newest.end(); it++)
		{
			auto& kvp = *it;
			i32_t kBytesWrittenBeforeGroup = kBytesWritten; // remember this in case the group has no changed items
			*(u32_t*)(dataBuffer + kBytesWritten) = kvp.first; // is groupId (4 bytes)
			assert( it != m_SendQueue_reliable_newest.begin() || kBytesWritten == off_RelNew_GroupId );
			kBytesWritten += 4; // group id byte size
			i8_t* groupBitsPtr = dataBuffer + kBytesWritten; // remember this position as the groupBits cannot be written yet
			assert( it != m_SendQueue_reliable_newest.begin() || kBytesWritten == off_RelNew_GroupBits );
			kBytesWritten += 2;
			i8_t* groupSkipPtr = dataBuffer + kBytesWritten; // keep store amount of bytes to skip in case on the recipient the group does not exist
			assert( it != m_SendQueue_reliable_newest.begin() || kBytesWritten == off_RelNew_GroupSkipBytes );
			kBytesWritten += 2;
			u16_t groupBits = 0;	// keeps track of which variables in the group are written
			i32_t kBit = 0;			// which bit to set (if variable is written)
			bool itemsWereWritten = false; // see if at least a single item is written
			for (auto& it : kvp.second.groupItems)
			{
				reliableNewestItem& item = it;
				if (isSequenceNewerGroupItem(item.localRevision, item.remoteRevision)) // variable is changed compared to last acked revision
				{
					groupBits |= (1 << kBit);
					assert(kBytesWritten + item.dataLen <= ZERODELAY_BUFF_SIZE); 
					if ( kBytesWritten + item.dataLen > ZERODELAY_BUFF_SIZE )
					{
						Platform::log("CRITICAL: Buffer overrun detected in %s.", ZERODELAY_FUNCTION_LINE );
						m_RecvNode->getCoreNode()->setCriticalError( ECriticalError::TooMuchDataToSend, ZERODELAY_FUNCTION_LINE );
						return;
					}
					Platform::memCpy(dataBuffer + kBytesWritten, item.dataLen, item.data, item.dataLen);
					kBytesWritten += item.dataLen;
					itemsWereWritten = true;
				}
				kBit++;
			}
			// if no items in group written, move write ptr back and continue
			if ( !itemsWereWritten )
			{
				// note, numGroupsWritten is not incremented, no need to undo written data to stream, just rewind write position
				kBytesWritten = kBytesWrittenBeforeGroup;
				continue;
			}
			// group bits are now known, write them
			*(u16_t*)(groupBitsPtr) = groupBits;
			kNumGroupsWritten++;
			// write skip bytes in case recipient does not know the group yet
			*(u16_t*)(groupSkipPtr) = (kBytesWritten - kBytesWrittenBeforeGroup);
			// quit loop if exceeding max send size
			if ( kBytesWritten >= ZERODELAY_BUFF_SIZE )
				break;
		}
		// as group count is know now, write it
		if ( kNumGroupsWritten > 0 )
		{
			*(i32_t*)(dataBuffer + off_RelNew_Num) = kNumGroupsWritten;
			socket->send( m_EndPoint, dataBuffer, kBytesWritten );
			m_SendSeq_reliable_newest++;
		}
	}

	void RUDPLink::dispatchAckQueue(ISocket* socket)
	{
		std::lock_guard<std::mutex> lock(m_AckMutex);
		for(u32_t i=0; i<sm_NumChannels; i++)
		{
			auto& ackQueue = m_AckQueue[i];
			i8_t buff[ZERODELAY_BUFF_RECV_SIZE]; // recv buff size correct
			i32_t  kSizeWritten = 0;
			for (auto& it : ackQueue)
			{
				u32_t seq = it;
				*(u32_t*)&buff[kSizeWritten + off_Ack_Payload] = seq;
				kSizeWritten += 4;
				if (kSizeWritten >= ZERODELAY_BUFF_SIZE) // send when send buff size is exceeded
					break;
			}
			ackQueue.clear();
			if (kSizeWritten > 0) // only transmit acks if there was still something in the queue
			{
				*(u32_t*)buff = m_LinkId;
				buff[off_Type] = (i8_t)EHeaderPacketType::Ack;
				buff[off_Ack_Chan] = (i8_t)i; // channel
				*(u32_t*)&buff[off_Ack_Num] = kSizeWritten / 4; // num of acks
				socket->send(m_EndPoint, buff, kSizeWritten + off_Ack_Payload); // <-- this is correct, ack_seq is payload offset (if no dataId attached)
			}
		}
	}

	void RUDPLink::dispatchRelNewestAckQueue(ISocket* socket)
	{
		// get value from receive thread, if value is changed after load, it will simply send again the next update, 
		// do however not update the send_thread_value to a send load of the recv_thread value
		uint32_t recvThreadAckValue = m_RecvSeq_reliable_newest_recvThread;
		if ( isPendingDelete() || !isSequenceNewer(recvThreadAckValue, m_RecvSeq_reliable_newest_sendThread) )
		{
			return;
		}
		i8_t buff[32];
		buff[off_Type] = (i8_t)EHeaderPacketType::Ack_Reliable_Newest;
		*(u32_t*)&buff[off_Ack_RelNew_Seq] = recvThreadAckValue;
		socket->send(m_EndPoint, buff, 4);
		m_RecvSeq_reliable_newest_sendThread = recvThreadAckValue+1;
	}


	// ----------------- Called from receive thread -----------------------------------------------

	void RUDPLink::recvData(const i8_t* buff, i32_t rawSize)
	{
		if ( m_PacketLossPercentage > 0 && (u8_t)(rand() % 100) < m_PacketLossPercentage )
			return; // discard

		u32_t linkId; 
		EHeaderPacketType type;
		if (!deserializeGenericHdr(buff, rawSize, linkId, type))
			return;

		switch ( type )
		{
		case EHeaderPacketType::Ack:
			receiveAck( buff, rawSize );
			break;

		case EHeaderPacketType::Ack_Reliable_Newest:
			receiveAckRelNewest( buff, rawSize );
			break;

		case EHeaderPacketType::Reliable_Ordered:
			// ack it (even if we already processed this packet)
			addAckToAckQueue( buff[off_Norm_ChanNFlags] & 7, *(u32_t*)&buff[off_Norm_Seq] );
			receiveReliableOrdered( linkId, buff, rawSize );	
			break;

		case EHeaderPacketType::Unreliable_Sequenced:
			receiveUnreliableSequenced( linkId, buff, rawSize );
			break;

		case EHeaderPacketType::Reliable_Newest:
			receiveReliableNewest( linkId, buff, rawSize );
			break;

		default:
			Platform::log("WARNING: Unknown HeaderPacketType received. Packet dropped.");
			break;
		}
	}

	void RUDPLink::addAckToAckQueue(i8_t channel, u32_t seq)
	{
		std::lock_guard<std::mutex> lock(m_AckMutex);
		auto it = std::find( m_AckQueue[channel].begin(), m_AckQueue[channel].end(), seq );
		if ( it == m_AckQueue[channel].end() )
		{
			Platform::log("Acked seq %d", seq);
			m_AckQueue[channel].emplace_back( seq );
		}
	}

	void RUDPLink::receiveReliableOrdered(u32_t linkId, const i8_t * buff, i32_t rawSize)
	{
		i8_t channel;
		bool relay;
		u32_t seq;
		bool firstFragment, lastFragment;
		if ( !deserializeNormalHdr(buff, rawSize, channel, relay, seq, firstFragment, lastFragment) )
		{
			// Critical because packet is already acked while it cannot be reliably handled.
			m_RecvNode->getCoreNode()->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION, ZERODELAY_LINE);
			return;
		}

		auto& recvSeq = m_RecvSeq_reliable_recvThread[channel];
		if ( !isSequenceNewer(seq, recvSeq) )
			return;

		Platform::log("Rcvd seq %d", seq);
		std::lock_guard<std::mutex> lock(m_RecvQueuesMutex);

		auto& queue = m_RecvQueue_reliable_order[channel];
		if ( firstFragment && lastFragment ) // not fragmented
		{
			Platform::log("Rcvd seq %d is NOT fragmented", seq);
			// packet may arrive multiple time (if is not next expected sequence)
		//	std::lock_guard<std::mutex> lock(m_RecvQueuesMutex);
			if ( queue.count( seq ) == 0 )
			{
				Packet pack; // Offset off_Norm_Id is correct data packet type (EDataPacketType) (not EHeaderPacketType!) is included in the data
				createNormalPacket( pack, buff + off_Norm_Id, rawSize-off_Norm_Id, linkId, channel, relay, EHeaderPacketType::Reliable_Ordered );
				queue.insert( std::make_pair(seq, std::make_pair(pack, 1)) );
				while ( queue.count( recvSeq ) != 0 )
				{
					recvSeq++;
				}
			}
		}
		else // fragmented
		{
			Platform::log("Rcvd seq %d IS fragmented", seq);
			auto& fragments = m_Reliable_fragments[channel];
			if ( fragments.count( seq ) == 0 )
			{
				Packet pack; // Offset off_Norm_Id is correct data packet type (EDataPacketType) (not EHeaderPacketType!) is included in the data
				createNormalPacket( pack, buff + off_Norm_Id, rawSize-off_Norm_Id, linkId, channel, relay, EHeaderPacketType::Reliable_Ordered );
				if ( firstFragment ) pack.flags |= (FirstFragmentBit);
				if ( lastFragment )  pack.flags |= (LastFragmentBit);
				fragments.insert( std::make_pair(seq, pack) );
				Platform::log("Rcvd seq %d was inserted", seq);
			}

			// while expected in order sequence is received as a fragment, try to defragment into a single packet
			while ( fragments.count( recvSeq ) != 0 && (fragments[recvSeq].flags & FirstFragmentBit) != 0 )
			{
				bool unfragmentedPacket = false;
				u32_t nxtFragSeq = recvSeq+1;
				while ( fragments.count( nxtFragSeq ) != 0 )
				{
					if ( fragments[nxtFragSeq].flags & LastFragmentBit )
					{
						Packet finalPack;
						unfragmentReliablePacket( finalPack, recvSeq, nxtFragSeq, fragments );

						for ( auto i = recvSeq; i != nxtFragSeq+1; i++ )
						{
							Platform::log("Seq %d was handled", i);
						}

						// insert defragmented packet into 'normal' queue
						{
				//			std::lock_guard<std::mutex> lock(m_RecvQueuesMutex);
							queue.insert( std::make_pair(recvSeq, std::make_pair(finalPack, 1+(nxtFragSeq-recvSeq))) );
							recvSeq = nxtFragSeq+1;
						}

						// break out to next possible range of fragments
						unfragmentedPacket = true;
						break;
					}
					nxtFragSeq++;
				}
				if (!unfragmentedPacket) break; // double break if nothing could be defragmented
			}
		}
	}

	void RUDPLink::receiveUnreliableSequenced(u32_t linkId, const i8_t * buff, i32_t rawSize)
	{
		i8_t channel;
		bool relay;
		u32_t seq;
		bool firstFragment, lastFragment;
		if ( !deserializeNormalHdr(buff, rawSize, channel, relay, seq, firstFragment, lastFragment) )
		{
			Platform::log("WARNING: Serialization error in %s, line %d.", ZERODELAY_FUNCTION, ZERODELAY_LINE );
			return;
		}
		
		if ( !isSequenceNewer(seq, m_RecvSeq_unreliable[channel]) )
			return;
	
		// In case of unreliable, immediately update the expected sequenced to the received seq.
		// Therefore, unsequenced but arrived packets, will be discarded!
		m_RecvSeq_unreliable[channel] = seq+1;

		Packet pack;  // Id offset of Norm_ID is correct as id is enclosed in payload/data
		createNormalPacket( pack, buff + off_Norm_Id, rawSize-off_Norm_Id, linkId, channel, relay, EHeaderPacketType::Unreliable_Sequenced );

		if ( firstFragment && lastFragment )
		{ // not fragmented
			std::lock_guard<std::mutex> lock(m_RecvQueuesMutex);
			m_RecvQueue_unreliable_sequenced[channel].emplace_back( pack );
		}
		else
		{ // unreliable fragmented packet
			auto& fragmentBuffer = m_Ureliable_fragments[channel];
			if ( firstFragment )
			{
				fragmentBuffer.clear();
				fragmentBuffer.emplace_back( std::make_pair(pack, seq) );
			}
			else if (!firstFragment && !fragmentBuffer.empty())
			{
				fragmentBuffer.emplace_back( std::make_pair(pack, seq) );
				if ( lastFragment )
				{
					// validate that all sequences between first and last were received
					bool validRange = true;
					for (i32_t i=1; i<(i32_t)fragmentBuffer.size(); i++)
					{
						u32_t prevFragSeq = fragmentBuffer[i-1].second;
						u32_t fragSeq = fragmentBuffer[i].second;
						if ( fragSeq - prevFragSeq != 1 )
						{
							validRange = false;
							break;
						}
					}
					if (validRange)
					{
						Packet finalPack;
						unfragmentUnreliablePacket(finalPack, fragmentBuffer);
						// put in queue accessed by other thread
						std::lock_guard<std::mutex> lock(m_RecvQueuesMutex);
						m_RecvQueue_unreliable_sequenced[channel].emplace_back( finalPack );
					}
					// handled or invalid range -> not all fragments received, cannot recover, clear fragment buffer
					fragmentBuffer.clear();
				}
			}
			else
			{
				// invalid state, cannot recover, clear fragment buffer (big unreliable packet lost)
				fragmentBuffer.clear();
			}
		}
	}

	void RUDPLink::receiveReliableNewest(u32_t linkId, const i8_t* buff, i32_t rawSize)
	{
		if ( rawSize < hdr_Relnew_Size )
		{
			Platform::log("WARNING: Invalid reliable newest data, too short. In %s, line %d.", ZERODELAY_FUNCTION, ZERODELAY_LINE);
			return;
		}

		// If sequence is older than already received, discarda all info
		u32_t seq = *(u32_t*)(buff + off_RelNew_Seq);
		if ( !isSequenceNewer(seq, m_RecvSeq_reliable_newest_recvThread) )
			return;
		
		// from now on only interested in current sequence +1
		m_RecvSeq_reliable_newest_recvThread = seq+1;					

		// do further processing of individual groups in higher level group unwrap system
		Packet pack;
		createNormalPacket( pack, buff + off_RelNew_Num, rawSize - off_RelNew_Num, linkId, 0, false, EHeaderPacketType::Reliable_Newest );

		std::lock_guard<std::mutex> lock(m_RecvQueuesMutex);
		m_RecvQueue_reliable_newest.emplace_back( pack );
	}

	void RUDPLink::receiveAck(const i8_t * buff, i32_t rawSize)
	{
		if (rawSize < hdr_Ack_Size)
		{
			Platform::log("WARNING: Invalid ack size detected in %s, line %d.", ZERODELAY_FUNCTION, ZERODELAY_LINE);
			return;
		}
		i8_t channel = buff[off_Ack_Chan];
		i32_t num	 = *(i32_t*)(buff + off_Ack_Num); // num of acks
		if (rawSize - (hdr_Ack_Size+hdr_Generic_Size) != num*4)
		{
			Platform::log("WARNING: Invalid ack payload detected in %s, line %d.", ZERODELAY_FUNCTION, ZERODELAY_LINE);
			return;
		}
		std::lock_guard<std::mutex> lock(m_ReliableOrderedQueueMutex);
		auto& queue = m_RetransmitQueue_reliable[channel];
		for (i32_t i = 0; i < num; ++i) // for each ack, try to find it, and remove as was succesfully transmitted
		{
			// remove from send queue if we receive an ack for the packet
			u32_t seq = *(u32_t*)(buff + (i*4) + off_Ack_Payload);
			auto it   = std::find_if(queue.begin(), queue.end(), [seq](auto& pack)
			{
				return *(u32_t*)&pack.data[off_Norm_Seq] == seq;
			});
			if (it != queue.end())
			{
		//		Platform::log("Packet with seq: %d chan %d, acked.", seq, channel);
				auto& pack = (*it);
				delete [] pack.data;
				queue.erase(it);
			}
		}
	}

	void RUDPLink::receiveAckRelNewest(const i8_t* buff, i32_t rawSize)
	{
		if (rawSize < hdr_Ack_RelNew_Size)
		{
			Platform::log("WARNING: Invalid reliable newest ack size detected in %s, line %d.", ZERODELAY_FUNCTION, ZERODELAY_LINE);
			return;
		}

		u32_t ackSeq = *(i32_t*) (buff + off_Ack_RelNew_Seq);
		if ( !isSequenceNewer( ackSeq, m_RecvSeq_reliable_newest_ack ) )
			return; // if sequence is already acked, ignore

		// update to newer ack + 1;
		m_RecvSeq_reliable_newest_ack = ackSeq+1;

		std::lock_guard<std::mutex> lock(m_ReliableOrderedQueueMutex);
		auto& queue = m_SendQueue_reliable_newest;
		// For all groups in queue, update all items per group to received ack.
		// If not a single localRevision is newer than the Remote, remove the group from the list.
		for ( auto it = queue.begin(); it != queue.end(); )
		{
			auto& group = it->second;
			bool removeGroup = true;
			for (auto& item : group.groupItems)
			{
				item.remoteRevision = ackSeq;
				if ( removeGroup && isSequenceNewerGroupItem( item.localRevision, item.remoteRevision ) )
					removeGroup = false; // cannot remove group
			}
			if ( removeGroup )
			{
				for(auto & groupItem : group.groupItems) // cleanup data
					delete [] groupItem.data;
				it = queue.erase(it);
			}
			else
			{
				it++;
			}
		}
	}

	// ----------------- Support functions (does not touch class data) -----------------------------------------------

	void RUDPLink::serializeNormalPacket(std::vector<Packet>& packs, u32_t linkId, EHeaderPacketType packetType, u8_t dataId, const i8_t* data, i32_t len, i32_t fragmentSize, i8_t channel, bool relay)
	{
		bool bStartFragment = true;
		u32_t offset = 0;
		while (len > 0 || bStartFragment) // allow zero length payload packets
		{
			Packet pack;
			u32_t payloadLen = Util::min(len, fragmentSize);
			pack.data = new i8_t[payloadLen+off_Norm_Data];
			*(u32_t*)(pack.data + off_Link) = linkId;
			pack.data[off_Type] = (i8_t)packetType;
			pack.data[off_Norm_ChanNFlags] = channel;
			pack.data[off_Norm_ChanNFlags] |= ((i8_t)relay) << 3; // skip over the bits for channel, 0 to 7
			pack.data[off_Norm_ChanNFlags] |= ((i8_t)bStartFragment) << 4; // first fragment bit
			pack.data[off_Norm_Id] = dataId;
			Platform::memCpy(pack.data + off_Norm_Data, payloadLen, data + offset, payloadLen);
			pack.len = payloadLen + off_Norm_Data;
			packs.emplace_back(pack);
			// prepare next fragment
			bStartFragment = false;
			len -= payloadLen;
			offset += payloadLen;
		}
		// set last fragment bit in last packet
		packs.back().data[off_Norm_ChanNFlags] |= ((i8_t)1) << 5; // last fragment bit
	}

	bool RUDPLink::deserializeGenericHdr(const i8_t* buff, i32_t rawSize, u32_t& linkIdOut, EHeaderPacketType& packetType)
	{
		if ( rawSize < hdr_Generic_Size ) return false;
		linkIdOut  = *(u32_t*)(buff + off_Link);
		packetType = (EHeaderPacketType)*(buff + off_Type);
		return true;
	}

	bool RUDPLink::deserializeNormalHdr(const i8_t* buff, i32_t rawSize, i8_t& channel, bool& relay, u32_t& seq, bool& firstFragment, bool& lastFragment)
	{
		if ( rawSize <= hdr_Norm_Size ) return false; 
		channel = (buff[off_Norm_ChanNFlags] & 7);		// first 3 bits [0 - 2]
		relay   = (buff[off_Norm_ChanNFlags] & 8) != 0;	// 3rth bit
		firstFragment = (buff[off_Norm_ChanNFlags] & 16) != 0; // 4th bit
		lastFragment  = (buff[off_Norm_ChanNFlags] & 32) != 0; // 5th bit
		seq		= *(u32_t*)(buff + off_Norm_Seq);
		return true;
	}

	void RUDPLink::createNormalPacket(Packet& pack, const i8_t* buff, i32_t dataSize, u32_t linkId, i8_t channel, 
									  bool relay, EHeaderPacketType type)
	{
		pack.linkId = linkId;
		pack.len  = dataSize;
		pack.data = new i8_t[pack.len];
		pack.channel = channel;
		pack.type  = type;
		pack.flags = relay;
		Platform::memCpy(pack.data, pack.len, buff, pack.len); 
	}

	void RUDPLink::unfragmentUnreliablePacket(Packet& pack, const std::vector<std::pair<Packet, u32_t>>& fragments)
	{
		// copy id, type etc from first fragment
		pack = fragments.front().first;
		u32_t len = 0;
		for (auto& pair : fragments)
		{
			len += pair.first.len-1; // length includes data hdr id
		}
		pack.data = new i8_t[len+1];
		pack.len  = len+1;
		pack.data[0] = fragments.front().first.data[0]; // data hdr id
		u32_t curLen = 1;
		for (auto& pair : fragments)
		{
			const Packet& frag = pair.first;
			u32_t copySize = frag.len-1; // subtract data id (-1)
			Platform::memCpy(pack.data + curLen, (len+1-curLen), frag.data, copySize);
			delete [] frag.data;
			curLen += copySize;
		}
	}

	void RUDPLink::unfragmentReliablePacket(Packet& pack, u32_t beginSeq, u32_t lastSeq, std::map<u32_t, Packet>& fragments)
	{
		// copy id, type etc from first fragment
		pack = fragments.begin()->second;
		u32_t len = 0;
		u32_t curSeq = beginSeq;
		while (curSeq != lastSeq+1) // take into account that seq wraps
		{
			len += fragments[curSeq].len - 1; // length includes data hdr id
			curSeq++;
		}
		pack.data = new i8_t[len+1]; // + 1 for data hdr id
		pack.len  = len+1;
		pack.data[0] = fragments.begin()->second.data[0]; // data hdr id
		u32_t curLen = 1;
		curSeq = beginSeq;
		while (curSeq != lastSeq+1) // take into account that seq wraps
		{
			auto fragIt = fragments.find(curSeq);
			const Packet& frag = fragIt->second;
			u32_t copySize = frag.len-1; // subtract data id (-1)
			Platform::memCpy(pack.data + curLen, (len+1-curLen), frag.data+1, copySize);
			delete [] frag.data;
			fragments.erase(fragIt);
			curLen += copySize;
			curSeq++;
		}
		// zero out the first/last fragment bit in the flags for cleanness (upper layer is not interested in this)
		pack.flags &= ~FirstFragmentBit;
		pack.flags &= ~LastFragmentBit;
	}

	bool RUDPLink::isSequenceNewer(u32_t incoming, u32_t having)
	{
		return (incoming >= having && (incoming - having) <= (UINT_MAX>>1)) || 
			   (incoming < having && (having - incoming) > (UINT_MAX>>1));
	}

	bool RUDPLink::isSequenceNewerGroupItem(u32_t incoming, u32_t having)
	{
		return (incoming > having && (incoming - having) <= (UINT_MAX>>1)) || 
			   (incoming < having && (having - incoming) > (UINT_MAX>>1));
	}
}