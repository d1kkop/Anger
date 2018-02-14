#include "RecvNode.h"
#include "Socket.h"
#include "EndPoint.h"
#include "RUDPLink.h"
#include "CoreNode.h"
#include "Platform.h"
#include "ConnectionNode.h"
#include "Util.h"

#include <cassert>
#include <chrono>


namespace Zerodelay
{
	RecvNode::RecvNode(u32_t sendRelNewestIntervalMs, u32_t ackAggregateTimeMs):
		m_IsClosing(false),
		m_SendRelNewestIntervalMs(sendRelNewestIntervalMs),
		m_AckAggregateTimeMs(ackAggregateTimeMs),
		m_Socket(nullptr),
		m_RecvThread(nullptr),
		m_SendThread(nullptr),
		m_ListPinned(0)
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

	ESendCallResult RecvNode::send(u8_t id, const i8_t* data,i32_t len, const EndPoint* specific, bool exclude, EHeaderPacketType type,
								   u8_t channel, bool relay, std::vector<ZAckTicket>* deliveryTraceOut)
	{
		assert( type == EHeaderPacketType::Reliable_Ordered || type == EHeaderPacketType::Unreliable_Sequenced );
		if ( !(type == EHeaderPacketType::Reliable_Ordered || type == EHeaderPacketType::Unreliable_Sequenced) )
		{
			m_CoreNode->setCriticalError(ECriticalError::InvalidLogic, ZERODELAY_FUNCTION_LINE);
			return ESendCallResult::InternalError;
		}
		u32_t listCount;
		ESendCallResult sendResult = ESendCallResult::NotSent;
		forEachLink( specific, exclude, true, listCount, [&] (RUDPLink* link)
		{
			ESendCallResult individualResult;
			if (deliveryTraceOut)
			{
				u32_t sequence;
				u32_t numFragments;
				individualResult = link->addToSendQueue( id, data, len, type, channel, relay, &sequence, &numFragments );
				Util::addTraceCallResult(deliveryTraceOut, link->getEndPoint(), ETraceCallResult::Tracking, sequence, numFragments, channel);
			}
			else
			{
				individualResult = link->addToSendQueue( id, data, len, type, channel, relay );
			}
			if ( sendResult == ESendCallResult::NotSent && individualResult == sendResult )
			{
				sendResult = ESendCallResult::Succes;
			}
		});
		if (sendResult == ESendCallResult::NotSent && !exclude && listCount == 1)
		{
			Platform::log("WARNING: Data with id %d was not sent to anyone.", id);
		}
		return sendResult;
	}

	void RecvNode::sendReliableNewest(u8_t id, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const EndPoint* specific, bool exclude)
	{
		bool bWasSent = false;
		u32_t listCount;
		forEachLink( specific, exclude, true, listCount, [&] (RUDPLink* link)
		{
			link->addReliableNewest( id, data, len, groupId, groupBit );
			bWasSent = true;
		});
		if (!bWasSent)
		{
			Platform::log("WARNING: Reliable newest data with id %d, group id %d and groupBit %d was not sent to anyone.", id, groupId, groupBit);
		}
	}

	RUDPLink* RecvNode::getLinkAndPinIt(u32_t idx) const
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

