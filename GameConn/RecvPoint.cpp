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

	void RecvPoint::send(u8_t id, const i8_t* data, i32_t len, const EndPoint* specific, bool exclude, EPacketType type, u8_t channel, bool relay)
	{
		std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
		forEachConnection( specific, exclude, [&] (IConnection* conn) 
		{
			conn->addToSendQueue( id, data, len, type, channel, relay );
		});
	}

	void RecvPoint::sendReliableNewest(u8_t id, u32_t groupId, i8_t groupBit, const i8_t* data, i32_t len, const EndPoint* specific, bool exclude, bool relay)
	{
		std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
		forEachConnection( specific, exclude, [&] (IConnection* conn ) 
		{
			conn->addReliableNewest( id, data, len, groupId, groupBit, relay );
		});
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
			if ( !kvp.second->isPendingDelete() )
			{
				dstList.emplace_back( kvp.second );
			}
		}
	}

	void RecvPoint::markIsPendingDelete(const std::vector<class IConnection*>& srcList)
	{
		std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
		for (auto& it : srcList)
		{
			it->setIsPendingDelete();
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
						if ( kvp.second->isPendingDelete() && kvp.second->getTimeSincePendingDelete() > 5000 ) // if longer than 5 sec in delete state, actually delete it
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
						if ( conn->isPendingDelete() )
						{
							Platform::log("ignoring data for %s, is pending delete..", conn->getEndPoint().asString().c_str());
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