#pragma once


#ifdef _WIN32
	#define RPC_EXPORT __declspec(dllexport)
#else
	#define RPC_EXPORT
#endif

#ifdef _WIN32
	#define ALIGN(n) __declspec(align(n))
#else
	#define ALIGN(n)
#endif

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

#include "Zerodelay.h"


namespace Zerodelay
{
	class Platform
	{
	public:
		// Call once before all network code. Returns 0 on succes, socket error code otherwise.
		static i32_t initialize();
		// Call upon application close or no network code is to be execute anymore
		static void shutdown();
		// Obtain ptr to address in executing img, given that the function was exported
		static void* getPtrFromName(const i8_t* name);
		// Do thread safe logging
		static void log(const i8_t* format, ...);

		static bool memCpy( void* dst, i32_t dstSize, const void* src, i32_t srcSize );
		static bool formatPrint( i8_t* dst, i32_t dstSize, const i8_t* frmt, ... );

	private:
		static bool wasInitialized;
		static std::mutex mapMutex;
		static std::mutex logMutex;
		static std::map<std::string, void*> name2RpcFunction;
	};
}