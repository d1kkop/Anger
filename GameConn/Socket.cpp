#include "Socket.h"
#include "Platform.h"

#include <cassert>


namespace Zerodelay
{
	ISocket::ISocket():
		m_Open(false),
		m_Bound(false),
		m_IpProto(IPProto::Ipv4),
		m_LastError(SocketError::Succes)
	{
	}

	ISocket* ISocket::create()
	{
		Platform::initialize();
	#if ZERODELAY_WIN32SOCKET
		return new BSDSocket();
	#endif
	#if ZERODELAY_SDLSOCKET
		return new SDLSocket();
	#endif
		return nullptr;
	}

#if ZERODELAY_FAKESOCKET

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

	ESendResult FakeSocket::send( const struct EndPoint& endPoint, const i8_t* data, i32_t len )
	{
		// Normally, the endpoint is not to be altered but in the fake setup it owns the socket of the recipient
		// so that the data can be artificially placed there..
		FakeSocket* recvSock = const_cast<FakeSocket*>( dynamic_cast<const FakeSocket*>( endPoint.m_Socket ) );
		recvSock->storeData( data, len, const_cast<struct EndPoint&>(m_SourceEndPoint) );
		return ESendResult::Succes;
	}

	ERecvResult FakeSocket::recv( i8_t* buff, i32_t& kSize, EndPoint& endPoint )
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

	void FakeSocket::storeData( const i8_t* data, i32_t len, const EndPoint& srcPoint )
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		FakePacket pack;
		pack.data = new i8_t[len];
		pack.len  = len;
		pack.ep   = srcPoint;
		memcpy( pack.data, data, len);
		m_Packets.emplace_back( pack );
	}
#endif

