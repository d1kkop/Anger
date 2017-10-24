#pragma once

#include "EndPoint.h"

#include <cstring>
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
		EHeaderPacketType type;
	};


	class RecvNode
	{
	public:
		RecvNode(bool captureSocketErrors=true, i32_t sendThreadSleepTimeMs=6);
		virtual ~RecvNode();
		void postInitialize(class CoreNode* coreNode);
		bool openSocketOnPort(u16_t port);

	public:
		bool send( u8_t id, const i8_t* data, i32_t len, const EndPoint* specific=nullptr, bool exclude=false, 
				   EHeaderPacketType type=EHeaderPacketType::Reliable_Ordered, u8_t channel=0, bool relay=true );
		bool sendReliableNewest( u8_t id, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const EndPoint* specific=nullptr, bool exclude=false );

		class RUDPLink* getLinkAndPinIt(u32_t idx);
		void unpinLink(RUDPLink* link);

		void simulatePacketLoss( i32_t percentage );
		class ISocket* getSocket() const { return m_Socket; }

	private:
		void startThreads();
		void recvThread();
		void sendThread();
		void updatePendingDeletes();
		class RUDPLink* getOrAddLink( const EndPoint& endPoint );

		// for each link
		template <typename Callback>
		void forEachLink( const EndPoint* specific, bool exclude, bool connected, const Callback& cb );

		bool m_SocketOpened;
		volatile bool m_IsClosing;
		class ISocket* m_Socket;
		bool   m_CaptureSocketErrors;
		i32_t  m_SendThreadSleepTimeMs;
		std::thread* m_RecvThread;
		std::thread* m_SendThread;
		std::condition_variable m_SendThreadCv;
		std::mutex m_OpenLinksMutex;
		// Currently opened links are put in a list so that reopend links on same address can not depend on a previously opened session
		std::map<EndPoint, class RUDPLink*, EndPoint::STLCompare> m_OpenLinksMap;
		std::vector<class RUDPLink*> m_OpenLinksList;
		// -- Ptrs of other managers
		class CoreNode* m_CoreNode;
		class ConnectionNode* m_ConnectionNode;
	};


	template <typename Callback>
	void RecvNode::forEachLink(const EndPoint* specific, bool exclude, bool connected, const Callback& cb)
	{
		if ( specific )
		{
			if ( exclude )
			{
				for ( auto it : m_OpenLinksList )
				{
					if ( it->getEndPoint() != *specific )
					{
						cb( it );
					}
				}
			}
			else
			{
				auto it = m_OpenLinksMap.find( *specific );
				if ( it != m_OpenLinksMap.end() )
				{
					cb( it->second );
				}
			}
		}
		else
		{
			for ( auto it : m_OpenLinksList ) 
			{
				cb( it );
			}
		}
	}
}