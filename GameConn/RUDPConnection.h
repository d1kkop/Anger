#pragma once

#include "Zerodelay.h"
#include "EndPoint.h"
#include "RecvPoint.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>


namespace Zerodelay
{
	class IConnection
	{
	public:
		virtual ~IConnection() = default;

		// --- All functions thread safe ---
		virtual void addToSendQueue(u8_t id, const i8_t* data, i32_t len, EPacketType packetType, u8_t channel=0, bool relay=true) = 0;
		virtual void addReliableNewest( u8_t id, const i8_t* data, i32_t len, u32_t groupId, i8_t groupBit, bool relay=true ) = 0;
		virtual void beginPoll() = 0;
		virtual bool poll(Packet& packet) = 0;
		virtual void endPoll() = 0;
		virtual void flushSendQueue( class ISocket* socket ) = 0;
		virtual void recvData( const i8_t* buff, i32_t len ) = 0;
		virtual void simulatePacketLoss(u8_t percentage) = 0;

		const EndPoint& getEndPoint() const { return m_EndPoint; }

		// -- Only to be accessed by network recv-thread --
		bool  isPendingDelete() const { return m_IsPendingDelete; }
		void  setIsPendingDelete() 
		{ 
			if ( m_IsPendingDelete )
				return;
			m_IsPendingDelete = true; 
			m_MarkDeleteTS = ::clock(); 
		}
		i32_t getTimeSincePendingDelete() const
		{
			auto now = ::clock();
			return i32_t((float(now - m_MarkDeleteTS) / (float)CLOCKS_PER_SEC) * 1000.f); // to ms
		}

	protected:
		bool m_IsPendingDelete;
		EndPoint m_EndPoint;
		clock_t m_MarkDeleteTS;
	};

	// Represents an item in a group of newest reliable data.
	// If a at least a single item in a group changes, the group with the new item is transmitted.
	struct reliableNewestItem 
	{
		u32_t localRevision;
		u32_t remoteRevision;
		i8_t* data;
		i32_t dataLen, dataCapacity;
	};

	// Represents a group of data that guarentees that the last piece of data (update) is always transmitted.
	struct reliableNewestDataGroup
	{
		u32_t groupSeq;
		u16_t writeMask;
		reliableNewestItem groupItems[16];
	};

	class RUDPConnection: public IConnection
	{
		// Packet layout offsets
		static const i32_t off_Type = 0;	// Reliable, Unreliable or Ack
		static const i32_t off_Ack_Chan = 1; // In case of ack, the channel
		static const i32_t off_Ack_Num = 2;  // In case of ack, ther number of acks in one packet cluttered together
		static const i32_t off_Ack_Payload = 6;  // In case of ack, the sequence numb
		static const i32_t off_Norm_Chan = 1;	// Normal, Channel
		static const i32_t off_Norm_Seq  = 2; // Normal, Seq numb
		static const i32_t off_Norm_Id   = 6;	// Normal, Packet Id
		static const i32_t off_Norm_Data = 7; // Normal, Payload
		static const i32_t off_RelNew_GroupId   = 7;	// ReliableNew, Data Id
		static const i32_t off_RelNew_GroupBits = 11;	// 2 Bytes (max 16 vars per group)
		static const i32_t off_RelNew_Data  = 13;	// ReliableNew, Payload

		static const i32_t sm_NumChannels  = 8;

		typedef std::deque<Packet> sendQueueType;
		typedef std::deque<Packet> recvQueueType;
		typedef std::deque<u32_t> ackQueueType;
		typedef std::deque<std::pair<u32_t, unsigned>> ackRelNewestQueueType; // groupId, seq
		typedef std::map<u32_t, Packet> recvReliableOrderedQueueType;
		typedef std::map<u32_t, reliableNewestDataGroup> sendReliableNewestQueueType; 

	public:
		RUDPConnection(const struct EndPoint& endPoint);
		virtual ~RUDPConnection();

		// Thread safe
		//------------------------------
			virtual void addToSendQueue( u8_t id, const i8_t* data, i32_t len, EPacketType packetType, u8_t channel=0, bool relay=true ) override;
			virtual void addReliableNewest( u8_t id, const i8_t* data, i32_t len, u32_t groupId, i8_t groupBit, bool relay=true ) override;
		
			// Always first call beginPoll, then repeatedly poll until no more packets, then endPoll
			virtual void beginPoll() override;
			virtual bool poll(Packet& pack) override;
			virtual void endPoll() override;

			virtual void flushSendQueue( ISocket* socket ) override;
			virtual void recvData( const i8_t* buff, i32_t len ) override;

			bool areAllReliableSendQueuesEmpty() const;
		//------------------------------

		// Set to 0, to turn off. Default is off.
		virtual void simulatePacketLoss( u8_t percentage = 10 ) override;

	private:
		void addAckToAckQueue( i8_t channel, u32_t seq );
		void addAckToRelNewestAckQueue( u32_t seq, u32_t groupId );
		void dispatchSendQueue(ISocket* socket);
		void dispatchAckQueue(ISocket* socket);
		void dispatchRelNewestAckQueue(ISocket* socket);
		void receiveReliableOrdered(const i8_t * buff, i32_t rawSize);
		void receiveUnreliableSequenced(const i8_t * buff, i32_t rawSize);
		void receiveReliableNewest(const i8_t* buff, i32_t rawSize, u32_t& seq, u32_t& groupId);
		void receiveAck(const i8_t* buff, i32_t rawSize);
		void receiveAckRelNewest(const i8_t* buff, i32_t rawSize);
		void assembleNormalPacket( Packet& pack, EPacketType packetType, u8_t id, const i8_t* data, i32_t len, i32_t hdrSize, i8_t channel, bool relay );
		void extractChannelRelayAndSeq(const i8_t* buff, i32_t rawSize, i8_t& channOut, bool& relayOut, u32_t& seqOut );
		void setPacketChannelRelayAndType( Packet& pack, const i8_t*  buff, i32_t rawSize, i8_t channel, bool relay, EPacketType type ) const;
		void setPacketPayloadNormal( Packet& pack, const i8_t* buff, i32_t rawSize ) const;
		void setPacketPayloadRelNewest( Packet& pack, const i8_t* buff, i32_t rawSize ) const;
		bool isSequenceNewer( u32_t incoming, u32_t having ) const;

		// send queues
		sendQueueType m_SendQueue_reliable[sm_NumChannels];
		sendQueueType m_SendQueue_unreliable;
		sendReliableNewestQueueType m_SendQueue_reliable_newest;
		// recv queues
		recvQueueType m_RecvQueue_unreliable_sequenced[sm_NumChannels];
		recvReliableOrderedQueueType m_RecvQueue_reliable_order[sm_NumChannels];
		recvQueueType m_RecvQueue_reliable_newest;
		// ack queue
		ackQueueType m_AckQueue[sm_NumChannels];
		ackRelNewestQueueType m_AckQueueRelNewest;
		// other
		mutable std::mutex m_SendMutex;
		mutable std::mutex m_RecvMutex;
		mutable std::mutex m_AckMutex;
		mutable std::mutex m_AckRelNewestMutex;
		u32_t m_SendSeq_reliable[sm_NumChannels];
		u32_t m_SendSeq_unreliable[sm_NumChannels];
		u32_t m_RecvSeq_unreliable[sm_NumChannels];
		u32_t m_RecvSeq_reliable_recvThread[sm_NumChannels];
		u32_t m_RecvSeq_reliable_gameThread[sm_NumChannels];
		std::map<u32_t, u32_t> m_RecvSeq_reliable_newest;
		u8_t m_PacketLossPercentage;
	};
}