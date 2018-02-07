#include "EndPoint.h"
#include "Platform.h"
#include "Util.h"

#include <cassert>


namespace Zerodelay
{
	bool EndPoint::operator==(const EndPoint& other) const
	{
		return equal(other);
	}

	bool EndPoint::equal(const EndPoint& other) const
	{
		return compareLess(*this, other) == 0;
	}

	std::string EndPoint::toIpAndPort() const
	{
	#if ZERODELAY_WIN32SOCKET
		// Ip
		i8_t ipBuff[128] = { 0 };
		inet_ntop(m_SockAddr.si_family, (PVOID)&m_SockAddr.Ipv4, ipBuff, 128);
		return ipBuff;
	#endif

	#if ZERODELAY_SDLSOCKET
		IPaddress iph;
		iph.host = Util::hton(m_IpAddress.host);
		iph.port = Util::hton(m_IpAddress.port);
		u8_t* ip = (u8_t*)&iph.host;
		//std::string ip = SDLNet_ResolveIP(&iph); // expects in host form <-- is superrrr slow function
		char buff[1024];
		// todo big endian order on big endian machine
		Platform::formatPrint(buff, 1024, "%d.%d.%d.%d:%d", ip[3], ip[2], ip[1], ip[0], iph.port);
		return std::string(buff);
	#endif

		return "";
	}

	EndPoint::EndPoint(const std::string& service, u16_t port)
	{
		Platform::initialize();
		resolve(service, port);
	}

	bool EndPoint::resolve(const std::string& name, u16_t port)
	{
	#if ZERODELAY_WIN32SOCKET
		addrinfo hints;
		addrinfo *addrInfo = nullptr;

		memset(&hints, 0, sizeof(hints));

		hints.ai_family   = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		// hints.ai_flags = AI_PASSIVE; <-- use this for binding, otherwise connecting

		i8_t ipBuff[128];
		i8_t portBuff[32];
		sprintf_s(ipBuff, 128, "%s", name.c_str());
		sprintf_s(portBuff, 32, "%d", port);

		m_LastError = getaddrinfo(ipBuff, portBuff, &hints, &addrInfo);
		if (m_LastError != 0)
		{
			freeaddrinfo(addrInfo);
			return false;
		}

		// try binding on the found host addresses
		for (addrinfo* inf = addrInfo; inf != nullptr; inf = inf->ai_next)
		{
			memcpy( &m_SockAddr, inf->ai_addr, (i32_t)inf->ai_addrlen );
			freeaddrinfo(addrInfo);
			return true;
		}

		freeaddrinfo(addrInfo);
		return false;
	#endif

	#if ZERODELAY_SDLSOCKET
		// put port in network order
		if ( 0 == SDLNet_ResolveHost( &m_IpAddress, name.c_str(), port ) )
		{
			return true;
		}
	#endif

		return false;
	}

	u16_t EndPoint::getPortHostOrder() const
	{
		return Util::hton(getPortNetworkOrder());
	}

	u16_t EndPoint::getPortNetworkOrder() const
	{
	#if ZERODELAY_WIN32SOCKET
		return m_SockAddr.Ipv4.sin_port;
	#endif
	#if ZERODELAY_SDLSOCKET
		return m_IpAddress.port;
	#endif	
		return (u16_t)-1;
	}

	u32_t EndPoint::getIpv4HostOrder() const
	{
		return Util::hton(getIpv4NetworkOrder());
	}

	u32_t EndPoint::getIpv4NetworkOrder() const
	{
	#if ZERODELAY_WIN32SOCKET
		return m_SockAddr.Ipv4.sin_addr.S_un.S_addr;
	#endif
	#if ZERODELAY_SDLSOCKET
		return m_IpAddress.host;
	#endif	
		return (u32_t)-1;
	}

	const void* EndPoint::getLowLevelAddr() const
	{
	#if ZERODELAY_WIN32SOCKET
		return &m_SockAddr;
	#endif
	#if ZERODELAY_SDLSOCKET
		return &m_IpAddress;
	#endif	
		assert(0);
		return nullptr;
	}

	i32_t EndPoint::getLowLevelAddrSize() const
	{
	#if ZERODELAY_WIN32SOCKET
		return sizeof(m_SockAddr);
	#endif
	#if ZERODELAY_SDLSOCKET
		return sizeof(m_IpAddress);
	#endif	
		assert(0);
		return 0;
	}

	void EndPoint::setIpAndPortFromNetworkOrder(u32_t ip, u16_t port)
	{
	#if ZERODELAY_WIN32SOCKET
		memset(&m_SockAddr, 0, sizeof(m_SockAddr));
		m_SockAddr.Ipv4.sin_port = port;
		m_SockAddr.Ipv4.sin_addr.S_un.S_addr = ip;
		m_SockAddr.si_family = AF_INET;
		m_SockAddr.Ipv4.sin_family = AF_INET;
	#endif
	#if ZERODELAY_SDLSOCKET
		m_IpAddress.host = ip;
		m_IpAddress.port = port;
	#endif	
	}

	void EndPoint::setIpAndPortFromHostOrder(u32_t ip, u16_t port)
	{
		setIpAndPortFromNetworkOrder(Util::hton(ip), Util::hton(port));
	}

	i32_t EndPoint::compareLess(const EndPoint& a, const EndPoint& b)
	{
		//if ( a.getLowLevelAddrSize() < b.getLowLevelAddrSize() ) return -1;
		//if ( a.getLowLevelAddrSize() > b.getLowLevelAddrSize() ) return 1;
		return ::memcmp( a.getLowLevelAddr(), b.getLowLevelAddr(), a.getLowLevelAddrSize() ) ;
	}

	i32_t EndPoint::write(i8_t* buff, i32_t bufSize) const
	{
		u16_t port = getPortNetworkOrder();
		u32_t ipv4 = getIpv4NetworkOrder();
		if ( bufSize >= 6 )
		{
			Platform::memCpy( buff, 4, &ipv4, 4 );
			Platform::memCpy( buff+4, 2, &port, 2 );
			return 6;
		}
		return -1;
	}

	i32_t EndPoint::read(const i8_t* buff, i32_t bufSize)
	{
		if ( bufSize >= 6 )
		{
			u32_t ipv4;
			u16_t port;
			Platform::memCpy( &ipv4, 4, buff, 4 );
			Platform::memCpy( &port, 2, buff+4, 2 );
			setIpAndPortFromNetworkOrder( ipv4, port );
			return 6;
		}
		return -1;
	}

}