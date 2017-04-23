#include "Socket.h"


namespace Motor
{
	namespace Anger
	{
		ISocket* ISocket::create()
		{
			return new BSDSocket();
		}

		bool ISocket::readString(char* buff, int buffSize, const char* buffIn, int buffInSize)
		{
			if ( !buff || !buffIn )
				return false;
			int k = 0;
			while ((*buffIn != '\0') && (k < buffSize-1) && (k < buffInSize))
			{
				*buff++ = *buffIn++;
				++k;
			}
			if ( buffSize > 0 )
			{
				*buff = '\0';
				return true;
			}
			return false;
		}

		bool ISocket::readFixed(char* dst, int dstSize, const char* buffIn, int buffInSize)
		{
			if ( !dst || !buffIn || buffInSize < dstSize )
				return false;
			memcpy( dst, buffIn, dstSize );
			return true;
		}

#ifdef MOTOR_NETWORK_DEBUG

		//////////////////////////////////////////////////////////////////////////
		// Fake Socket
		//////////////////////////////////////////////////////////////////////////

		FakeSocket::~FakeSocket()
		{
			for ( auto& p : m_Packets )
			{
				delete [] p.data;
			}
		}

		ESendResult FakeSocket::send( const struct EndPoint& endPoint, const char* data, int len )
		{
			// Normally, the endpoint is not to be altered but in the fake setup it owns the socket of the recipient
			// so that the data can be artificially placed there..
			FakeSocket* recvSock = const_cast<FakeSocket*>( dynamic_cast<const FakeSocket*>( endPoint.m_Socket ) );
			recvSock->storeData( data, len, const_cast<struct EndPoint&>(m_SourceEndPoint) );
			return ESendResult::Succes;
		}

		ERecvResult FakeSocket::recv( char* buff, int& kSize, EndPoint& endPoint )
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if ( !m_Packets.empty() )
			{
				FakePacket& pack = m_Packets.front();
				if ( pack.len > kSize )
				{
					return ERecvResult::Error;
				}
				else
				{
					memcpy( buff, pack.data, pack.len );
					kSize = pack.len;
					endPoint = pack.ep;
					delete [] pack.data;
					m_Packets.pop_front();
					return ERecvResult::Succes;
				}
			}
			return ERecvResult::NoData;
		}

		void FakeSocket::storeData( const char* data, int len, const EndPoint& srcPoint )
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			FakePacket pack;
			pack.data = new char[len];
			pack.len  = len;
			pack.ep   = srcPoint;
			memcpy( pack.data, data, len);
			m_Packets.emplace_back( pack );
		}
#endif

		BSDSocket::BSDSocket():
			m_LastError(0),
			m_Socket(INVALID_SOCKET)
		{
		}

		bool BSDSocket::open(IPProto ipv, bool reuseAddr)
		{
			m_IpProto = ipv;

			m_Socket = socket( (ipv == IPProto::Ipv4 ? AF_INET : AF_INET6), SOCK_DGRAM, IPPROTO_UDP) ;
			if (m_Socket == INVALID_SOCKET)
			{
				setLastError();
				return false;
			}

			BOOL bReuseAddr = reuseAddr ? TRUE : FALSE;
			if ( SOCKET_ERROR == setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, (char*)&bReuseAddr, sizeof(BOOL)) )
			{
				setLastError();
				return false;
			}

			return true;
		}

		bool BSDSocket::bind(unsigned short port)
		{
			if ( m_Socket == INVALID_SOCKET )
				return false;

			addrinfo hints;
			addrinfo *addrInfo = nullptr;

			memset(&hints, 0, sizeof(hints));

			hints.ai_family = m_IpProto == IPProto::Ipv4 ? PF_INET : PF_INET6;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;
			hints.ai_flags = AI_PASSIVE; // <-- means that we intend to bind the socket

			char portBuff[64];
#if _WIN32
			sprintf_s(portBuff, 64, "%d", port);
#else
			sprintf(portBuff, "%d", port);
#endif
			if (0 != getaddrinfo(nullptr, portBuff, &hints, &addrInfo))
			{
				setLastError();
				freeaddrinfo(addrInfo);
				return false;
			}

			// try binding on the found host addresses
			for (addrinfo* inf = addrInfo; inf != nullptr; inf = inf->ai_next)
			{
				if (0 == ::bind(m_Socket, inf->ai_addr, (int)inf->ai_addrlen))
				{
					freeaddrinfo(addrInfo);
					return true;
				}
			}
			
			setLastError();
			return false;
		}

		bool BSDSocket::close()
		{
			int result = 0;
			if ( m_Socket != INVALID_SOCKET )
			{
#if _WIN32
				result = closesocket( m_Socket );
#else	
				result = close( m_Socket );
#endif
				if ( result == SOCKET_ERROR )
				{
					setLastError();
				}
				m_Socket = INVALID_SOCKET;
			}
			else
			{
				// was already closed or never opened
				result = -1;
			}
			return (result == 0);
		}

		void BSDSocket::setLastError()
		{
			if ( m_Socket != INVALID_SOCKET )
			{
#if _WIN32
				m_LastError = WSAGetLastError();
#endif
			}
		}

		ESendResult BSDSocket::send(const EndPoint& endPoint, const char* data, int len)
		{
			if ( m_Socket == INVALID_SOCKET )
				return ESendResult::SocketClosed;
				
			const void* addr= endPoint.getLowLevelAddr();
			int addrSize	= endPoint.getLowLevelAddrSize();

			if ( SOCKET_ERROR == sendto( m_Socket, data, len, 0, (const sockaddr*)addr, addrSize ) )
			{
				setLastError();
				return ESendResult::Error;
			}

			return ESendResult::Succes;
		}

		ERecvResult BSDSocket::recv(char* buff, int& rawSize, EndPoint& endPoint)
		{
			if ( m_Socket == INVALID_SOCKET )
				return ERecvResult::SocketClosed;

			int addrSize = endPoint.getLowLevelAddrSize();
			rawSize = recvfrom(m_Socket, buff, rawSize, 0, (sockaddr*)endPoint.getLowLevelAddr(), &addrSize);

			if ( rawSize < 0 )
			{
				setLastError();
				return ERecvResult::Error;
			}

			return ERecvResult::Succes;
		}

		int BSDSocket::getUnderlayingSocketError() const
		{
			return m_LastError;
		}

	}
}