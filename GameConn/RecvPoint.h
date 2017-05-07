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
		void beginSend( const EndPoint* specific=nullptr, bool exclude=false );
		void send( unsigned char id, const char* data, int len, EPacketType type=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true );
		void endSend();

		void simulatePacketLoss( int percentage );

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
		const EndPoint* m_WasSpecific;
		bool m_WasExclude;
		std::thread* m_RecvThread;
		std::thread* m_SendThread;
		std::condition_variable m_SendThreadCv;
		std::mutex m_ConnectionListMutex;
		std::vector<int> m_SocketErrors;
		std::map<EndPoint, class IConnection*, EndPoint::STLCompare> m_Connections;

#if ZNETWORK_DEBUG
	public:
		ISocket* dbg_getSocket() const { return m_ListenSocket; }
#endif
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