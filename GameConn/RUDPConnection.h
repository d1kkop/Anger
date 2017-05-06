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
		virtual ~IConnection() { }

		// --- All functions thread safe ---
		virtual void beginAddToSendQueue() = 0;
		virtual void addToSendQueue(unsigned char id, const char* data, int len, EPacketType packetType, unsigned char channel=0, bool relay=true) = 0;
		virtual void endAddToSendQueue() = 0;
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
		static const int off_Ack_Seq = 6;  // In case of ack, the sequence numb
		static const int off_Norm_Chan = 1;	// Normal, Channel
		static const int off_Norm_Seq  = 2; // Normal, Seq numb
		static const int off_Norm_Id   = 6;	// Normal, Packet Id
		static const int off_Norm_Data = 7; // Normal, Payload

		static const int sm_NumChannels  = 8;

		typedef std::deque<Packet> sendQueueType;
		typedef std::deque<Packet>  recvQueueType;
		typedef std::deque<unsigned int> ackQueueType;
		typedef std::map<unsigned int, Packet>  recvReliableOrderedQueueType;

	public:
		RUDPConnection(const struct EndPoint& endPoint);
		virtual ~RUDPConnection();

		// Thread safe
		//------------------------------
			// Acquires the lock
			virtual void beginAddToSendQueue() override;
			// In case of 'Ordered', the channel specifies on which channel a packet should arrive ordered.
			// Num of channels is 8, so 0 to 7 is valid channel.
			virtual void addToSendQueue( unsigned char id, const char* data, int len, EPacketType packetType, unsigned char channel=0, bool relay=true ) override;
			virtual void endAddToSendQueue() override; // releases the lock
		
			// Calls beginSend-send-endSend in a chain
			void sendSingle( unsigned char id, const char* data, int len, EPacketType packetType, unsigned char channel=0 );

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
		void dispatchSendQueue(ISocket* socket);
		void dispatchAckQueue(ISocket* socket);
		void receiveReliableOrdered(const char * buff, int rawSize);
		void receiveUnreliableSequenced(const char * buff, int rawSize);
		void receiveAck(const char* buff, int rawSize);
		void assemblePacket( Packet& pack, const char*  buff, int rawSize, char channel, bool relay, EPacketType type ) const;
		void extractPayload( Packet& pack, const char* buff, int rawSize ) const;
		bool isSequenceNewer( unsigned int incoming, unsigned int having ) const;

		sendQueueType m_SendQueue_reliable[sm_NumChannels];
		sendQueueType m_SendQueue_unreliable;
		recvQueueType m_RecvQueue_unreliable_order[sm_NumChannels];
		recvReliableOrderedQueueType m_RecvQueue_reliable_order[sm_NumChannels];
		ackQueueType m_AckQueue[sm_NumChannels];
		mutable std::mutex m_SendMutex;
		mutable std::mutex m_RecvMutex;
		mutable std::mutex m_AckMutex;
		unsigned int m_SendSeq_reliable[sm_NumChannels];
		unsigned int m_SendSeq_unreliable[sm_NumChannels];
		unsigned int m_RecvSeq_unreliable[sm_NumChannels];
		unsigned int m_RecvSeq_reliable_recvThread[sm_NumChannels];
		unsigned int m_RecvSeq_reliable_gameThread[sm_NumChannels];
		unsigned char m_PacketLossPercentage;
	};
}