#pragma once


#ifdef _WIN32
#include <ws2tcpip.h>
#include <ws2ipdef.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

#ifdef linux
#include <sys/types.h>
#include <sys/socket.h>
#endif


namespace Motor
{
	namespace Anger
	{
		class Platform
		{
		public:
			// Call once before all network code. Returns 0 on succes, socket error code otherwise.
			static int initialize();
			// Call upon application close or no network code is to be execute anymore
			static void shutdown();
		};
	}
}