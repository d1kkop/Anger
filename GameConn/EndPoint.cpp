#include "EndPoint.h"
#include "Platform.h"


namespace Zerodelay
{

	bool EndPoint::operator==(const EndPoint& other) const
	{
		return compareLess(*this, other) == 0;
	}

	std::string EndPoint::asString() const
	{
		Platform::initialize();

		// Ip
		i8_t ipBuff[128] = { 0 };

#if _WIN32
		inet_ntop(m_SockAddr.si_family, (PVOID)&m_SockAddr.Ipv4, ipBuff, 128);
#else
//#pragma error("no implement");
#endif

		// Port
		i8_t portBuff[32] = { 0 };
#if _WIN32
		sprintf_s(portBuff, 32, "%d", getPortHostOrder());
#else
		sprintf(portBuff, "%d", getPortHostOrder());
#endif
		return std::string(ipBuff) + ":" + portBuff;
	}

	bool EndPoint::resolve(const std::string& name, u16_t port)
	{
		addrinfo hints;
		addrinfo *addrInfo = nullptr;

		memset(&hints, 0, sizeof(hints));

		hints.ai_family   = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		// hints.ai_flags = AI_PASSIVE; <-- use this for binding, otherwise connecting

		i8_t ipBuff[128];
		i8_t portBuff[32];
#if _WIN32
		sprintf_s(ipBuff, 128, "%s", name.c_str());
		sprintf_s(portBuff, 32, "%d", port);
#else
		sprintf(ipBuff, "%s", name.c_str());
		sprintf(portBuff, "%d", port);
#endif
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
	}

	u16_t EndPoint::getPortHostOrder() const
	{
		return ntohs( getPortNetworkOrder() );
	}

	u16_t EndPoint::getPortNetworkOrder() const
	{
#if _WIN32
		return m_SockAddr.Ipv4.sin_port;
#else
//#pragma error
#endif
	}

	i32_t EndPoint::compareLess(const EndPoint& a, const EndPoint& b)
	{
		if ( a.getLowLevelAddrSize() < b.getLowLevelAddrSize() ) return -1;
		if ( a.getLowLevelAddrSize() > b.getLowLevelAddrSize() ) return 1;
		return ::memcmp( a.getLowLevelAddr(), b.getLowLevelAddr(), a.getLowLevelAddrSize() ) ;
	}

	i32_t EndPoint::write(i8_t* buff, i32_t len) const
	{
		i32_t addrSize = getLowLevelAddrSize();
		if ( len >= addrSize )
		{
			memcpy_s( buff, addrSize, getLowLevelAddr(), addrSize );
			return addrSize;
		}
		return -1;
	}

	i32_t EndPoint::read(const i8_t* buff, i32_t len)
	{
		i32_t addrSize = getLowLevelAddrSize();
		if ( len >= addrSize )
		{
			memcpy_s( &m_SockAddr, addrSize, buff, addrSize );
			return addrSize;
		}
		return -1;
	}

}