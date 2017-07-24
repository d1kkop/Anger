#include "RUDPConnection.h"
#include "Socket.h"
#include "RecvPoint.h"

#include <algorithm>
#include <cassert>


namespace Zerodelay
{
	RUDPConnection::RUDPConnection(const EndPoint& endPoint):
		/* packet loss */
		m_PacketLossPercentage(0)
	{
		m_EndPoint = endPoint;
		m_SendSeq_reliable_newest = 0;
		m_RecvSeq_reliable_newest = -1; // the first expected seq is 0, so the previous is -1, as it starts sending this immediately, make sure it is recognized as older initially (-1).
		m_RecvSeq_reliable_newest_ack = -1; // first expected new recved ack is 0, it will send: 'm_RecvSeq_reliable_newest', which is also -1 in the beginning, unless at least a single transmission is done
		for (i32_t i=0; i<sm_NumChannels; ++i)
		{
			m_SendSeq_reliable[i] = 0;
			m_SendSeq_unreliable[i] = 0;
			m_RecvSeq_reliable_recvThread[i] = 0;
			m_RecvSeq_reliable_gameThread[i] = 0;
		}
	}

	RUDPConnection::~RUDPConnection()
	{
		for ( auto& it : m_SendQueue_unreliable ) delete [] it.data;
		for ( i32_t i=0; i<sm_NumChannels; ++i ) for (auto& it : m_SendQueue_reliable[i] ) delete [] it.data;
		for ( i32_t i=0; i<sm_NumChannels; ++i ) for (auto& it : m_RecvQueue_reliable_order[i] ) delete [] it.second.data;
		for ( i32_t i=0; i<sm_NumChannels; ++i ) for (auto& it : m_RecvQueue_unreliable_sequenced[i] ) delete [] it.data;
		for ( auto& kvp : m_SendQueue_reliable_newest ) for (auto i = 0; i < 16 ; i++) delete [] kvp.second.groupItems[i].data;
	}

	void RUDPConnection::addToSendQueue(u8_t id, const i8_t* data, i32_t len, EHeaderPacketType packetType, u8_t channel, bool relay)
	{
		// user not allowed to send acks
		if ( packetType == EHeaderPacketType::Ack || packetType == EHeaderPacketType::Reliable_Newest )
		{
			assert( false && "Invalid packet type" );
			return;
		}
		Packet pack;
		assembleNormalPacket( pack, packetType, id, data, len, off_Norm_Data, channel, relay );
		if ( packetType == EHeaderPacketType::Reliable_Ordered )
		{
			std::lock_guard<std::mutex> lock(m_SendMutex);
			*(u32_t*)&pack.data[off_Norm_Seq] = m_SendSeq_reliable[channel]++;
			m_SendQueue_reliable[channel].emplace_back( pack );
		}
		else
		{
			std::lock_guard<std::mutex> lock(m_SendMutex);
			*(u32_t*)&pack.data[off_Norm_Seq] = m_SendSeq_unreliable[channel]++;
			m_SendQueue_unreliable.emplace_back( pack );
		}
	}

