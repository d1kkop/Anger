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

	void RUDPConnection::beginAddToSendQueue()
	{
		m_SendMutex.lock();
	}

	void RUDPConnection::addToSendQueue(unsigned char id, const char* data, int len, EPacketType packetType, unsigned char channel, bool relay)
	{
		// user not allowed to send acks
		if ( packetType == EPacketType::Ack )
		{
			assert( false && "The user is not allowed to send ack packets" );
			return;
		}
		Packet pack;
		pack.data = new char[len+off_Norm_Data];
		pack.data[off_Type] = (char)packetType;
		pack.data[off_Norm_Chan] = channel;
		pack.data[off_Norm_Chan] |= ((char)relay) << 3; // skip over the bits for channel, 0 to 7
		pack.data[off_Norm_Id] = id;
		memcpy_s( pack.data + off_Norm_Data, len, data, len ); 
		pack.len = len + off_Norm_Data;
		if ( packetType == EPacketType::Reliable_Ordered )
		{
			*(unsigned int*)&pack.data[off_Norm_Seq] = m_SendSeq_reliable[channel]++;
			m_SendQueue_reliable[channel].emplace_back( pack );
		}
		else 
		{
			*(unsigned int*)&pack.data[off_Norm_Seq] = m_SendSeq_unreliable[channel]++;
			m_SendQueue_unreliable.emplace_back( pack );
		}
	}

	void RUDPConnection::endAddToSendQueue()
	{
		m_SendMutex.unlock();
	}

	void RUDPConnection::sendSingle(unsigned char id, const char* data, int len, EPacketType packetType, unsigned char channel)
	{
		beginAddToSendQueue();
		addToSendQueue( id, data, len, packetType, channel );
		endAddToSendQueue();
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

			case EPacketType::Reliable_Ordered:
				// ack it (even if we already processed this packet)
				addAckToAckQueue( buff[off_Norm_Chan] & 7, *(unsigned int*)&buff[off_Norm_Seq] );
				receiveReliableOrdered(buff, rawSize);	
				break;

			case EPacketType::Unreliable_Sequenced:
				receiveUnreliableSequenced(buff, rawSize);
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

	void RUDPConnection::dispatchSendQueue(ISocket* socket)
	{
		std::unique_lock<std::mutex> lock(m_SendMutex);
		for ( int i=0; i<sm_NumChannels; ++i )
		{
			for (auto& it : m_SendQueue_reliable[i])
			{
				auto& pack = it;
				socket->send(m_EndPoint, pack.data, pack.len);
			}
		}
		for (auto& it : m_SendQueue_unreliable)
		{
			auto& pack = it;
			socket->send(m_EndPoint, pack.data, pack.len);
		}
		m_SendQueue_unreliable.clear(); // clear unreliable queue immediately
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
				*(unsigned int*)&buff[kSizeWritten + off_Ack_Seq] = seq;
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
				socket->send(m_EndPoint, buff, kSizeWritten + off_Ack_Seq);
			}
		}
	}

	void RUDPConnection::receiveReliableOrdered(const char * buff, int rawSize)
	{
		char channel = (buff[off_Norm_Chan] & 7);
		bool relay   = (buff[off_Norm_Chan] & 8) != 0;
		unsigned int seq = *(unsigned int*)(buff + off_Norm_Seq);

		std::lock_guard<std::mutex> lock(m_RecvMutex);
		auto& recvSeq = m_RecvSeq_reliable_recvThread[channel];
		if ( !isSequenceNewer(seq, recvSeq) )
			return;

		// only insert if received data is not already stored, but waiting to be processed as is out of order
		auto& queue = m_RecvQueue_reliable_order[channel];
		if ( queue.count( seq ) == 0 )
		{
			Packet pack;
			assemblePacket( pack, buff, rawSize, channel, relay, EPacketType::Reliable_Ordered );
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
		char channel = (buff[off_Norm_Chan] & 7);
		bool relay   = (buff[off_Norm_Chan] & 8) != 0;
		unsigned int seq = *(unsigned int*)(buff + off_Norm_Seq);
	
		std::lock_guard<std::mutex> lock(m_RecvMutex);
		if ( !isSequenceNewer(seq, m_RecvSeq_unreliable[channel]) )
			return;
	
		// In case of unreliable, immediately update the expected sequenced to the received seq.
		// Therefore, unsequenced but arrived packets, will be discarded!
		m_RecvSeq_unreliable[channel] = seq+1;

		Packet pack;
		assemblePacket( pack, buff, rawSize, channel, relay, EPacketType::Unreliable_Sequenced );
		m_RecvQueue_unreliable_sequenced[channel].emplace_back( pack );
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
			unsigned int seq = *(unsigned int*)(buff + (i*4) + off_Ack_Seq);
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

	void RUDPConnection::assemblePacket(Packet& pack, const char* buff, int rawSize, char channel, bool relay, EPacketType type) const
	{
		extractPayload( pack, buff, rawSize );
		pack.channel = channel;
		pack.relay = relay;
		pack.type  = type;
	}

	void RUDPConnection::extractPayload(Packet& pack, const char* buff, int rawSize) const
	{
		pack.len  = rawSize - off_Norm_Id; // <--- this is correct, id is enclosed in the payload
		pack.data = new char[pack.len];
		memcpy_s(pack.data, pack.len, buff + off_Norm_Id, pack.len); // off_Norm_Id is correct
	}

	bool RUDPConnection::isSequenceNewer(unsigned int incoming, unsigned int having) const
	{
		incoming -= having;
		return incoming <= (UINT_MAX>>1);
	}
}