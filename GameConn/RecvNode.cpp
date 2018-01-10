#include "RecvNode.h"
#include "Socket.h"
#include "EndPoint.h"
#include "RUDPLink.h"
#include "CoreNode.h"
#include "Platform.h"

#include <cassert>
#include <chrono>


namespace Zerodelay
{
	RecvNode::RecvNode(i32_t resendIntervalMs):
		m_IsClosing(false),
		m_ResendIntervalMs(resendIntervalMs),
		m_Socket(nullptr),
		m_RecvThread(nullptr),
		m_SendThread(nullptr),
		m_ListPinned(false)
	{
		m_CaptureSocketErrors = true;
	}

	RecvNode::~RecvNode()
	{
		reset();
	}

	void RecvNode::reset()
	{
		// destruct memory
		m_IsClosing = true;
		if ( m_Socket )
		{
			m_Socket->close();
		}
		if ( m_RecvThread && m_RecvThread->joinable() )
		{
			m_RecvThread->join();
		}
		if ( m_SendThread && m_SendThread->joinable() )
		{
			m_SendThread->join();
		}
		delete m_RecvThread;
		delete m_SendThread;
		for ( auto& kvp : m_OpenLinksMap )
		{
			delete kvp.second;
		}
		delete m_Socket;
		// reset state
		Platform::log("RecvNode reset called, num links %d.", (i32_t)m_OpenLinksMap.size());
		m_RecvThread = nullptr;
		m_SendThread = nullptr;
		m_OpenLinksMap.clear();
		m_OpenLinksList.clear();
		m_Socket = nullptr;
		m_ListPinned = false;
		m_IsClosing  = false;
		//
		// !! ptrs to other managers can be left in tact as well as user settings !!
		//
		// class CoreNode* m_CoreNode;
		// class ConnectionNode* m_ConnectionNode;
	}

	void RecvNode::postInitialize(CoreNode* coreNode)
	{
		assert(!m_CoreNode && !m_ConnectionNode);
		m_CoreNode = coreNode;
		m_ConnectionNode = coreNode->cn();
	}

	bool RecvNode::openSocketOnPort(u16_t port)
	{
		if (m_Socket)
			return true; // already opened
		if (!m_Socket) 
			m_Socket = ISocket::create();
		if (!m_Socket)
			return false;
		if (!m_Socket->open())
			return false;
		if (!m_Socket->bind(port))
			return false;
		return true;
	}

	void RecvNode::send(u8_t id,const i8_t* data,i32_t len,const EndPoint* specific,bool exclude,EHeaderPacketType type,u8_t channel,bool relay)
	{
		assert( type == EHeaderPacketType::Reliable_Ordered || type == EHeaderPacketType::Unreliable_Sequenced );
		if ( !(type == EHeaderPacketType::Reliable_Ordered || type == EHeaderPacketType::Unreliable_Sequenced) )
		{
			m_CoreNode->setCriticalError(ECriticalError::InvalidLogic, ZERODELAY_FUNCTION);
			return;
		}
		bool bWasSent = false;
		forEachLink( specific, exclude, true, [&] (RUDPLink* link)
		{
			link->addToSendQueue( id, data, len, type, channel, relay );
			bWasSent = true;
		});
		if (!bWasSent)
		{
			Platform::log("WARNING: data with id %d was not sent to anyone.", id);
		}
	}

	void RecvNode::sendReliableNewest(u8_t id, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const EndPoint* specific, bool exclude)
	{
		bool bWasSent = false;
		forEachLink( specific, exclude, true, [&] (RUDPLink* link)
		{
			link->addReliableNewest( id, data, len, groupId, groupBit );
			bWasSent = true;
		});
		if (!bWasSent)
		{
			Platform::log("WARNING: reliable newest data with id %d, group id %d and groupBit %d was not sent to anyone.", id, groupId, groupBit);
		}
	}

