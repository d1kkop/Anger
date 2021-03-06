#pragma once

#include "Platform.h"
#include <cassert>

#if ZERODELAY_SDL
	#include "SDL.h"
	#include "SDL_net.h"
#endif


namespace Zerodelay
{
	i32_t Platform::initialize()
	{
		if ( wasInitialized )
			return 0;

		srand ((u32_t)time(nullptr));

	#if ZERODELAY_SDL

		i32_t err =  SDL_Init(0);
		if (err != 0) 
		{ 
			Platform::log("SDL error %s.", SDL_GetError());
			return err; 
		}

		err = SDLNet_Init();
		if (err != 0) 
		{
			Platform::log("SDL net error %s.", SDL_GetError());
			return err;
		}

	#else

	#if ZERODELAY_WIN32SOCKET

			WORD wVersionRequested;
			WSADATA wsaData;
			i32_t err;

			/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
			wVersionRequested = MAKEWORD(2, 2);

			err = WSAStartup(wVersionRequested, &wsaData);
			if ( err != 0 )
				return err;

			/* Confirm that the WinSock DLL supports 2.2.*/
			/* Note that if the DLL supports versions greater    */
			/* than 2.2 in addition to 2.2, it will still return */
			/* 2.2 in wVersion since that is the version we      */
			/* requested.                                        */

			if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) 
			{
				/* Tell the user that we could not find a usable */
				/* WinSock DLL.                                  */
				WSACleanup();
				return -1;
			}
	#endif

	#endif

		wasInitialized = true;
		return 0;
	}

	void Platform::shutdown()
	{
		if ( !wasInitialized )
			return;

	#if ZERODELAY_SDL
		SDLNet_Quit();
		SDL_Quit();
	#else
		#if ZERODELAY_WIN32SOCKET
			WSACleanup();
		#endif
	#endif

		name2RpcFunction.clear();
		wasInitialized = false;
	}

	void* Platform::getPtrFromName(const i8_t* name)
	{
		std::lock_guard<std::mutex> lock(mapMutex);
		auto it = name2RpcFunction.find( name );
		if ( it == name2RpcFunction.end() )
		{
			// ptr to function
			void* pf = nullptr;

	#if ZERODELAY_SDL
			pf = SDL_LoadFunction(nullptr, name);
	#else
		#if ZERODELAY_INCWINDOWS
			HMODULE hModule = ::GetModuleHandle(NULL);
			pf = ::GetProcAddress( hModule, name );
		#endif
	#endif

			if ( pf )
			{
				name2RpcFunction.insert( std::make_pair( name, pf ) );
			}

			return pf;
		}
		return it->second;
	}

	void Platform::log(const i8_t* fmt, ...)
	{
		std::lock_guard<std::mutex> lock(logMutex);

		/* Declare a va_list type variable */
		i8_t buff[2048];
		va_list myargs;
		va_start(myargs, fmt);
#if ZERODELAY_SECURECRT
		vsprintf_s(buff, 2048, fmt, myargs);
		strcat_s(buff, 2048, "\n");
#else
		vsprintf(buff, fmt, myargs);
		strcat(buff, "\n");
#endif
		va_end(myargs);

		static bool isFirstOpen = true;
		if ( isFirstOpen )
			::remove( "ZerodelayLog.txt" );
		FILE* f;
	#if ZERODELAY_SECURECRT
		fopen_s( &f, "ZerodelayLog.txt", "a" );
	#else
		f = fopen("ZerodelayLog.txt", "a");
	#endif
		if ( f )
		{
			time_t rawtime;
			struct tm timeinfo;
			time (&rawtime);
			localtime_s(&timeinfo, &rawtime);
			i8_t asciitime[128];
			asctime_s(asciitime, 128, &timeinfo);
			i8_t* p = strstr(asciitime, "\n");
			if ( p )
				*p ='\0';
			if ( isFirstOpen )
			{
				isFirstOpen = false;
			#if ZERODELAY_SECURECRT
				fprintf_s( f, "--------- NEW SESSION ---------\n" );
				fprintf_s( f, "-------------------------------\n" );
			#else
				fprintf( f, "--------- NEW SESSION ---------\n" );
				fprintf( f, "-------------------------------\n" );
			#endif
			}
		#if ZERODELAY_SECURECRT
			fprintf_s( f, "%s\t\t%s", asciitime, buff );
		#else
			fprintf( f, "%s\t\t%s", asciitime, buff );
		#endif
			fflush(f);
			fclose(f);
		}
#if ZERODELAY_INCWINDOWS
		::OutputDebugString(buff);
#endif
	}

	bool Platform::memCpy(void* dst, i32_t dstSize, const void* src, i32_t srcSize)
	{
		if ( srcSize > dstSize )
		{
			assert(false);
			return false;
		}
		i32_t res;
	#ifdef ZERODELAY_SECURECRT
		res = memcpy_s( dst, dstSize, src, srcSize );
	#else
		res = memcpy( dst, src, srcSize );
	#endif
		assert(res == 0);
		return true;
	}

	bool Platform::formatPrint(i8_t* dst, i32_t dstSize, const i8_t* fmt, ...)
	{
		va_list myargs;
		va_start(myargs, fmt);
	#if ZERODELAY_SECURECRT
		vsprintf_s(dst, dstSize, fmt, myargs);
	#else
		vsprintf(dst, fmt, myargs);
	#endif
		va_end(myargs);
		return true;
	}

	void Platform::sleep(i32_t milliSeconds)
	{
		if (milliSeconds>0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(milliSeconds));
		}
	}

	bool Platform::wasInitialized = false;
	std::mutex Platform::mapMutex;
	std::mutex Platform::logMutex;
	std::map<std::string, void*> Platform::name2RpcFunction;
}