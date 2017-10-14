#include "RecvPoint.h"
#include "Socket.h"
#include "EndPoint.h"
#include "VariableGroup.h"
#include "RUDPConnection.h"
#include "Platform.h"

#include <cassert>
#include <chrono>
using namespace std::chrono_literals;


namespace Zerodelay
{
	RecvPoint::RecvPoint(bool captureSocketErrors, i32_t sendThreadSleepTimeMs):
		m_IsClosing(false),
		m_CaptureSocketErrors(captureSocketErrors),
		m_SendThreadSleepTimeMs(sendThreadSleepTimeMs),
		m_ListenSocket(ISocket::create()),
		m_RecvThread(nullptr),
		m_SendThread(nullptr),
		m_UserPtr(nullptr),
		m_UserIndex(0)
	{
	}

	RecvPoint::~RecvPoint()
	{
		m_IsClosing = true;
		if ( m_ListenSocket )
		{
			m_ListenSocket->close();
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
		for ( auto& kvp : m_Connections )
		{
			delete kvp.second;
		}
		delete m_ListenSocket;
	}

	bool RecvPoint::send(u8_t id, const i8_t* data, i32_t len, const EndPoint* specific, bool exclude, EHeaderPacketType type, u8_t channel, bool relay, bool discardSendIfNotConnected)
	{
		assert( type == EHeaderPacketType::Reliable_Ordered || type == EHeaderPacketType::Unreliable_Sequenced );
		m_TempConnections.clear();
		copyConnectionsTo( m_TempConnections );
		// Only in case of UnreliableSequenced check for discardSendIfNotConnected.
		bool bWasAddedToQueue = false;
		if ( type == EHeaderPacketType::Reliable_Ordered )
		{
			forEachConnection( specific, exclude, [&] (auto* conn)
			{
				conn->addToSendQueue( id, data, len, type, channel, relay );
				bWasAddedToQueue = true;
			});
		}
		else
		{
			forEachConnection( specific, exclude, [&] (auto* conn)
			{
				if ( !discardSendIfNotConnected || conn->isConnected() )
				{
					conn->addToSendQueue( id, data, len, type, channel, relay );
					bWasAddedToQueue = true;
				}
			});
		}
		if ( 0 == m_TempConnections.size() ) 
		{
			char debugData[4048];
			Platform::memCpy( debugData, 4048, data, len );
			debugData[len] = '\0';
			Platform::log("WARNING: Trying to send reliable/unreliable data to 0 connections, perhaps not connected or already disconnected. HdrId: %d, data: %s, dataId: %d", (u8_t)type, debugData, data ? (u8_t)data[0] : 0 );
		}
		return bWasAddedToQueue;
	}

	void RecvPoint::sendReliableNewest(u8_t id, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const EndPoint* specific, bool exclude)
	{
		u32_t kNumSends = 0;
		m_TempConnections.clear();
		copyConnectionsTo( m_TempConnections );
		forEachConnection( specific, exclude, [&] (auto* conn)
		{
			conn->addReliableNewest( id, data, len, groupId, groupBit );
			kNumSends++;
		});
		if ( 0 == kNumSends )
		{
			Platform::log("WARNING: Trying to send reliable newest data to 0 connections, perhaps not connected or already disconnected");
		}
	}

	void RecvPoint::simulatePacketLoss(i32_t percentage)
	{
		std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
		for (auto& kvp : m_Connections )
		{
			if ( !kvp.second->isPendingDelete() )
			{
				kvp.second->simulatePacketLoss( percentage );
			}
		}
	}

	void RecvPoint::startThreads()
	{
		if ( m_RecvThread )
			return;
		m_RecvThread = new std::thread( [this] () { recvThread(); } );
		m_SendThread = new std::thread( [this] () { sendThread(); } );
	}

	void RecvPoint::copyConnectionsTo(std::vector<class IConnection*>& dstList)
	{
		std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
		for ( auto& kvp : m_Connections )
		{
			// If is pending delete because remotely called, or if disconnect called locally. Do no longer update network events.
			if ( !kvp.second->isPendingDelete() )
			{
				dstList.emplace_back( kvp.second );
			}
		}
	}

	void RecvPoint::recvThread()
	{
		EndPoint endPoint;
		while ( !m_IsClosing )
		{
			// non blocking sockets for testing purposes
			if ( !m_ListenSocket->isBlocking() )
			{
				std::this_thread::sleep_for(100ms);
			}

			i8_t buff[RecvPoint::sm_MaxRecvBuffSize];
			i32_t  rawSize = sm_MaxRecvBuffSize;
			auto eResult = m_ListenSocket->recv( buff, rawSize, endPoint );

			// discard socket interrupt 'errors' if closing
			if ( m_IsClosing )
				break;

			if ( eResult != ERecvResult::Succes || rawSize <= 0 )
			{
				// optionally capture the socket errors
				if ( m_CaptureSocketErrors )
				{
					i32_t err = m_ListenSocket->getUnderlayingSocketError();
					if ( err != 0 )
					{
						Platform::log("Socket error in recvPoint %d\n", err);
					}
				}
				continue;
			}

			IConnection* conn = nullptr;
			{
				std::lock_guard<std::mutex> lock(m_ConnectionListMutex);

				// Delete (memory wise) dead connections
				{
					for ( auto it = m_Connections.begin(); it != m_Connections.end(); )
					{
						conn = it->second;

						// Do not immediately delete a disconnecting client, because data may still be destined to this address.
						// If the client could reconnect immediately, we would also process the data of the previous session (not actually true because sender will have diff bound port).
						// So keep the client for some seconds in a lingering state.
						if ( !conn->isPendingDelete() )
						{
							it++;
							continue;
						}
						
						// Keep lingering for some time..
						if ( conn->getTimeSincePendingDelete() > conn->getLingerTimeMs() )
						{
							delete it->second;
							it = m_Connections.erase(it);
						}
						else
						{
							it++;
						}
					}
				}

				// Add new connections
				conn = nullptr;
				{
					auto it = m_Connections.find( endPoint );
					if ( it == m_Connections.end() )
					{
						conn = createNewConnection( endPoint );
						m_Connections.insert( std::make_pair( endPoint, conn ) );
					}
					else
					{
						conn = it->second;
						if ( !conn ) // Should always have a valid connection
						{
							m_Connections.erase( it );
						}
						else if ( conn->isPendingDelete() )
						{
							// Only report messages after the connection has stopped lingering, otherwise we may end up with many messages that were just send after disconnect
							// or if disconnect is re-transmitted very often due to high retransmission rate in reliable ordered protocol.
							if ( conn->getTimeSincePendingDelete() >= IConnection::sm_MaxLingerTimeMs*2 )
							{ 
								buff[rawSize] = '\0';
								i8_t norm_id = 0;
								if ( rawSize > RUDPConnection::off_Norm_Id )
								{
									norm_id = buff[RUDPConnection::off_Norm_Id];
								}
								Platform::log("Ignoring data for conn %s as is pending delete... hdrId %d, data %s, dataId %d", conn->getEndPoint().asString().c_str(), buff[0], buff, norm_id);
							}
							conn = nullptr;
						}
					}
				}
			}

			// let the connection recv and process the data
			if ( conn )
			{
				conn->recvData( buff, rawSize );	
			}
		}
	}

	void RecvPoint::sendThread()
	{
		while ( !m_IsClosing )
		{
			std::unique_lock<std::mutex> lock(m_ConnectionListMutex);
			m_SendThreadCv.wait_for( lock, std::chrono::milliseconds(m_SendThreadSleepTimeMs) );
			for (auto& kvp : m_Connections)
			{
				kvp.second->flushSendQueue( m_ListenSocket );
			}
		}
	}
}