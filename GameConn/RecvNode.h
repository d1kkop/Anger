#pragma once

#include "EndPoint.h"
#include "CoreNode.h"

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
		u32_t linkId;
		i32_t len; 
		i8_t* data;
		i8_t channel;
		bool relay;
		EHeaderPacketType type;
	};


	class RecvNode
	{
	public:
		RecvNode(u32_t sendRelNewestIntervalMs=33, u32_t ackAggregateTimeMs=8);
		virtual ~RecvNode();
		void reset();

		void postInitialize(class CoreNode* coreNode);
		bool openSocketOnPort(u16_t port);

	public:
		ESendCallResult send( u8_t id, const i8_t* data, i32_t len, const EndPoint* specific=nullptr, bool exclude=false, 
							  EHeaderPacketType type=EHeaderPacketType::Reliable_Ordered, u8_t channel=0, bool relay=true, 
							  std::vector<ZAckTicket>* deliveryTraceOut=nullptr );
		void sendReliableNewest( u8_t id, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const EndPoint* specific=nullptr, bool exclude=false );

		class RUDPLink* getLinkAndPinIt(u32_t idx);
		void unpinLink(RUDPLink* link);

		// This functionality can only be called from one thread (the main usually)
		void pinList(); // call from main
		bool isListPinned() const;  // call from receive (must have openListMutex)
		void unpinList(); // call from main

		void simulatePacketLoss( i32_t percentage );
		class ISocket* getSocket() const { return m_Socket; }

		class RUDPLink* getLink( const EndPoint& endPoint, bool getIfIsPendingDelete ) const;
		class RUDPLink* addLink( const EndPoint& endPoint, const u32_t* linkPtr ); // returns nullptr if already exists
		void startThreads();

		i32_t getNumOpenLinks() const;
		CoreNode* getCoreNode() const { return m_CoreNode; }

	private:
		void recvThread();
		void sendThread();
		void updatePendingDeletes();

		// for each link (only to b called from main thread)
		template <typename Callback>
		void forEachLink( const EndPoint* specific, bool exclude, bool connected, u32_t& linkCount, const Callback& cb );

		volatile bool m_IsClosing;
		class ISocket* m_Socket;
		bool  m_CaptureSocketErrors;
		u32_t m_SendRelNewestIntervalMs;
		u32_t m_AckAggregateTimeMs;
		std::thread* m_RecvThread;
		std::thread* m_SendThread;
		std::condition_variable m_SendThreadCv;
		mutable std::mutex m_OpenLinksMutex;
		// Currently opened links are put in a list so that reopend links on same address can not depend on a previously opened session
		std::map<EndPoint, class RUDPLink*, EndPoint::STLCompare> m_OpenLinksMap;
		std::vector<class RUDPLink*> m_OpenLinksList;
		volatile bool m_ListPinned;
		// -- Ptrs to other managers
		class CoreNode* m_CoreNode;
		class ConnectionNode* m_ConnectionNode;
	};


	template <typename Callback>
	void RecvNode::forEachLink(const EndPoint* specific, bool exclude, bool connected, u32_t& linkCount, const Callback& cb)
	{
		pinList();
		linkCount = (u32_t) m_OpenLinksList.size();
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
		unpinList();
	}
}