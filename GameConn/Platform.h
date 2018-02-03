#pragma once


#define ZERODELAY_FAKESOCKET							(0)
#define ZERODELAY_WIN32SOCKET							(0)
#define ZERODELAY_SDLSOCKET								(1)
#define ZERODELAY_SDL									(1)
#define ZERODELAY_BUFF_SIZE								(2048)	// send buff size
#define ZERODELAY_BUFF_RECV_SIZE						(3000)  // recvbuff size

#define ZERODELAY_LIL_ENDIAN						(1)
#define ZERODELAY_BIG_ENDIAN						(0)


#if _WIN32
#define ZERODELAY_LINE __LINE__
#define ZERODELAY_FUNCTION __FUNCTION__
#define ZERODELAY_FUNCTION_LINE __FUNCTION__, __LINE__
#else
#define ZERODELAY_LINE 0
#define ZERODELAY_FUNCTION "UNKNOWN"
#define ZERODELAY_FUNCTION_LINE "UKNOWN", 0
#endif


#ifdef _WIN32
	
	#include <ws2tcpip.h>
	#include <ws2ipdef.h>

	#pragma comment(lib, "Ws2_32.lib")
	#pragma comment(lib, "User32.lib")

	#if ZERODELAY_SDL // if windows and SDL 
		#include "SDL.h"
		#include "SDL_net.h"
		#pragma  comment(lib, "SDL2.lib")
		#pragma  comment(lib, "SDL2_net.lib")
	#endif
	
#else

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
		static void sleep( i32_t milliSeconds );

	private:
		static bool wasInitialized;
		static std::mutex mapMutex;
		static std::mutex logMutex;
		static std::map<std::string, void*> name2RpcFunction;
	};
}