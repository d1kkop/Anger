#pragma once

#include "Platform.h"


namespace Zerodelay
{
	struct EndPoint
	{
		EndPoint() { Platform::initialize(); ::memset(this, 0, sizeof(*this)); }
		EndPoint( const std::string& service, u16_t port );
		bool operator==(const EndPoint& other) const;
		bool operator!=(const EndPoint& other) const { return !(*this == other); }
		bool equal(const EndPoint& other) const;

		// Use to see if a host can be connected to. If false is erturned, use getLastError to obtain more info.
		bool resolve( const std::string& name, u16_t port );

		// Return in canonical form.
		std::string toIpAndPort() const;

		u16_t getPortHostOrder() const;
		u16_t getPortNetworkOrder() const;
		u32_t getIpv4HostOrder() const;
		u32_t getIpv4NetworkOrder() const;
		i32_t getLastError() const { return m_LastError; }

		// Returns ptr to actual host data
		const void* getLowLevelAddr() const;
		// Returns size that contains host data
		i32_t   getLowLevelAddrSize() const;

		void setIpAndPortFromNetworkOrder( u32_t ip, u16_t port ); // network order is big endian
		void setIpAndPortFromHostOrder( u32_t ip, u16_t port );

		struct STLCompare
		{
			bool operator() (const EndPoint& left, const EndPoint& right) const { return compareLess(left,right)<0; }
		};

		static i32_t compareLess( const EndPoint& a, const EndPoint& b );

		i32_t write( i8_t* buff, i32_t bufSize ) const;
		i32_t read( const i8_t* buff, i32_t bufSize );

	private:
		i32_t m_LastError;

	#if ZERODELAY_WIN32SOCKET
		SOCKADDR_INET m_SockAddr;
	#endif

	#if ZERODELAY_SDLSOCKET
		IPaddress m_IpAddress;
	#endif
		
	#if ZERODELAY_FAKESOCKET
	public:
		class ISocket* m_Socket;
	#endif

	};
}