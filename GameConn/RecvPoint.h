#pragma once

#include "Zerodelay.h"
#include "EndPoint.h"

#include <mutex>
#include <map>
#include <thread>
#include <vector>
#include <condition_variable>


namespace Zerodelay
{
	struct Packet 
	{
		i32_t len; 
		i8_t* data;
		i8_t channel;
		bool relay;
		// reliable newest
		u32_t groupId;
		u16_t groupBits;
		EPacketType type;
	};


	class RecvPoint
	{
	public:
		static const i32_t sm_MaxRecvBuffSize = 8192; // Ensure that this at easily encompasses the maxSendSize as it makes computations in some algorithms easier. Eg. twice as big as max SendSize.
		static const i32_t sm_MaxSendSize = 1600;

	protected:
		RecvPoint(bool captureSocketErrors=true, i32_t sendThreadSleepTimeMs=2);
		virtual ~RecvPoint();

		// Called on recv thread
		virtual class IConnection* createNewConnection( const EndPoint& endPoint ) const = 0;

	public:
		void send( u8_t id, const i8_t* data, i32_t len, const EndPoint* specific=nullptr, bool exclude=false, 
				   EPacketType type=EPacketType::Reliable_Ordered, u8_t channel=0, bool relay=true );
		void sendReliableNewest( u8_t id, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const EndPoint* specific=nullptr, bool exclude=false, bool relay=true );

		void  setUserDataPtr( void* ptr) { m_UserPtr = ptr; }
		void* getUserDataPtr() const { return m_UserPtr; }

		void setUserDataIdx( i32_t idx ) { m_UserIndex = idx; }
		i32_t  getUserDataIdx() const { return m_UserIndex; }

		void simulatePacketLoss( i32_t percentage );
		class ISocket* getSocket() const { return m_ListenSocket; }

	protected:
		void startThreads();
		void copyConnectionsTo( std::vector<class IConnection*>& dstList );
		void markIsPendingDelete( const std::vector<class IConnection*>& srcList );

	private:
		template <typename Callback>
		void forEachConnection( const EndPoint* specific, bool exclude, Callback cb );
		void recvThread();
		void sendThread();

	protected:
		volatile bool m_IsClosing;
		class ISocket* m_ListenSocket;
		bool m_CaptureSocketErrors;
		i32_t  m_SendThreadSleepTimeMs;
		std::thread* m_RecvThread;
		std::thread* m_SendThread;
		std::condition_variable m_SendThreadCv;
		std::mutex m_ConnectionListMutex;
		std::vector<i32_t> m_SocketErrors;
		std::map<EndPoint, class IConnection*, EndPoint::STLCompare> m_Connections;
		void* m_UserPtr;
		i32_t  m_UserIndex;
	};

	template <typename Callback>
	void RecvPoint::forEachConnection(const EndPoint* specific, bool exclude, Callback cb)
	{
		if ( specific )
		{
			if ( exclude )
			{
				for ( auto& kvp : m_Connections )
				{
					if ( kvp.first != *specific )
					{
						cb( kvp.second );
					}
				}
			}
			else
			{
				auto it = m_Connections.find( *specific );
				if ( it != m_Connections.end() )
				{
					cb( it->second );
				}
			}
		}
		else
		{
			for ( auto& kvp : m_Connections ) 
			{
				cb( kvp.second );
			}
		}
	}
}