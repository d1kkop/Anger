#pragma once

#include "Zerodelay.h"
#include "EndPoint.h"
#include "RecvNode.h"

#include <atomic>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <map>


namespace Zerodelay
{
	// Represents an item in a group of newest reliable data.
	// If a at least a single item in a group changes, the group with the new item is transmitted.
	struct reliableNewestItem 
	{
		u32_t localRevision;
		u32_t remoteRevision;
		u16_t dataLen, dataCapacity;
		i8_t* data;
	};

	// Represents a group of data that guarentees that the last piece of data (update) is always transmitted.
	struct reliableNewestDataGroup
	{
		reliableNewestItem groupItems[16];
		u8_t  dataId;
	};


	typedef std::deque<Packet> sendQueueType;
	typedef std::deque<Packet> recvQueueType;
	typedef std::deque<u32_t> ackQueueType;
	typedef std::map<u32_t, Packet> recvReliableOrderedQueueType;
	typedef std::map<u32_t, reliableNewestDataGroup> sendReliableNewestQueueType; 


	class RUDPLink
	{
	public:
		static const i32_t sm_MaxLingerTimeMs  = 500;
		static const i32_t sm_MaxItemsPerGroup = 16;

		// TODO Packet layout offsets should not be completely here because it contains higher level data layout information
		static const i32_t off_Type = 0;	// Reliable, Unreliable or Ack
		static const i32_t off_Ack_Chan = 1; // In case of ack, the channel
		static const i32_t off_Ack_Num = 2;  // In case of ack, ther number of acks in one packet cluttered together
		static const i32_t off_Ack_Payload = 6;  // In case of ack, the sequence numb
		static const i32_t off_Norm_Chan = 1;	// Normal, Channel
		static const i32_t off_Norm_Seq  = 2;	// Normal, Seq numb
		static const i32_t off_Norm_Id   = 6;	// Normal/Data, Packet Id 
		static const i32_t off_Norm_Data = 7;	// Normal, Payload
		static const i32_t off_RelNew_Seq  = 1;	// RelNew, sequence
		static const i32_t off_RelNew_Num  = 5;	// RelNew, num groups
		static const i32_t off_RelNew_GroupId   = 9;		// ReliableNew, GroupId (4 bytes)
		static const i32_t off_RelNew_GroupBits = 13;		// 2 Bytes (max 16 vars per group)
		static const i32_t off_RelNew_GroupSkipBytes = 15;	// 2 bytes that hold amount of bytes to skip in case group id is is not known
		static const i32_t off_RelNew_Data = 17;			// pay load
		
		static const i32_t off_Ack_RelNew_Seq = 1;			// Ack_ReliableNew_Seq numb

		static const i32_t sm_NumChannels  = 8;


	public:
		RUDPLink(class RecvNode* recvNode, const EndPoint& endPoint);
		~RUDPLink();

		// Thread safe
		//------------------------------
			void addToSendQueue( u8_t id, const i8_t* data, i32_t len, EHeaderPacketType packetType, u8_t channel=0, bool relay=true );
			void addReliableNewest( u8_t id, const i8_t* data, i32_t len, u32_t groupId, i8_t groupBit );
			void blockAllUpcomingSends();
		
			// Always first call beginPoll, then repeatedly poll until no more packets, then endPoll
			void beginPoll();
			bool poll(Packet& pack);
			void endPoll();

			void flushSendQueue( ISocket* socket );
			void recvData( const i8_t* buff, i32_t len );

			void markPendingDelete();
			bool isPendingDelete() const { return m_IsPendingDelete; }

			// If pinned, link will not be deleted from memory
			// Should only be called when having RecvNode lock
			void pin();
			bool isPinned() const;
			void unpin();

			bool areAllQueuesEmpty() const;
		//------------------------------

		// Set to 0, to turn off. Default is off.
		void simulatePacketLoss( u8_t percentage = 10 );						// Not thread safe, but only for debugging purposes so deliberately no atomic_int.
		const EndPoint& getEndPoint() const { return m_EndPoint; }				// Set at beginning, can be queried by multiple threads.
		i32_t getTimeSincePendingDelete() const;								// Is set when becomes pending delete which is thread safe, so this can be queried thread safe.


	private:
		void addAckToAckQueue( i8_t channel, u32_t seq );
		void dispatchSendQueue(ISocket* socket);
		void dispatchReliableNewestQueue(ISocket* socket);
		void dispatchReliableQueue(ISocket* socket);
		void dispatchUnreliableQueue(ISocket* socket);
		void dispatchAckQueue(ISocket* socket);
		void dispatchRelNewestAckQueue(ISocket* socket);
		void receiveReliableOrdered(const i8_t * buff, i32_t rawSize);
		void receiveUnreliableSequenced(const i8_t * buff, i32_t rawSize);
		void receiveReliableNewest(const i8_t* buff, i32_t rawSize);
		void receiveAck(const i8_t* buff, i32_t rawSize);
		void receiveAckRelNewest(const i8_t* buff, i32_t rawSize);
		void assembleNormalPacket( Packet& pack, EHeaderPacketType packetType, u8_t dataId, const i8_t* data, i32_t len, i32_t hdrSize, i8_t channel, bool relay );
		void extractChannelRelayAndSeq(const i8_t* buff, i32_t rawSize, i8_t& channOut, bool& relayOut, u32_t& seqOut );
		void createNormalPacket(Packet& pack, const i8_t* buff, i32_t dataSize, i8_t channel, bool relay, EHeaderPacketType type) const;
		bool isSequenceNewer( u32_t incoming, u32_t having ) const;
		bool isSequenceNewerGroupItem( u32_t incoming, u32_t having ) const; // This one is newer only when having is incoming-UINT_MAX/2

		// manager ptrs
		RecvNode* m_RecvNode;
		// state
		EndPoint m_EndPoint;
		std::atomic_bool m_BlockNewSends;
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
		// sequencers
		u32_t m_SendSeq_reliable[sm_NumChannels];
		u32_t m_SendSeq_unreliable[sm_NumChannels];
		u32_t m_RecvSeq_unreliable[sm_NumChannels];
		u32_t m_RecvSeq_reliable_recvThread[sm_NumChannels];		// keeps track of which packets have arrived but possibly not yet processed on game thread
		u32_t m_RecvSeq_reliable_gameThread[sm_NumChannels];		// keep track of which packets have been processed on game thread
		u32_t m_SendSeq_reliable_newest;
		std::atomic<u32_t> m_RecvSeq_reliable_newest_recvThread;				// atomic, because updated in recv thread, but used for sending ack sequence in send thread
		u32_t m_RecvSeq_reliable_newest_sendThread;								// checked against the 'm_RecvSeq_reliable_newest_sendThread' and will dispatch if necessary
		u32_t m_RecvSeq_reliable_newest_ack;
		// threading
		mutable std::mutex m_SendQueuesMutex;
		mutable std::mutex m_RecvQueuesMutex;
		mutable std::mutex m_AckMutex;
		mutable std::mutex m_AckRelNewestMutex;
		// statistics
		u8_t m_PacketLossPercentage;
		// pinned
		uint32_t m_PinnedCount;
		// on delete
		std::mutex m_PendingDeleteMutex;
		std::atomic_bool m_IsPendingDelete; // Set from main thread, queried by recv thread
		clock_t m_MarkDeleteTS;
	};
}