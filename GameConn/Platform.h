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

#include <string>
#include <map>
#include <mutex>


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
			// Obtain ptr to address in executing img, given that the function was exported
			static void* getPtrFromName(const char* name);
		private:
			static std::mutex mapMutex;
			static std::map<std::string, void*> name2RpcFunction;
		};
	}
}