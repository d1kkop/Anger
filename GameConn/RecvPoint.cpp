#include "RecvPoint.h"
#include "Socket.h"
#include "EndPoint.h"
#include "RUDPConnection.h"

#include <chrono>
using namespace std::chrono_literals;


namespace Zeroone
{
	RecvPoint::RecvPoint(bool captureSocketErrors, int sendThreadSleepTimeMs):
		m_IsClosing(false),
		m_CaptureSocketErrors(captureSocketErrors),
		m_SendThreadSleepTimeMs(sendThreadSleepTimeMs),
		m_Socket(ISocket::create()),
		m_WasSpecific(nullptr),
		m_WasExclude(false),
		m_RecvThread(nullptr),
		m_SendThread(nullptr)
	{
	}

	RecvPoint::~RecvPoint()
	{
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
		for ( auto& kvp : m_Connections )
		{
			delete kvp.second;
		}
		delete m_Socket;
	}

	void RecvPoint::beginSend(const EndPoint* specific, bool exclude)
	{
		m_ConnectionListMutex.lock();
		m_WasSpecific = specific;
		m_WasExclude  = exclude;
		forEachConnection( specific, exclude, [] (IConnection* conn) 
		{
			conn->beginAddToSendQueue();
		});
	}

	void RecvPoint::send(unsigned char id, const char* data, int len, EPacketType type, unsigned char channel)
	{
		forEachConnection( m_WasSpecific, m_WasExclude, [=] (IConnection* conn) 
		{
			conn->addToSendQueue( id, data, len, type, channel );
		});
	}

	void RecvPoint::endSend()
	{
		forEachConnection( m_WasSpecific, m_WasExclude, [] (IConnection* conn) 
		{
			conn->endAddToSendQueue();
		});
		m_ConnectionListMutex.unlock();
	}

	void RecvPoint::simulatePacketLoss(int percentage)
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
			if ( !m_Socket->isBlocking() )
			{
				std::this_thread::sleep_for(100ms);
			}

			char buff[RecvPoint::sm_MaxRecvBuffSize];
			int  rawSize = sm_MaxRecvBuffSize;
			auto eResult = m_Socket->recv( buff, rawSize, endPoint );

			if ( eResult != ERecvResult::Succes || rawSize <= 0 )
			{
				// optionally capture the socket errors
				if ( m_CaptureSocketErrors )
				{
					int err = m_Socket->getUnderlayingSocketError();
					if ( err != 0 )
					{
						std::lock_guard<std::mutex> lock(m_ConnectionListMutex);
						m_SocketErrors.push_back( err );
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
						if ( kvp.second->isPendingDelete()  && 
							 kvp.second->getTimeSincePendingDelete() > 5000 ) // if longer than 5 sec in delete state, actually delete it
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
				if ( !kvp.second->isPendingDelete() ) // not mandatory but it has no use to keep flushing the send queue on a connection that will no longer be active
				{
					kvp.second->flushSendQueue( m_Socket );
				}
			}
		}
	}

}