	class RUDPLink* RecvNode::getLinkAndPinIt(const EndPoint& endpoint) const
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex); // pin requries this lock
		auto it = m_OpenLinksMap.find(endpoint);
		if ( it != m_OpenLinksMap.end() )
		{
			RUDPLink* link = it->second;
			link->pin();
			return link;
		}
		return nullptr;
	}

	void RecvNode::unpinLink(RUDPLink* link) const
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex); // unpin requires this lock
		assert(link);
		if (!link) return;
		link->unpin();
	}

	void RecvNode::pinList()
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex);
		m_ListPinned++;
	}

	bool RecvNode::isListPinned() const
	{
		return m_ListPinned != 0;
	}

	void RecvNode::unpinList()
	{
		m_ListPinned--;
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

	bool RecvNode::isPacketDelivered(const ZEndpoint& ztp, u32_t sequence, u32_t numFragments, i8_t channel) const
	{
		RUDPLink* link = getLinkAndPinIt( Util::toEtp(ztp) );
		if ( link ) 
		{
			bool bAllDelivered = true;
			for (u32_t i=0; i<numFragments; i++)
			{
				if (!link->isSequenceDelivered(sequence + i, channel))
				{
					bAllDelivered = false;
					break;
				}
			}
			unpinLink( link );
			return bAllDelivered;
		}
		return false;
	}

	void RecvNode::recvThread()
	{
		EndPoint endPoint;
		i32_t lastUpdateTS = 0;
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

			// do occasional administration updates k times/sec
			if (Util::getTimeSince(lastUpdateTS) >= 200)
			{
				updatePendingDeletes();
				lastUpdateTS = Util::timeNow();
			}

			if ( eResult != ERecvResult::Succes )
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

			if ( rawSize < RUDPLink::hdr_Generic_Size )
			{
				Platform::log("WARNING: Incoming packet smaller than hdr size, dropping packet.");
				continue;
			}

			// get link, even if is pending delete
			RUDPLink* link = getLink (endPoint, true);
			if ( link && link->isPendingDelete() )
			{
				// Only report messages after the link has stopped lingering, otherwise we may end up with many messages that were just send after disconnect
				// or if disconnect is re-transmitted very often due to high retransmission rate in reliable ordered protocol.
				i8_t norm_id  = -1;
				if ( rawSize > RUDPLink::off_Norm_Id ) norm_id = buff[RUDPLink::off_Norm_Id];
				buff[rawSize] = '\0';

				i32_t timeSincePenDelete = link->getTimeSincePendingDelete();
				if ( timeSincePenDelete >= RUDPLink::sm_MaxLingerTimeMs )
				{ 
					Platform::log("WARNING: Ignoring data for link %s (id %d) as is pending delete and more than %dms lingered (%dms). HdrId: %d dataId: %d payload: %s.",
								  link->getEndPoint().toIpAndPort().c_str(), link->id(), RUDPLink::sm_MaxLingerTimeMs, timeSincePenDelete, buff[RUDPLink::off_Type], norm_id, buff);
					continue;
				}
				else
				{
					Platform::log("Receiving data on %s (id %d) %dms after became pending delete. HdrId: %d dataId: %d payload: %s.",
								  link->getEndPoint().toIpAndPort().c_str(), link->id(), timeSincePenDelete, buff[RUDPLink::off_Type], norm_id, buff);
				}
			}
		
			u32_t linkId = *(u32_t*)(buff + RUDPLink::off_Link);
			if (!link) // add must be succesful if link wasnt found
			{
				// if not known link, first packet MUST be a connect packet, otherwise discard it
				// this is an early out routine to avoid going through the whole connection node for all 'random' packets that come in
				if ( rawSize < RUDPLink::off_Norm_Data || 
					 buff[RUDPLink::off_Type] != (i8_t)EHeaderPacketType::Reliable_Ordered || 
					 buff[RUDPLink::off_Norm_Id] != (i8_t)EDataPacketType::ConnectRequest )
				{
					u32_t linkId = 0;
					if ( rawSize >= 4 ) { linkId = *(u32_t*)(buff + RUDPLink::off_Link); }
					Platform::log("WARNING: Ignoring data for link %s (id %d) as packet was not a connect request packet and connection was not know.",
								   endPoint.toIpAndPort().c_str(), linkId );
					continue;
				}
				link = addLink(endPoint, &linkId);
				assert(link);
			}
			else if ( linkId != link->id() )
			{
				// Fail if link id's dont match
				i8_t hdrType  = -1;
				i8_t dataType = -1;
				if ( rawSize >= RUDPLink::off_Type+1 ) hdrType = buff[RUDPLink::off_Type];
				if ( rawSize >= RUDPLink::off_Norm_Id+1 ) dataType = buff[RUDPLink::off_Norm_Id];
				Platform::log("WARNING: Dropping packet because link id does not match. Incoming %d, having %d, hdrType %d dataType %d.",
								linkId, link->id(), hdrType, dataType);
				continue;
			}

			link->recvData( buff, rawSize );
		}
	}

	void RecvNode::sendThread()
	{
		u32_t ackAccumTime = 0;
		u32_t relNewAccumTime = 0;
		while ( !m_IsClosing )
		{
			u32_t lowestLatency = ~0UL;
			std::unique_lock<std::mutex> lock(m_OpenLinksMutex);
			for (auto l : m_OpenLinksList)
			{
				u32_t lat = l->getLatency();
				lowestLatency = Util::min(lat, lowestLatency);
			}
			u32_t waitTime = Util::min(lowestLatency, Util::min(m_SendRelNewestIntervalMs, m_AckAggregateTimeMs));
			m_SendThreadCv.wait_for( lock, std::chrono::milliseconds(waitTime) );
			if ( m_IsClosing )
				return;
			for (auto l : m_OpenLinksList)
			{
				l->dispatchRelOrderedQueueIfLatencyTimePassed(waitTime, m_Socket);
			}
			ackAccumTime += waitTime;
			if (ackAccumTime >= m_AckAggregateTimeMs) 
			{
				ackAccumTime -= m_AckAggregateTimeMs;
				for (auto l : m_OpenLinksList)
				{
					l->dispatchAckQueue(m_Socket);
					l->dispatchRelNewestAckQueue(m_Socket);
				}
			}
			relNewAccumTime += waitTime;
			if (waitTime >= m_SendRelNewestIntervalMs)
			{
				relNewAccumTime -= m_SendRelNewestIntervalMs;
				for (auto l : m_OpenLinksList)
					l->dispatchRelNewestAckQueue(m_Socket);
			}
		}
	}

	void RecvNode::updatePendingDeletes()
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex);
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
				Platform::log("Link to %s id: %d deleted.", link->getEndPoint().toIpAndPort().c_str(), link->id());
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

	class RUDPLink* RecvNode::addLink(const EndPoint& endPoint, const u32_t* linkIdPtr)
	{
		std::lock_guard<std::mutex> lock(m_OpenLinksMutex);
		auto it = m_OpenLinksMap.find( endPoint );
		if ( it == m_OpenLinksMap.end() )
		{
			u32_t linkId;
			if ( linkIdPtr ) linkId = *linkIdPtr;
			else 
			{ 
				linkId = rand();
				Platform::log("New LinkId %d generated.", linkId);
			}
			Platform::log("Link to %s (id %d) added.", endPoint.toIpAndPort().c_str(), linkId);
			RUDPLink* link = new RUDPLink( this, endPoint, linkId );
			m_OpenLinksMap[endPoint] = link;
			m_OpenLinksList.emplace_back( link );
			return link;
		}
		return nullptr;
	}

}