#if ZERODELAY_WIN32SOCKET

	//////////////////////////////////////////////////////////////////////////
	// BSDWin32 Socket
	//////////////////////////////////////////////////////////////////////////

	BSDSocket::BSDSocket():
		m_Socket(INVALID_SOCKET)
	{
		m_Blocking = true;
	}

	bool BSDSocket::open(IPProto ipv, bool reuseAddr)
	{
		if ( m_Open && m_Socket != INVALID_SOCKET )
		{
			return true;
		}
		// reset open state if was invalid socket
		m_Open = false;

		m_IpProto = ipv;

		m_Socket = socket( (ipv == IPProto::Ipv4 ? AF_INET : AF_INET6), SOCK_DGRAM, IPPROTO_UDP) ;
		if (m_Socket == INVALID_SOCKET)
		{
			setLastError();
			return false;
		}

		BOOL bReuseAddr = reuseAddr ? TRUE : FALSE;
		if ( SOCKET_ERROR == setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, (i8_t*)&bReuseAddr, sizeof(BOOL)) )
		{
			setLastError();
			return false;
		}

		m_Open = true;
		return true;
	}

	bool BSDSocket::bind(u16_t port)
	{
		if ( m_Socket == INVALID_SOCKET )
			return false;

		if ( m_Bound )
			return true;

		addrinfo hints;
		addrinfo *addrInfo = nullptr;

		memset(&hints, 0, sizeof(hints));

		hints.ai_family = m_IpProto == IPProto::Ipv4 ? PF_INET : PF_INET6;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_flags = AI_PASSIVE; // <-- means that we intend to bind the socket

		i8_t portBuff[64];
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
			if (0 == ::bind(m_Socket, inf->ai_addr, (i32_t)inf->ai_addrlen))
			{
				freeaddrinfo(addrInfo);
				m_Bound = true;
				return true;
			}
		}
			
		setLastError();
		return false;
	}

	bool BSDSocket::close()
	{
		i32_t result = 0;
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
		m_Open  = false;
		m_Bound = false;
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

	ESendResult BSDSocket::send(const EndPoint& endPoint, const i8_t* data, i32_t len)
	{
		if ( m_Socket == INVALID_SOCKET )
			return ESendResult::SocketClosed;
				
		const void* addr= endPoint.getLowLevelAddr();
		i32_t addrSize	= endPoint.getLowLevelAddrSize();

		if ( SOCKET_ERROR == sendto( m_Socket, data, len, 0, (const sockaddr*)addr, addrSize ) )
		{
			setLastError();
			return ESendResult::Error;
		}

		return ESendResult::Succes;
	}

	ERecvResult BSDSocket::recv(i8_t* buff, i32_t& rawSize, EndPoint& endPoint)
	{
		if ( m_Socket == INVALID_SOCKET )
			return ERecvResult::SocketClosed;

		i32_t addrSize = endPoint.getLowLevelAddrSize();
		rawSize = recvfrom(m_Socket, buff, rawSize, 0, (sockaddr*)endPoint.getLowLevelAddr(), &addrSize);

		if ( rawSize < 0 )
		{
			setLastError();
			return ERecvResult::Error;
		}

		return ERecvResult::Succes;
	}

#endif


#if ZERODELAY_SDLSOCKET

	//////////////////////////////////////////////////////////////////////////
	// BSDWin32 Socket
	/////////////////////////////////////////////////////////////////////////

	SDLSocket::SDLSocket():
		m_Socket(nullptr),
		m_SocketSet(nullptr)
	{
		m_Blocking = true;
		m_SocketSet = SDLNet_AllocSocketSet(1);
		if (!m_SocketSet)
		{
			m_LastError = SocketError::CannotCreateSet;
		}
	}

	SDLSocket::~SDLSocket()
	{
		assert (!m_Open);
		close();
		SDLNet_FreeSocketSet(m_SocketSet);
	}

	bool SDLSocket::open(IPProto ipProto, bool reuseAddr)
	{
		if (m_LastError!=SocketError::Succes) return false;
		m_Open = true;
		return m_Open;
	}

	bool SDLSocket::bind(u16_t port)
	{
		if (m_LastError!=SocketError::Succes) return false;
		if (!m_Open) 
		{
			m_LastError = SocketError::NotOpened;
			return false;
		}
		if (m_Bound) return true;
		assert(m_SocketSet && !m_Socket);
		m_Socket = SDLNet_UDP_Open(port);
		if (!m_Socket) 
		{
			// binding multiple times on same port already returns -1 (as it turns out)
			m_LastError = SocketError::PortAlreadyInUse;
			//m_LastError = SocketError::CannotOpen;
			return false;
		}
		IPaddress* ip =  SDLNet_UDP_GetPeerAddress(m_Socket, -1);
		if (!ip)
		{
			m_LastError = SocketError::CannotResolveLocalAddress;
			return false;
		}
		int channel = SDLNet_UDP_Bind(m_Socket, 0, ip);
		if (channel != 0)
		{
			m_LastError = SocketError::PortAlreadyInUse;
			return false;
		}
		if (-1 == SDLNet_UDP_AddSocket(m_SocketSet, m_Socket))
		{
			m_LastError = SocketError::CannotAddToSet;
			return false;
		}
		m_Bound = true;
		return m_Bound;
	}

	bool SDLSocket::close()
	{
		m_Open = false;
		if (m_Socket != nullptr)
		{
			if (m_Bound)
			{
				SDLNet_UDP_Unbind(m_Socket, 0);
				SDLNet_UDP_DelSocket(m_SocketSet, m_Socket); // ignore errors here as is closing routine
			}
			m_Bound = false;
			SDLNet_UDP_Close(m_Socket);
			m_Socket = nullptr;
		}
		// clean it all
		m_LastError = SocketError::Succes;
		return true;
	}

	ESendResult SDLSocket::send( const struct EndPoint& endPoint, const i8_t* data, i32_t len )
	{
		if ( m_Socket == nullptr )
			return ESendResult::SocketClosed;

		IPaddress dstIp;
		dstIp.host = endPoint.getIpv4NetworkOrder();
		dstIp.port = endPoint.getPortNetworkOrder();

		UDPpacket pack;
		pack.len     = len;
		pack.maxlen  = len;
		pack.address = dstIp;
		pack.data    = (Uint8*)data;
//		Platform::memCpy( pack.data, len, data, len );

		if ( 1 != SDLNet_UDP_Send( m_Socket, -1, &pack ) )
		{
			m_LastError = SocketError::SendFailure;
			Platform::log("SDL send udp packet error %s", SDLNet_GetError());
			return ESendResult::Error;
		}

//		SDLNet_FreePacket(pack);
		return ESendResult::Succes;
	}

	ERecvResult SDLSocket::recv( i8_t* buff, i32_t& rawSize, struct EndPoint& endPoint )
	{
		if (!m_Socket || !m_SocketSet)
			return ERecvResult::SocketClosed;

		i32_t res = SDLNet_CheckSockets( m_SocketSet, 100 );
		if (!m_Open || !m_Socket) // if closing, ignore error
			return ERecvResult::SocketClosed;
		if ( res == -1 )
			return ERecvResult::Error;
		if ( res == 0 || !SDLNet_SocketReady(m_Socket) )
			return ERecvResult::NoData;

		UDPpacket packet = { 0 };
		packet.data = (Uint8 *) buff;
		packet.len  = rawSize;
		packet.maxlen = rawSize;
		i32_t numPackets = SDLNet_UDP_Recv( m_Socket, &packet );
		
		if (!m_Open)
		{
			return ERecvResult::SocketClosed;
		}

		if ( numPackets == 0 )
		{
			return ERecvResult::NoData;
		}

		if ( -1 == numPackets )
		{
			Platform::log("SDL recv error %s.:", SDLNet_GetError());
			m_LastError = SocketError::RecvFailure;
			return ERecvResult::Error;
		}

		rawSize = packet.len;
		endPoint.setIpAndPortFromNetworkOrder( packet.address.host, packet.address.port );
//		Platform::memCpy( buff, rawSize, packet.data, packet.len );

		return ERecvResult::Succes;
	}

#endif
}