	RUDPLink* RecvNode::getLinkAndPinIt(u32_t idx)
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex); // pin requries this lock
		if ( idx < m_OpenLinksList.size() )
		{
			RUDPLink* link = m_OpenLinksList[idx];
			link->pin();
			return link;
		}
		return nullptr;
	}

	void RecvNode::unpinLink(RUDPLink* link)
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex); // unpin requires this lock
		assert(link);
		if (!link) return;
		link->unpin();
	}

	void RecvNode::pinList()
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex);
		m_ListPinned = true;
	}

	bool RecvNode::isListPinned() const
	{
		return m_ListPinned;
	}

	void RecvNode::unpinList()
	{
		m_ListPinned = false;
	}

	void RecvNode::simulatePacketLoss(i32_t percentage)
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex);
		for (auto& kvp : m_OpenLinksMap )
		{
			if ( !kvp.second->isPendingDelete() )
			{
				kvp.second->simulatePacketLoss( percentage );
			}
		}
	}

	void RecvNode::startThreads()
	{
		if ( m_RecvThread )
			return;
		m_RecvThread = new std::thread( [this] () { recvThread(); } );
		m_SendThread = new std::thread( [this] () { sendThread(); } );
	}

	i32_t RecvNode::getNumOpenLinks() const
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex);
		return (i32_t)m_OpenLinksList.size();
	}

	void RecvNode::recvThread()
	{
		EndPoint endPoint;
		while ( !m_IsClosing )
		{
			// non blocking sockets for testing purposes
			if ( !m_Socket->isBlocking() )
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}

			i8_t buff[ZERODELAY_BUFF_RECV_SIZE];
			i32_t rawSize = ZERODELAY_BUFF_RECV_SIZE;
			ERecvResult eResult = m_Socket->recv( buff, rawSize, endPoint );

			// discard socket interrupt 'errors' if closing
			if ( m_IsClosing )
				break;

			if ( eResult != ERecvResult::Succes || rawSize <= 0 )
			{
				// optionally capture the socket errors
				if ( m_CaptureSocketErrors )
				{
					i32_t err = m_Socket->getUnderlayingSocketError();
					if ( err != 0 )
					{
						Platform::log("WARNING: Socket error in recvPoint %d.", err);
					}
				}
				continue;
			}

			RUDPLink* link = getLink(endPoint, true);
			if ( link && link->isPendingDelete() )
			{
				// Only report messages after the link has stopped lingering, otherwise we may end up with many messages that were just send after disconnect
				// or if disconnect is re-transmitted very often due to high retransmission rate in reliable ordered protocol.
				i32_t timeSincePenDelete = link->getTimeSincePendingDelete();
				if ( timeSincePenDelete >= RUDPLink::sm_MaxLingerTimeMs )
				{ 
					buff[rawSize] = '\0';
					i8_t norm_id = 0;
					if ( rawSize > RUDPLink::off_Norm_Id )
					{
						norm_id = buff[RUDPLink::off_Norm_Id];
					}
					Platform::log("Ignoring data for conn %s as is pending delete... hdrId: %d data: %s dataId: %d, deleted for time %d ms.", link->getEndPoint().toIpAndPort().c_str(), buff[0], buff, norm_id, timeSincePenDelete);
				}
				continue;
			}
		
			if (!link) // try add
			{
				// add fails of previous already exists
				link = addLink(endPoint);
			}
			
			if (link) 
			{
				link->recvData( buff, rawSize );	
			}
		}
	}

	void RecvNode::sendThread()
	{
		while ( !m_IsClosing )
		{
			std::unique_lock<std::mutex> lock(m_OpenLinksMutex);
			m_SendThreadCv.wait_for( lock, std::chrono::milliseconds(m_ResendIntervalMs) );
			if ( m_IsClosing )
				return;
			for (auto& kvp : m_OpenLinksMap)
			{
				kvp.second->flushSendQueue( m_Socket );
			}
			updatePendingDeletes();
		}
	}

	void RecvNode::updatePendingDeletes()
	{
		// std::lock_guard<std::mutex> lock(m_OpenLinksMutex); Already acquired by send thread
		if ( isListPinned() ) 
			return;

		// If list is pinned, it cannot be pinned in the mean time because we have the OpenLinksMutex lock

		// Delete (memory wise) dead connections
		for ( auto it = m_OpenLinksList.begin(); it != m_OpenLinksList.end(); )
		{
			RUDPLink* link = *it;

			// When a link is placed in pending delete state, keep it in that for some time to ignore all remaining data that was still underway
			// and discard it silently without throwing warnings that it it received data for a connection that is already deleted or not connected.
			if ( link->isPinned() || !link->isPendingDelete() )
			{
				it++;
				continue;
			}

			// Keep lingering for some time..
			if ( link->getTimeSincePendingDelete() > RUDPLink::sm_MaxLingerTimeMs*2 )
			{
				// Actually delete the connection
				Platform::log("Link to %s deleted.", link->getEndPoint().toIpAndPort().c_str());
				m_OpenLinksMap.erase(link->getEndPoint());
				delete link;
				it = m_OpenLinksList.erase(it);
			}
			else
			{
				it++;
			}
		}
	}

	RUDPLink* RecvNode::getLink(const EndPoint& endPoint, bool getIfIsPendingDelete) const
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex);
		auto it = m_OpenLinksMap.find( endPoint );
		if ( it != m_OpenLinksMap.end() && (getIfIsPendingDelete || !it->second->isPendingDelete()) )
		{
			return it->second;
		}
		return nullptr;
	}

	class RUDPLink* RecvNode::addLink(const EndPoint& endPoint)
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex);
		auto it = m_OpenLinksMap.find( endPoint );
		if ( it == m_OpenLinksMap.end() )
		{
			Platform::log("Link to %s added.", endPoint.toIpAndPort().c_str());
			RUDPLink* link = new RUDPLink( this, endPoint );
			m_OpenLinksMap.insert( std::make_pair( endPoint, link ) );
			m_OpenLinksList.emplace_back( link );
			return link;
		}
		return nullptr;
	}

}