	void RUDPConnection::addReliableNewest(u8_t id, const i8_t* data, i32_t len, u32_t groupId, i8_t groupBit)
	{
		assert( groupBit >= 0 && groupBit < 16 );
		std::lock_guard<std::mutex> lock(m_SendMutex);
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
			for ( auto i=0; i<16; i++ )
			{
				// initialize items in group
				rd.groupItems[i].data = nullptr;
				rd.groupItems[i].dataLen = rd.groupItems[i].dataCapacity = 0;
				rd.groupItems[i].localRevision  = m_SendSeq_reliable_newest-1; // make sure both are on curr-1, so that they will not be seen as changed the first time
				rd.groupItems[i].remoteRevision = m_SendSeq_reliable_newest-1;
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

	void RUDPConnection::beginPoll()
	{
		return m_RecvMutex.lock();
	}

	bool RUDPConnection::poll(Packet& pack)
	{
		// try reliable ordered packets
		for (i32_t i=0; i<sm_NumChannels; ++i)
		{
			auto& queue = m_RecvQueue_reliable_order[i];
			if ( !queue.empty() )
			{
				auto& it = queue.find( m_RecvSeq_reliable_gameThread[i] );
				if ( it != queue.end() )
				{
					pack = it->second;
					queue.erase( it );
					m_RecvSeq_reliable_gameThread[i]++;
					return true;
				}
			}
		}
		// try unreliable sequenced packets
		for (i32_t i=0; i<sm_NumChannels; i++)
		{
			auto& queue = m_RecvQueue_unreliable_sequenced[i];
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

	void RUDPConnection::endPoll()
	{
		m_RecvMutex.unlock();
	}

	bool RUDPConnection::areAllReliableSendQueuesEmpty() const
	{
		std::unique_lock<std::mutex> lock(m_SendMutex);
		for ( i32_t i=0; i<sm_NumChannels; ++i )
		{
			if ( !m_SendQueue_reliable[i].empty() )
			{
				return false;
			}
		}
		return true;
	}

	void RUDPConnection::simulatePacketLoss(u8_t percentage)
	{
		m_PacketLossPercentage = percentage;
	}

	void RUDPConnection::flushSendQueue(ISocket* socket)
	{
		dispatchSendQueue(socket);
		dispatchAckQueue(socket);
		dispatchRelNewestAckQueue(socket);
	}

	void RUDPConnection::recvData(const i8_t* buff, i32_t rawSize)
	{
		if ( m_PacketLossPercentage > 0 && (u8_t)(rand() % 100) < m_PacketLossPercentage )
			return; // discard

		EHeaderPacketType type = (EHeaderPacketType)buff[off_Type];
		switch ( type )
		{
		case EHeaderPacketType::Ack:
			receiveAck(buff, rawSize);
			break;

		case EHeaderPacketType::Ack_Reliable_Newest:
			receiveAckRelNewest(buff, rawSize);
			break;

		case EHeaderPacketType::Reliable_Ordered:
			// ack it (even if we already processed this packet)
			addAckToAckQueue( buff[off_Norm_Chan] & 7, *(u32_t*)&buff[off_Norm_Seq] );
			receiveReliableOrdered(buff, rawSize);	
			break;

		case EHeaderPacketType::Unreliable_Sequenced:
			receiveUnreliableSequenced(buff, rawSize);
			break;

		case EHeaderPacketType::Reliable_Newest:
			receiveReliableNewest( buff, rawSize );
			break;
		}
	}

	void RUDPConnection::addAckToAckQueue(i8_t channel, u32_t seq)
	{
		std::lock_guard<std::mutex> lock(m_AckMutex);
		auto it = std::find( m_AckQueue[channel].begin(), m_AckQueue[channel].end(), seq );
		if ( it == m_AckQueue[channel].end() )
		{
			m_AckQueue[channel].emplace_back( seq );
		}
	}

	void RUDPConnection::dispatchSendQueue(ISocket* socket)
	{
		std::unique_lock<std::mutex> lock(m_SendMutex);
		dispatchReliableQueue(socket);
		dispatchUnreliableQueue(socket);
		dispatchReliableNewestQueue(socket);
	}

	void RUDPConnection::dispatchReliableQueue(ISocket* socket)
	{
		for (i32_t i = 0; i < sm_NumChannels; ++i)
		{
			for (auto& it : m_SendQueue_reliable[i])
			{
				auto& pack = it;
				// reliable pack.data is deleted when it gets acked
				socket->send(m_EndPoint, pack.data, pack.len);
			}
		}
	}

	void RUDPConnection::dispatchUnreliableQueue(ISocket* socket)
	{
		for (auto& it : m_SendQueue_unreliable)
		{
			auto& pack = it;
			socket->send(m_EndPoint, pack.data, pack.len);
			delete[] pack.data;
		}
		m_SendQueue_unreliable.clear(); // clear unreliable queue immediately
	}

	void RUDPConnection::dispatchReliableNewestQueue(ISocket* socket)
	{
		// format per entry: groupId(4bytes) | groupBits( (items.size()+7)/8 bytes ) | n x groupData (sum( item_data_size, n ) bytes )
		i8_t dataBuffer[RecvPoint::sm_MaxRecvBuffSize]; // decl buffer
		dataBuffer[off_Type] = (i8_t)EHeaderPacketType::Reliable_Newest; // start with type
		*(u32_t*)(dataBuffer + off_RelNew_Seq) = m_SendSeq_reliable_newest; // followed by sequence number
		i32_t kNumGroupsWritten = 0;  // keeps track of num groups as is not know yet
		i32_t kBytesWritten = off_RelNew_GroupId; // skip 4 bytes, as num groups is not yet known
		for (auto& kvp : m_SendQueue_reliable_newest)
		{
			i32_t kBytesWrittenBeforeGroup = kBytesWritten; // remember this in case the group has no changed items
			*(u32_t*)(dataBuffer + kBytesWritten) = kvp.first; // is groupId (4 bytes)
			kBytesWritten += 4; // group id byte size
			i8_t* groupBitsPtr = dataBuffer + kBytesWritten; // remember this position as the groupBits cannot be written yet
			kBytesWritten += 2;
			i8_t* groupSkipPtr = dataBuffer + kBytesWritten; // keep store amount of bytes to skip in case on the recipient the group does not exist
			kBytesWritten += 2;
			u16_t groupBits = 0;	// keeps track of which variables in the group are written
			i32_t kBit = 0;			// which bit to set (if variable is written)
			bool itemsWereWritten = false; // see if at least a single item is written
			for (auto& it : kvp.second.groupItems)
			{
				reliableNewestItem& item = it;
				if (isSequenceNewer(item.localRevision, item.remoteRevision)) // variable is changed compared to last acked revision
				{
					groupBits |= (1 << kBit);
					assert(kBytesWritten + item.dataLen <= RecvPoint::sm_MaxRecvBuffSize); // TODO emit error and disconnect , this is critical!
					Platform::memCpy(dataBuffer + kBytesWritten, item.dataLen, item.data, item.dataLen);
					kBytesWritten += item.dataLen;
					itemsWereWritten = true;
				}
				kBit++;
			}
			// if no items in group written, move write ptr back and continue
			if ( !itemsWereWritten )
			{
				kBytesWritten = kBytesWrittenBeforeGroup;
				continue;
			}
			// group bits are now known, write them
			*(u16_t*)(groupBitsPtr) = groupBits;
			kNumGroupsWritten++;
			// write skip bytes in case recipient does not know the group yet
			*(u16_t*)(groupSkipPtr) = (kBytesWritten - kBytesWrittenBeforeGroup) - 8; /* hdr size */
			// quit loop if exceeding max send size
			if ( kBytesWritten >= RecvPoint::sm_MaxSendSize )
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

	void RUDPConnection::dispatchAckQueue(ISocket* socket)
	{
		std::lock_guard<std::mutex> lock(m_AckMutex);
		for (i32_t i=0; i<sm_NumChannels; ++i)
		{
			i8_t buff[RecvPoint::sm_MaxRecvBuffSize];
			i32_t  kSizeWritten = 0;
			for (auto& it : m_AckQueue[i])
			{
				u32_t seq = it;
				*(u32_t*)&buff[kSizeWritten + off_Ack_Payload] = seq;
				kSizeWritten += 4;
				if (kSizeWritten >= RecvPoint::sm_MaxSendSize)
					break;
			}
			m_AckQueue[i].clear();
			if (kSizeWritten > 0) // only transmit acks if there was still something in the queue
			{
				buff[off_Type] = (i8_t)EHeaderPacketType::Ack;
				buff[off_Ack_Chan] = (i8_t)i; // channel
				*(u32_t*)&buff[off_Ack_Num] = kSizeWritten / 4; // num of acks
				socket->send(m_EndPoint, buff, kSizeWritten + off_Ack_Payload); // <-- this is correct, ack_seq is payload offset (if no dataId attached)
			}
		}
	}

	void RUDPConnection::dispatchRelNewestAckQueue(ISocket* socket)
	{
		// stop acking if either, remote disconnected, or we disconnected ourselves
		if ( isPendingDelete() || isDisconnectInvokedHere() || !isConnected() )
			return;
		i8_t buff[8];
		buff[off_Type] = (i8_t)EHeaderPacketType::Ack_Reliable_Newest;
		*(u32_t*)&buff[off_Ack_RelNew_Seq] = m_RecvSeq_reliable_newest;
		socket->send(m_EndPoint, buff, 5);
	}

	void RUDPConnection::receiveReliableOrdered(const i8_t * buff, i32_t rawSize)
	{
		i8_t channel;
		bool relay;
		u32_t seq;
		extractChannelRelayAndSeq( buff, rawSize, channel, relay, seq );

		auto& recvSeq = m_RecvSeq_reliable_recvThread[channel];
		if ( !isSequenceNewer(seq, recvSeq) )
			return;

		std::lock_guard<std::mutex> lock(m_RecvMutex);
		// only insert if received data is not already stored, but waiting to be processed as is out of order
		auto& queue = m_RecvQueue_reliable_order[channel];
		if ( queue.count( seq ) == 0 )
		{
			Packet pack; // Offset off_Norm_Id is correct data packet type (EDataPacketType) (not EHeaderPacketType!) is included in the data
			createNormalPacket( pack, buff + off_Norm_Id, rawSize-off_Norm_Id, channel, relay, EHeaderPacketType::Reliable_Ordered );
			queue.insert( std::make_pair(seq, pack) );
		}
		// update recv seq to most recent possible
		while ( queue.count( recvSeq ) != 0 )
		{
			recvSeq++;
		}
	}

	void RUDPConnection::receiveUnreliableSequenced(const i8_t * buff, i32_t rawSize)
	{
		i8_t channel;
		bool relay;
		u32_t seq;
		extractChannelRelayAndSeq( buff, rawSize, channel, relay, seq );
		
		if ( !isSequenceNewer(seq, m_RecvSeq_unreliable[channel]) )
			return;
	
		// In case of unreliable, immediately update the expected sequenced to the received seq.
		// Therefore, unsequenced but arrived packets, will be discarded!
		m_RecvSeq_unreliable[channel] = seq+1;

		Packet pack;  // Id offset of Norm_ID is correct as id is enclosed in payload/data
		createNormalPacket( pack, buff + off_Norm_Id, rawSize-off_Norm_Id, channel, relay, EHeaderPacketType::Unreliable_Sequenced );

		std::lock_guard<std::mutex> lock(m_RecvMutex);
		m_RecvQueue_unreliable_sequenced[channel].emplace_back( pack );
	}

	void RUDPConnection::receiveReliableNewest(const i8_t* buff, i32_t rawSize)
	{
		if ( rawSize < off_RelNew_Data ) // weak check to see if at least hdr size is available
		{
			// TODO emit warning, invalid reliableNewest data
			return;
		}

		// If sequence is older than already received, discarda all info
		u32_t sendSequence = *(u32_t*)(buff + off_RelNew_Seq);
		if ( !isSequenceNewer(sendSequence, m_RecvSeq_reliable_newest) )
			return;
		
		m_RecvSeq_reliable_newest = sendSequence; // update to what we received
		i32_t kNumGroups = *(i32_t*)(buff + off_RelNew_Num); // num of groups in pack

		// do further processing of individual groups in higher level group unwrap system
		Packet pack;
		createPacketReliableNewest( pack, buff + off_RelNew_Num, rawSize - off_RelNew_Num, kNumGroups );

		std::lock_guard<std::mutex> lock(m_RecvMutex);
		m_RecvQueue_reliable_newest.emplace_back( pack );
	}

	void RUDPConnection::receiveAck(const i8_t * buff, i32_t rawSize)
	{
		i8_t channel = buff[off_Ack_Chan];
		i32_t num = *(i32_t*)(buff + off_Ack_Num); // num of acks
		std::lock_guard<std::mutex> lock(m_SendMutex);
		auto& queue = m_SendQueue_reliable[channel];
		for (i32_t i = 0; i < num; ++i) // for each ack, try to find it, and remove as was succesfully transmitted
		{
			// remove from send queue if we receive an ack for the packet
			u32_t seq = *(u32_t*)(buff + (i*4) + off_Ack_Payload);
			auto it = std::find_if(queue.begin(), queue.end(), [seq](auto& pack)
			{
				return *(u32_t*)&pack.data[off_Norm_Seq] == seq;
			});
			if (it != queue.end())
			{
				auto& pack = (*it);
				delete [] pack.data;
				queue.erase(it);
			}
		}
	}

	void RUDPConnection::receiveAckRelNewest(const i8_t* buff, i32_t rawSize)
	{
		u32_t ackSeq = *(i32_t*) (buff + off_Ack_RelNew_Seq);
		if ( !isSequenceNewer( ackSeq, m_RecvSeq_reliable_newest_ack ) )
			return; // if sequence is already acked, ignore

		// update to newer ack
		m_RecvSeq_reliable_newest_ack = ackSeq;

		std::lock_guard<std::mutex> lock(m_SendMutex);
		auto& queue = m_SendQueue_reliable_newest;
		// For all groups in queue, update all items per group to received ack.
		// If not a single localRevision is newer than the Remote, remote the group from the list.
		for ( auto it = queue.begin(); it != queue.end(); )
		{
			auto& group = it->second;
			bool removeGroup = true;
			for (auto k=0; k<16; ++k)
			{
				auto& item = group.groupItems[k];
				item.remoteRevision = ackSeq;
				if ( removeGroup && isSequenceNewer( item.localRevision, item.remoteRevision ) )
					removeGroup = false; // cannot remove group
			}
			if ( removeGroup )
			{
				for(auto k=0; k<16; ++k) // cleanup data
					delete [] group.groupItems[k].data;
				it = queue.erase(it);
			}
			else
			{
				it++;
			}
		}
	}

	void RUDPConnection::assembleNormalPacket(Packet& pack, EHeaderPacketType packetType, u8_t dataId, const i8_t* data, i32_t len, i32_t hdrSize, i8_t channel, bool relay)
	{
		pack.data = new i8_t[len+hdrSize];
		pack.data[off_Type] = (i8_t)packetType;
		pack.data[off_Norm_Chan] = channel;
		pack.data[off_Norm_Chan] |= ((i8_t)relay) << 3; // skip over the bits for channel, 0 to 7
		pack.data[off_Norm_Id] = dataId;
		Platform::memCpy( pack.data + off_Norm_Data, len, data, len ); 
		pack.len = len + off_Norm_Data;
	}

	void RUDPConnection::extractChannelRelayAndSeq(const i8_t* buff, i32_t rawSize, i8_t& channel, bool& relay, u32_t& seq)
	{
		channel = (buff[off_Norm_Chan] & 7);
		relay   = (buff[off_Norm_Chan] & 8) != 0;
		seq		= *(u32_t*)(buff + off_Norm_Seq);
	}

	void RUDPConnection::createNormalPacket(Packet& pack, const i8_t* buff, i32_t dataSize, i8_t channel, bool relay, EHeaderPacketType type) const
	{
		pack.len  = dataSize;
		pack.data = new i8_t[pack.len];
		pack.channel = channel;
		pack.groupBits = 0;
		pack.numGroups = 0;
		pack.relay = relay;
		pack.type  = type;
		Platform::memCpy(pack.data, pack.len, buff, pack.len); 
	}

	void RUDPConnection::createPacketReliableNewest(Packet& pack, const i8_t* buff, i32_t embeddedGroupsSize, i32_t numGroups) const
	{
		pack.len  = embeddedGroupsSize;
		pack.data = new i8_t[pack.len];
		pack.channel = 0;
		pack.numGroups = numGroups;
		pack.relay = false;
		pack.type  = EHeaderPacketType::Ack_Reliable_Newest;
		Platform::memCpy(pack.data, pack.len, buff, pack.len); 
	}

	bool RUDPConnection::isSequenceNewer(u32_t incoming, u32_t having) const
	{
		incoming -= having;
		return incoming <= (UINT_MAX>>1);
	}
}