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
		for (int i=0; i<sm_NumChannels; ++i)
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
		for ( int i=0; i<sm_NumChannels; ++i ) for (auto& it : m_SendQueue_reliable[i] ) delete [] it.data;
		for ( int i=0; i<sm_NumChannels; ++i ) for (auto& it : m_RecvQueue_reliable_order[i] ) delete [] it.second.data;
		for ( int i=0; i<sm_NumChannels; ++i ) for (auto& it : m_RecvQueue_unreliable_sequenced[i] ) delete [] it.data;
	}

	void RUDPConnection::addToSendQueue(unsigned char id, const char* data, int len, EPacketType packetType, unsigned char channel, bool relay)
	{
		// user not allowed to send acks
		if ( packetType == EPacketType::Ack || packetType == EPacketType::Reliable_Newest )
		{
			assert( false && "Invalid packet type" );
			return;
		}
		Packet pack;
		assembleNormalPacket( pack, packetType, id, data, len, off_Norm_Data, channel, relay );
		if ( packetType == EPacketType::Reliable_Ordered )
		{
			std::lock_guard<std::mutex> lock(m_SendMutex);
			*(unsigned int*)&pack.data[off_Norm_Seq] = m_SendSeq_reliable[channel]++;
			m_SendQueue_reliable[channel].emplace_back( pack );
		}
		else
		{
			std::lock_guard<std::mutex> lock(m_SendMutex);
			*(unsigned int*)&pack.data[off_Norm_Seq] = m_SendSeq_unreliable[channel]++;
			m_SendQueue_unreliable.emplace_back( pack );
		}
	}

	void RUDPConnection::addReliableNewest(unsigned char id, const char* data, int len, unsigned int dataId, bool relay)
	{
		Packet pack;
		assembleNormalPacket( pack, EPacketType::Reliable_Newest, id, data, len, off_RelNew_Data, 0, relay );
		*(unsigned int*)(pack.data + off_RelNew_DataId) = dataId;
		std::lock_guard<std::mutex> lock(m_SendMutex);
		auto it = m_SendQueue_reliable_newest.find( dataId );
		if ( it == m_SendQueue_reliable_newest.end() )
		{
			m_SendQueue_reliable_newest.insert( std::make_pair( dataId, std::make_tuple(1, 0, pack) ) );
			pack.data[off_Norm_Seq] = 1;
		}
		else
		{
			std::tuple<unsigned int, unsigned int, Packet>& tuple = m_SendQueue_reliable_newest[dataId];
			delete [] std::get<2>(tuple).data; // TODO change this for packet recycle
			pack.data[off_Norm_Seq] = (++std::get<0>(tuple)); // inc seq numb
			std::get<2>(tuple) = pack;
		}
	}

	void RUDPConnection::beginPoll()
	{
		return m_RecvMutex.lock();
	}

	bool RUDPConnection::poll(Packet& pack)
	{
		// try reliable ordered packets
		for (int i=0; i<sm_NumChannels; ++i)
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
		for (int i=0; i<sm_NumChannels; i++)
		{
			auto& queue = m_RecvQueue_unreliable_sequenced[i];
			if ( !queue.empty() )
			{
				pack = queue.front();
				queue.pop_front();
				return true;
			}
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
		for ( int i=0; i<sm_NumChannels; ++i )
		{
			if ( !m_SendQueue_reliable[i].empty() )
			{
				return false;
			}
		}
		return true;
	}

	void RUDPConnection::simulatePacketLoss(unsigned char percentage)
	{
		m_PacketLossPercentage = percentage;
	}

	void RUDPConnection::flushSendQueue(ISocket* socket)
	{
		dispatchSendQueue(socket);
		dispatchAckQueue(socket);
		dispatchRelNewestAckQueue(socket);
	}

	void RUDPConnection::recvData(const char* buff, int rawSize)
	{
		if ( m_PacketLossPercentage > 0 && (unsigned char)(rand() % 100) < m_PacketLossPercentage )
			return; // discard

		EPacketType type = (EPacketType)buff[off_Type];
		switch ( type )
		{
		case EPacketType::Ack:
			receiveAck(buff, rawSize);
			break;

		case EPacketType::Ack_Reliable_Newest:
			receiveAckRelNewest(buff, rawSize);
			break;

		case EPacketType::Reliable_Ordered:
			// ack it (even if we already processed this packet)
			addAckToAckQueue( buff[off_Norm_Chan] & 7, *(unsigned int*)&buff[off_Norm_Seq] );
			receiveReliableOrdered(buff, rawSize);	
			break;

		case EPacketType::Unreliable_Sequenced:
			receiveUnreliableSequenced(buff, rawSize);
			break;

		case EPacketType::Reliable_Newest:
			{
				unsigned int seq, dataId;
				receiveReliableNewest(buff, rawSize, seq, dataId );
				addAckToRelNewestAckQueue( seq, dataId ); // ack it (even if we already processed this packet)
			}
			break;
		}
	}

	void RUDPConnection::addAckToAckQueue(char channel, unsigned int seq)
	{
		std::lock_guard<std::mutex> lock(m_AckMutex);
		auto it = std::find( m_AckQueue[channel].begin(), m_AckQueue[channel].end(), seq );
		if ( it == m_AckQueue[channel].end() )
		{
			m_AckQueue[channel].emplace_back( seq );
		}
	}

	void RUDPConnection::addAckToRelNewestAckQueue(unsigned int seq, unsigned int dataId)
	{
		std::lock_guard<std::mutex> lock(m_AckRelNewestMutex);
		auto it = std::find_if( m_AckQueueRelNewest.begin(), m_AckQueueRelNewest.end(), [&] (auto& pair) 
		{
			return pair.first == dataId;
		});
		// overwrite seq with newer if dataId is already in list and seq is newer
		if ( it != m_AckQueueRelNewest.end() )
		{
			if ( isSequenceNewer( seq, it->second ) )
			{
				it->second = seq; 
			}
		}
		else
		{
			m_AckQueueRelNewest.emplace_back( std::make_pair( dataId, seq ) );
		}
	}

	void RUDPConnection::dispatchSendQueue(ISocket* socket)
	{
		std::unique_lock<std::mutex> lock(m_SendMutex);
		// reliable
		for ( int i=0; i<sm_NumChannels; ++i )
		{
			for (auto& it : m_SendQueue_reliable[i])
			{
				auto& pack = it;
				// reliable pack.data is deleted when it gets acked
				socket->send(m_EndPoint, pack.data, pack.len);
			}
		}
		// unreliable
		for (auto& it : m_SendQueue_unreliable)
		{
			auto& pack = it;
			socket->send(m_EndPoint, pack.data, pack.len);
			delete [] pack.data;
		}
		m_SendQueue_unreliable.clear(); // clear unreliable queue immediately
		// reliable newest
		for ( auto& kvp : m_SendQueue_reliable_newest )
		{
			auto& tuple = kvp.second;
			if ( isSequenceNewer( std::get<0>(tuple), std::get<1>(tuple) ))
			{
				auto& pack = std::get<2>(tuple);
				socket->send(m_EndPoint, pack.data, pack.len);
			}
		}
	}

	void RUDPConnection::dispatchAckQueue(ISocket* socket)
	{
		std::lock_guard<std::mutex> lock(m_AckMutex);
		for (int i=0; i<sm_NumChannels; ++i)
		{
			char buff[RecvPoint::sm_MaxRecvBuffSize];
			int  kSizeWritten = 0;
			int  maxWriteSize = RecvPoint::sm_MaxRecvBuffSize - 64; // account for hdr

			for (auto& it : m_AckQueue[i])
			{
				unsigned int seq = it;
				*(unsigned int*)&buff[kSizeWritten + off_Ack_Payload] = seq;
				kSizeWritten += 4;
				if (kSizeWritten >= maxWriteSize)
					break;
			}
			m_AckQueue[i].clear();

			if (kSizeWritten > 0) // only transmit acks if there was still something in the queue
			{
				buff[off_Type] = (char)EPacketType::Ack;
				buff[off_Ack_Chan] = (char)i; // channel
				*(unsigned int*)&buff[off_Ack_Num] = kSizeWritten / 4; // num of acks
				socket->send(m_EndPoint, buff, kSizeWritten + off_Ack_Payload); // <-- this is correct, ack_seq is payload offset (if no dataId attached)
			}
		}
	}

	void RUDPConnection::dispatchRelNewestAckQueue(ISocket* socket)
	{
		std::lock_guard<std::mutex> lock(m_AckRelNewestMutex);
		char buff[RecvPoint::sm_MaxRecvBuffSize];
		int  kSizeWritten = 0;
		int  maxWriteSize = RecvPoint::sm_MaxRecvBuffSize - 64; // account for hdr
		int  ackEntrySize = sizeof(unsigned int)*2;
		for (auto& it : m_AckQueueRelNewest)
		{
			unsigned int dataId  = it.first;
			unsigned int sequenc = it.second;
			*(unsigned int*)&buff[kSizeWritten + off_Ack_Payload] = sequenc;
			*(unsigned int*)&buff[kSizeWritten + off_Ack_Payload + sizeof(unsigned int)] = dataId;
			kSizeWritten += ackEntrySize;
			if (kSizeWritten >= maxWriteSize)
				break; // TODO emit warning
		}
		m_AckQueueRelNewest.clear();

		if (kSizeWritten > 0) // only transmit acks if there was still something in the queue
		{
			buff[off_Type] = (char)EPacketType::Ack_Reliable_Newest;
			buff[off_Ack_Chan] = (char)0; // channel TODO perhaps change in future
			*(unsigned int*)&buff[off_Ack_Num] = kSizeWritten / ackEntrySize; // num of acks
			socket->send(m_EndPoint, buff, kSizeWritten + off_Ack_Payload);
		}
	}

	void RUDPConnection::receiveReliableOrdered(const char * buff, int rawSize)
	{
		char channel;
		bool relay;
		unsigned int seq;
		extractChannelRelayAndSeq( buff, rawSize, channel, relay, seq );

		auto& recvSeq = m_RecvSeq_reliable_recvThread[channel];
		if ( !isSequenceNewer(seq, recvSeq) )
			return;

		std::lock_guard<std::mutex> lock(m_RecvMutex);
		// only insert if received data is not already stored, but waiting to be processed as is out of order
		auto& queue = m_RecvQueue_reliable_order[channel];
		if ( queue.count( seq ) == 0 )
		{
			Packet pack;
			setPacketChannelRelayAndType( pack, buff, rawSize, channel, relay, EPacketType::Reliable_Ordered );
			setPacketPayloadNormal( pack, buff, rawSize );
			queue.insert( std::make_pair(seq, pack) );
		}
		// update recv seq to most recent possible
		while ( queue.count( recvSeq ) != 0 )
		{
			recvSeq++;
		}
	}

	void RUDPConnection::receiveUnreliableSequenced(const char * buff, int rawSize)
	{
		char channel;
		bool relay;
		unsigned int seq;
		extractChannelRelayAndSeq( buff, rawSize, channel, relay, seq );
		
		if ( !isSequenceNewer(seq, m_RecvSeq_unreliable[channel]) )
			return;
	
		// In case of unreliable, immediately update the expected sequenced to the received seq.
		// Therefore, unsequenced but arrived packets, will be discarded!
		m_RecvSeq_unreliable[channel] = seq+1;

		Packet pack;
		setPacketChannelRelayAndType( pack, buff, rawSize, channel, relay, EPacketType::Unreliable_Sequenced );
		setPacketPayloadNormal( pack, buff, rawSize );

		std::lock_guard<std::mutex> lock(m_RecvMutex);
		m_RecvQueue_unreliable_sequenced[channel].emplace_back( pack );
	}

	void RUDPConnection::receiveReliableNewest(const char* buff, int rawSize, unsigned int& seq, unsigned int& dataId)
	{
		char channel;
		bool relay;
		extractChannelRelayAndSeq( buff, rawSize, channel, relay, seq );
		dataId = *(unsigned int*)(buff + off_RelNew_DataId);

		auto it = m_RecvSeq_reliable_newest.find( dataId );
		if ( it != m_RecvSeq_reliable_newest.end() )
		{
			if ( !isSequenceNewer( seq, it->second ) )
				return; // already received newer data on this dataId
			it->second = seq; // otherwise update
		}
		else
		{
			m_RecvSeq_reliable_newest.insert( std::make_pair( dataId, seq ) );
		}
		
		Packet pack;
		pack.dataId = dataId;
		setPacketChannelRelayAndType( pack, buff, rawSize, channel, relay, EPacketType::Reliable_Newest );
		setPacketPayloadRelNewest( pack, buff, rawSize );

		std::lock_guard<std::mutex> lock(m_RecvMutex);
		m_RecvQueue_reliable_newest.emplace_back( pack );
	}

	void RUDPConnection::receiveAck(const char * buff, int rawSize)
	{
		char channel = buff[off_Ack_Chan];
		int num = *(int*)(buff + off_Ack_Num); // num of acks
		std::lock_guard<std::mutex> lock(m_SendMutex);
		auto& queue = m_SendQueue_reliable[channel];
		for (int i = 0; i < num; ++i) // for each ack, try to find it, and remove as was succesfully transmitted
		{
			// remove from send queue if we receive an ack for the packet
			unsigned int seq = *(unsigned int*)(buff + (i*4) + off_Ack_Payload);
			auto it = std::find_if(queue.begin(), queue.end(), [seq](auto& pack)
			{
				return *(unsigned int*)&pack.data[off_Norm_Seq] == seq;
			});
			if (it != queue.end())
			{
				auto& pack = (*it);
				delete [] pack.data;
				queue.erase(it);
			}
		}
	}

	void RUDPConnection::receiveAckRelNewest(const char* buff, int rawSize)
	{
		char channel = buff[off_Ack_Chan];
		int num = *(int*)(buff + off_Ack_Num); // num of acks
		std::lock_guard<std::mutex> lock(m_SendMutex);
		auto& queue = m_SendQueue_reliable_newest;
		for (int i = 0; i < num; ++i) // for each ack, try to find it, and remove as was succesfully transmitted
		{
			// update sequence of dataId to received so that possibly it is no longer necessary to transmit the data
			// from the dataId, unless th seq was in the meantime changed
			unsigned int seq    = *(unsigned int*)(buff + (i*8) + off_Ack_Payload);
			unsigned int dataId = *(unsigned int*)(buff + (i*8+4) + off_Ack_Payload);
			// obtain the data on the dataId
			auto it = m_SendQueue_reliable_newest.find( dataId );
			if ( it != m_SendQueue_reliable_newest.end() )
			{
				auto& tuple = it->second;
				if ( isSequenceNewer( seq, std::get<1>(tuple) ) )
				{
					// replace with newer
					std::get<1>(tuple) = seq;
				}
			}
			else
			{
				// TODO emit warning, received ack for unknown dataId
			}
		}
	}

	void RUDPConnection::assembleNormalPacket(Packet& pack, EPacketType packetType, unsigned char id, const char* data, int len, int hdrSize, char channel, bool relay)
	{
		pack.data = new char[len+hdrSize];
		pack.data[off_Type] = (char)packetType;
		pack.data[off_Norm_Chan] = channel;
		pack.data[off_Norm_Chan] |= ((char)relay) << 3; // skip over the bits for channel, 0 to 7
		pack.data[off_Norm_Id] = id;
		memcpy_s( pack.data + off_Norm_Data, len, data, len ); 
		pack.len = len + off_Norm_Data;
	}

	void RUDPConnection::extractChannelRelayAndSeq(const char* buff, int rawSize, char& channel, bool& relay, unsigned int& seq)
	{
		channel = (buff[off_Norm_Chan] & 7);
		relay   = (buff[off_Norm_Chan] & 8) != 0;
		seq		= *(unsigned int*)(buff + off_Norm_Seq);
	}

	void RUDPConnection::setPacketChannelRelayAndType(Packet& pack, const char* buff, int rawSize, char channel, bool relay, EPacketType type) const
	{
		pack.channel = channel;
		pack.relay = relay;
		pack.type  = type;
	}

	void RUDPConnection::setPacketPayloadNormal(Packet& pack, const char* buff, int rawSize) const
	{
		pack.len  = rawSize - off_Norm_Id; // <--- this is correct, id is enclosed in the payload
		pack.data = new char[pack.len];
		memcpy_s(pack.data, pack.len, buff + off_Norm_Id, pack.len); // off_Norm_Id is correct
	}

	void RUDPConnection::setPacketPayloadRelNewest(Packet& pack, const char* buff, int rawSize) const
	{
		pack.len  = rawSize - off_RelNew_Data;
		pack.data = new char[pack.len];
		memcpy_s(pack.data, pack.len, buff + off_RelNew_Data, pack.len); 
	}

	bool RUDPConnection::isSequenceNewer(unsigned int incoming, unsigned int having) const
	{
		incoming -= having;
		return incoming <= (UINT_MAX>>1);
	}
}