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
		virtual void addToSendQueue(unsigned char id, const char* data, int len, EPacketType packetType, unsigned char channel=0, bool relay=true) = 0;
		virtual void addReliableNewest( unsigned char id, const char* data, int len, unsigned int dataId, bool relay=true ) = 0;
		virtual void beginPoll() = 0;
		virtual bool poll(Packet& packet) = 0;
		virtual void endPoll() = 0;
		virtual void flushSendQueue( class ISocket* socket ) = 0;
		virtual void recvData( const char* buff, int len ) = 0;
		virtual void simulatePacketLoss(unsigned char percentage) = 0;

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
		int getTimeSincePendingDelete() const
		{
			auto now = ::clock();
			return int((float(now - m_MarkDeleteTS) / (float)CLOCKS_PER_SEC) * 1000.f); // to ms
		}

	protected:
		bool m_IsPendingDelete;
		EndPoint m_EndPoint;
		clock_t m_MarkDeleteTS;
	};

	class RUDPConnection: public IConnection
	{
		// Packet layout offsets
		static const int off_Type = 0;	// Reliable, Unreliable or Ack
		static const int off_Ack_Chan = 1; // In case of ack, the channel
		static const int off_Ack_Num = 2;  // In case of ack, ther number of acks in one packet cluttered together
		static const int off_Ack_Payload = 6;  // In case of ack, the sequence numb
		static const int off_Norm_Chan = 1;	// Normal, Channel
		static const int off_Norm_Seq  = 2; // Normal, Seq numb
		static const int off_Norm_Id   = 6;	// Normal, Packet Id
		static const int off_Norm_Data = 7; // Normal, Payload
		static const int off_RelNew_DataId = 7;		// ReliableNew, Data Id
		static const int off_RelNew_Data   = 13;	// ReliableNew, Payload

		static const int sm_NumChannels  = 8;

		typedef std::deque<Packet> sendQueueType;
		typedef std::deque<Packet> recvQueueType;
		typedef std::deque<unsigned int> ackQueueType;
		typedef std::deque<std::pair<unsigned int, unsigned>> ackRelNewestQueueType; // dataId, seq
		typedef std::map<unsigned int, Packet> recvReliableOrderedQueueType;
		typedef std::map<unsigned int, std::tuple<unsigned int, unsigned int, Packet>> sendReliableNewestQueueType;  // sendSeq, recvSeq, Packet

	public:
		RUDPConnection(const struct EndPoint& endPoint);
		virtual ~RUDPConnection();

		// Thread safe
		//------------------------------
			// Acquires the lock
			// In case of 'Ordered', the channel specifies on which channel a packet should arrive ordered.
			// Num of channels is 8, so 0 to 7 is valid channel.
			virtual void addToSendQueue( unsigned char id, const char* data, int len, EPacketType packetType, unsigned char channel=0, bool relay=true ) override;

			// Sends reliable newest, that is, the newest version of a given piece of data is guarenteed to arrive.
			// Eg. If twice data is transmitted on the same dataId, then it is only guarenteed that the latter will arive. No ordering takes place.
			virtual void addReliableNewest( unsigned char id, const char* data, int len, unsigned int dataId, bool relay=true ) override;
		
			// Always first call beginPoll, then repeatedly poll until no more packets, then endPoll
			virtual void beginPoll() override;
			virtual bool poll(Packet& pack) override;
			virtual void endPoll() override;

			virtual void flushSendQueue( ISocket* socket ) override;
			virtual void recvData( const char* buff, int len ) override;

			// Returns true if all packets for all reliable send queues have been acked
			bool areAllReliableSendQueuesEmpty() const;
		//------------------------------

		// Set to 0, to turn off. Default is off.
		virtual void simulatePacketLoss( unsigned char percentage = 10 ) override;

	private:
		void addAckToAckQueue( char channel, unsigned int seq );
		void addAckToRelNewestAckQueue( unsigned int seq, unsigned int dataId );
		void dispatchSendQueue(ISocket* socket);
		void dispatchAckQueue(ISocket* socket);
		void dispatchRelNewestAckQueue(ISocket* socket);
		void receiveReliableOrdered(const char * buff, int rawSize);
		void receiveUnreliableSequenced(const char * buff, int rawSize);
		void receiveReliableNewest(const char* buff, int rawSize, unsigned int& seq, unsigned int& dataId);
		void receiveAck(const char* buff, int rawSize);
		void receiveAckRelNewest(const char* buff, int rawSize);
		void assembleNormalPacket( Packet& pack, EPacketType packetType, unsigned char id, const char* data, int len, int hdrSize, char channel, bool relay );
		void extractChannelRelayAndSeq(const char* buff, int rawSize, char& channOut, bool& relayOut, unsigned int& seqOut );
		void setPacketChannelRelayAndType( Packet& pack, const char*  buff, int rawSize, char channel, bool relay, EPacketType type ) const;
		void setPacketPayloadNormal( Packet& pack, const char* buff, int rawSize ) const;
		void setPacketPayloadRelNewest( Packet& pack, const char* buff, int rawSize ) const;
		bool isSequenceNewer( unsigned int incoming, unsigned int having ) const;

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
		unsigned int m_SendSeq_reliable[sm_NumChannels];
		unsigned int m_SendSeq_unreliable[sm_NumChannels];
		unsigned int m_RecvSeq_unreliable[sm_NumChannels];
		unsigned int m_RecvSeq_reliable_recvThread[sm_NumChannels];
		unsigned int m_RecvSeq_reliable_gameThread[sm_NumChannels];
		std::map<unsigned int, unsigned int> m_RecvSeq_reliable_newest;
		unsigned char m_PacketLossPercentage;
	};
}