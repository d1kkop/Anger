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

	void RecvPoint::send(u8_t id, const i8_t* data, i32_t len, const EndPoint* specific, bool exclude, EHeaderPacketType type, u8_t channel, bool relay)
	{
		size_t kNumConnections = 0;
		{
			std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
			kNumConnections = m_Connections.size();
			forEachConnection( specific, exclude, [&] (IConnection* conn) 
			{
				conn->addToSendQueue( id, data, len, type, channel, relay );
			});
		}
		if ( 0 == kNumConnections )
		{
			Platform::log("Trying to send reliable/unreliable data to 0 connections, perhaps not connected or already disconnected");
		}
	}

	void RecvPoint::sendReliableNewest(u8_t id, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const EndPoint* specific, bool exclude)
	{
		size_t kNumConnections = 0;
		{
			std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
			kNumConnections = m_Connections.size();
			forEachConnection( specific, exclude, [&] (IConnection* conn ) 
			{
				conn->addReliableNewest( id, data, len, groupId, groupBit );
			});
		}
		if ( 0 == kNumConnections )
		{
			Platform::log("Trying to send reliable newest data to 0 connections, perhaps not connected or already disconnected");
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
			if ( !kvp.second->isPendingDelete() && !kvp.second->isDisconnectInvokedHere() )
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

			if ( eResult != ERecvResult::Succes || rawSize <= 0 )
			{
				// optionally capture the socket errors
				if ( m_CaptureSocketErrors )
				{
					i32_t err = m_ListenSocket->getUnderlayingSocketError();
					if ( err != 0 )
					{
						std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
						m_SocketErrors.push_back( err ); // TODO is this how we want it?
					}
				}
				continue;
			}

			IConnection* conn = nullptr;
			{
				std::lock_guard<std::mutex> lock(m_ConnectionListMutex);

				// Delete (memory wise) dead connections
				{
					for ( auto& kvp : m_Connections )
					{
						// Do not immediately delete a disconnecting client, because data may still be destined to this address.
						// If the client could reconnect immediately, we would also process the data of the previous session.
						// So keep the client for some seconds in a lingering state.
						if ( kvp.second->isPendingDelete() && /* If disconnect invoked here, immediately remove after done lingering. Else, keep in list for x seconds to avoid immediate reconnect. */
							(kvp.second->isDisconnectInvokedHere() || kvp.second->getTimeSincePendingDelete() > 8000) ) // Keep for 8 sec in connect list to avoid immediate reconnect
						{
							delete kvp.second;
							m_Connections.erase(kvp.first);
							break;
						}
					}
				}

				// Add new connections
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
								Platform::log("ignoring data for conn %s as is pending delete.... type: %d  data: %s", conn->getEndPoint().asString().c_str(), (i32_t)buff[0], buff);
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