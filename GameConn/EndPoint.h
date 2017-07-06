#pragma once

#include "Platform.h"
#include "Zerodelay.h"

#include <cstring>
#include <mutex>


namespace Zerodelay
{
	struct EndPoint
	{
		EndPoint() { ::memset(this, 0, sizeof(*this)); }
		bool operator==(const EndPoint& other) const;
		bool operator!=(const EndPoint& other) const { return !(*this == other); }

		// Use to see if a host can be connected to. If false is erturned, use getLastError to obtain more info.
		bool resolve( const std::string& name, u16_t port );

		// Formats to common ipv4 notation or ipv6 notation and adds port to it witha double dot in between.
		// eg: 255.173.28.53:19234
		std::string asString() const;

		u16_t getPortHostOrder() const;
		u16_t getPortNetworkOrder() const;
		i32_t  getLastError() const { return m_LastError; }

		struct STLCompare
		{
			bool operator() (const EndPoint& left, const EndPoint& right) const { return compareLess(left,right)<0; }
		};

		static i32_t compareLess( const EndPoint& a, const EndPoint& b );

		i32_t write( i8_t* buff, i32_t len ) const;
		i32_t read( const i8_t* buff, i32_t len );

		const void* getLowLevelAddr() const { return &m_SockAddr; }
		i32_t   getLowLevelAddrSize() const { return sizeof(m_SockAddr); }

#ifdef _WIN32
	private:
		SOCKADDR_INET m_SockAddr;
#endif
		i32_t m_LastError;

#ifdef MOTOR_NETWORK_DEBUG
	public:
		class ISocket* m_Socket;
#endif
	};
}