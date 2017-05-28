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
		int len; 
		char* data;
		char channel;
		bool relay;
		unsigned int dataId; // for ReliableNewest
		EPacketType type;
	};


	class RecvPoint
	{
	public:
		static const int sm_MaxRecvBuffSize = 8192;

	protected:
		RecvPoint(bool captureSocketErrors=true, int sendThreadSleepTimeMs=2);
		virtual ~RecvPoint();

		// Called on recv thread
		virtual class IConnection* createNewConnection( const EndPoint& endPoint ) const = 0;

	public:
		void send( unsigned char id, const char* data, int len, const EndPoint* specific=nullptr, bool exclude=false, 
				   EPacketType type=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true );

		void  setUserDataPtr( void* ptr) { m_UserPtr = ptr; }
		void* getUserDataPtr() const { return m_UserPtr; }

		void setUserDataIdx( int idx ) { m_UserIndex = idx; }
		int  getUserDataIdx() const { return m_UserIndex; }

		void simulatePacketLoss( int percentage );
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
		int  m_SendThreadSleepTimeMs;
		std::thread* m_RecvThread;
		std::thread* m_SendThread;
		std::condition_variable m_SendThreadCv;
		std::mutex m_ConnectionListMutex;
		std::vector<int> m_SocketErrors;
		std::map<EndPoint, class IConnection*, EndPoint::STLCompare> m_Connections;
		void* m_UserPtr;
		int  m_UserIndex;
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