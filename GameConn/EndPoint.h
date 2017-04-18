#pragma once

#include "Platform.h"

#include <cstring>
#include <mutex>


namespace Motor
{
	namespace Anger
	{
		struct EndPoint
		{
			EndPoint() { ::memset(this, 0, sizeof(*this)); }
			bool operator==(const EndPoint& other) const;
			bool operator!=(const EndPoint& other) const { return !(*this == other); }

			// Use to see if a host can be connected to. If false is erturned, use getLastError to obtain more info.
			bool resolve( const std::string& name, unsigned short port );

			// Formats to common ipv4 notation or ipv6 notation and adds port to it witha double dot in between.
			// eg: 255.173.28.53:19234
			std::string asString() const;

			unsigned short getPortHostOrder() const;
			unsigned short getPortNetworkOrder() const;
			int  getLastError() const { return m_LastError; }

			struct STLCompare
			{
				bool operator() (const EndPoint& left, const EndPoint& right) const { return compareLess(left,right)<0; }
			};

			static int compareLess( const EndPoint& a, const EndPoint& b );

			int write( char* buff, int len ) const;
			int read( const char* buff, int len );

			const void* getLowLevelAddr() const { return &m_SockAddr; }
			int   getLowLevelAddrSize() const { return sizeof(m_SockAddr); }

#ifdef _WIN32
		private:
			SOCKADDR_INET m_SockAddr;
#endif
			int m_LastError;

#ifdef MOTOR_NETWORK_DEBUG
		public:
			class ISocket* m_Socket;
#endif
		};
	}
}