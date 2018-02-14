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


	class RUDPLink
	{
	public:
		static const i32_t sm_MaxLingerTimeMs  = 1000;
		static const i32_t sm_MaxItemsPerGroup = 16;

		
		// Generic packet overhead
		static const i32_t off_Link = 0;
		static const i32_t off_Type = 4;			// Reliable, Unreliable or Ack
		static const i32_t hdr_Generic_Size = (off_Type - off_Link)+1;
		

		// Normal packet overhead
		static const i32_t off_Norm_ChanNFlags = 5;		// Normal, Channel (least 7 bits) & Relay (hi bit) & Fragment Start/End bit
		static const i32_t off_Norm_Seq  = 6;		// Normal, Seq numb
		static const i32_t off_Norm_Id   = 10;		// Normal/Data, Packet Id 
		static const i32_t off_Norm_Data = 11;		// Normal, Payload
		static const i32_t hdr_Norm_Size = (off_Norm_Id - off_Norm_ChanNFlags); // Note, payload includes ID (1byte)

		// Reliable newest packet overhead
		static const i32_t off_RelNew_Seq  = 5;		// RelNew, sequence
		static const i32_t off_RelNew_Num  = 9;		// RelNew, num groups
		static const i32_t off_RelNew_GroupId   = 13;		// ReliableNew, GroupId (4 bytes)
		static const i32_t off_RelNew_GroupBits = 17;		// 2 Bytes (max 16 vars per group)
		static const i32_t off_RelNew_GroupSkipBytes = 19;	// 2 bytes that hold amount of bytes to skip in case group id is is not known
		static const i32_t off_RelNew_Data = 21;			// pay load
		static const i32_t hdr_Relnew_Size = (off_RelNew_Data - off_RelNew_Seq);


		// Ack normal packet overhead
		static const i32_t off_Ack_Chan = 5;		// In case of ack, the channel
		static const i32_t off_Ack_Num  = 6;		// In case of ack, ther number of acks in one packet cluttered together
		static const i32_t off_Ack_Payload = 10;	// In case of ack, the first sequence numb
		static const i32_t hdr_Ack_Size = (off_Ack_Payload - off_Ack_Chan);

		
		// Ack reliable newest overhead
		static const i32_t off_Ack_RelNew_Seq  = 5;			// Ack_ReliableNew_Seq numb
		static const i32_t hdr_Ack_RelNew_Size = 4;			// Size of single sequence numb


		// Maximum channels in case of normal packet types
		static const i32_t sm_NumChannels  = 8;
		

	public:
		RUDPLink(class RecvNode* recvNode, const EndPoint& endPoint, u32_t linkId);
		~RUDPLink();

		// ------ Called from main thread -------

		ESendCallResult addToSendQueue( u8_t id, const i8_t* data, i32_t len, EHeaderPacketType packetType, u8_t channel=0, bool relay=true, u32_t* sequence=nullptr, u32_t* numFragments=nullptr);
		void addReliableNewest( u8_t id, const i8_t* data, i32_t len, u32_t groupId, i8_t groupBit );
		void blockAllUpcomingSends();
		
		// Always first call beginPoll, then repeatedly poll until no more packets, then endPoll
		void beginPoll();
		bool poll(Packet& pack);
		void endPoll();

		void markPendingDelete();
		bool isPendingDelete() const { return m_IsPendingDelete; }

		// If pinned, link will not be deleted from memory
		// Should only be called when having OpenListMutex lock
		void pin();
		bool isPinned() const;
		void unpin();

		u32_t id() const { return m_LinkId; }
		bool areAllQueuesEmpty() const;
		bool isSequenceDelivered(u32_t sequence, i8_t channel) const;

		// Set to 0, to turn off. Default is off.
		void simulatePacketLoss( u8_t percentage = 10 );						// Not thread safe, but only for debugging purposes so deliberately no atomic_int.
		const EndPoint& getEndPoint() const { return m_EndPoint; }				// Set at beginning, can be queried by multiple threads.
		i32_t getTimeSincePendingDelete() const;								// Is set when becomes pending delete which is thread safe, so this can be queried thread safe.

		// TODO To be implemented
		u32_t getLatency() const { return 40; }

	private:
		// executed on send thread
		void dispatchRelOrderedQueueIfLatencyTimePassed(u32_t deltaTime, ISocket* socket);
		void dispatchReliableOrderedQueue(ISocket* socket);
		void dispatchReliableNewestQueue(ISocket* socket);
		void dispatchAckQueue(ISocket* socket);
		void dispatchRelNewestAckQueue(ISocket* socket);

		// executed on recv thread
		void recvData( const i8_t* buff, i32_t len );
		void addAckToAckQueue( i8_t channel, u32_t seq );
		void receiveReliableOrdered(u32_t linkId, const i8_t * buff, i32_t rawSize);
		void receiveUnreliableSequenced(u32_t linkId, const i8_t * buff, i32_t rawSize);
		void receiveReliableNewest(u32_t linkId, const i8_t* buff, i32_t rawSize);
		void receiveAck(const i8_t* buff, i32_t rawSize);
		void receiveAckRelNewest(const i8_t* buff, i32_t rawSize);

		// serialize functions
		static void serializeNormalPacket( std::vector<Packet>& packs, u32_t linkId, EHeaderPacketType packetType, u8_t dataId, const i8_t* data, i32_t len, i32_t fragmentSize, i8_t channel, bool relay );
		static bool deserializeGenericHdr(const i8_t* buff, i32_t rawSize, u32_t& linkIdOut, EHeaderPacketType& packetType);
		static bool deserializeNormalHdr(const i8_t* buff, i32_t rawSize, i8_t& channOut, bool& relayOut, u32_t& seqOut, bool& firstFragment, bool& lastFragment );
		static void createNormalPacket(Packet& pack, const i8_t* buff, i32_t dataSize, u32_t linkId, i8_t channel, bool relay, EHeaderPacketType type);
		static void unfragmentUnreliablePacket(Packet& pack, const std::vector<std::pair<Packet, u32_t>>& fragments);
		static void unfragmentReliablePacket(Packet& pack, u32_t beginSeq, u32_t lastSeq, std::map<u32_t, Packet>& fragments); 

		// sequence newer support
		static bool isSequenceNewer( u32_t incoming, u32_t having );
		static bool isSequenceNewerGroupItem( u32_t incoming, u32_t having ); // This one is newer only when having is incoming-UINT_MAX/2

		// manager ptrs
		RecvNode* m_RecvNode;
		// state
		u32_t m_LinkId;
		EndPoint m_EndPoint;
		std::atomic_bool m_BlockNewSends;
		// send queues
		std::deque<Packet>  m_RetransmitQueue_reliable[sm_NumChannels];
		std::map<u32_t, reliableNewestDataGroup> m_SendQueue_reliable_newest;
		// recv queues
		std::deque<Packet>		m_RecvQueue_unreliable_sequenced[sm_NumChannels];
		std::map<u32_t, std::pair<Packet, u32_t>> m_RecvQueue_reliable_order[sm_NumChannels]; // packet/num fragments
		std::deque<Packet>		m_RecvQueue_reliable_newest;
		// fragment buffers
		std::vector<std::pair<Packet, u32_t>>		m_Ureliable_fragments[sm_NumChannels];
		std::map<u32_t, Packet>						m_Reliable_fragments[sm_NumChannels];
		// ack queue
		std::deque<u32_t> m_AckQueue[sm_NumChannels];
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
		mutable std::mutex m_ReliableOrderedQueueMutex;
		mutable std::mutex m_ReliableNewestQueueMutex;
		mutable std::mutex m_RecvQueuesMutex;
		mutable std::mutex m_AckMutex;
		// statistics
		u32_t m_RetransmitWaitingTime;
		u8_t  m_PacketLossPercentage;
		u32_t m_FragmentSize;
		// pinned
		u32_t m_PinnedCount;
		// on delete
		std::mutex m_PendingDeleteMutex;
		volatile bool m_IsPendingDelete; // Set from main thread, queried by recv thread
		clock_t m_MarkDeleteTS;

		friend class RecvNode;
	};
}