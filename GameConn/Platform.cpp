#pragma once

#include "Platform.h"


namespace Zerodelay
{
	i32_t Platform::initialize()
	{
		if ( wasInitialized )
			return 0;

#if _WIN32

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

		wasInitialized = true;
		return 0;
	}

	void Platform::shutdown()
	{
		if ( !wasInitialized )
			return;

#if _WIN32
		WSACleanup();
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
#if _WIN32
			HMODULE hModule = ::GetModuleHandle(NULL);
			auto* pf = ::GetProcAddress( hModule, name );
			if ( pf )
			{
				name2RpcFunction.insert( std::make_pair( name, pf ) );
				return pf;
			}
#endif
			return nullptr;
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
#if _WIN32
		vsprintf_s(buff, 2048, fmt, myargs);
		strcat_s(buff, 2048, "\n");
#else
		vsprintf(buff, fmt, myargs);
		strcat(buff, "\n");
#endif
		va_end(myargs);

#if _WIN32
		static bool isFirstOpen = true;
		if ( isFirstOpen )
			::remove( "ZerodelayLog.txt" );
		FILE* f;
		fopen_s( &f, "ZerodelayLog.txt", "a" );
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
				fprintf_s( f, "--------- NEW SESSION ---------\n" );
				fprintf_s( f, "-------------------------------\n" );
			}
			fprintf_s( f, "%s\t\t%s", asciitime, buff );
			fflush(f);
			fclose(f);
		}
		::OutputDebugString(buff);
#endif
	}

	bool Platform::memCpy(void* dst, i32_t dstSize, const void* src, i32_t srcSize)
	{
		if ( srcSize > dstSize )
			return false;
	#ifdef _WIN32
		memcpy_s( dst, dstSize, src, srcSize );
	#else
		memcpy( dst, src, srcSize );
	#endif
		return true;
	}

	bool Platform::formatPrint(i8_t* dst, i32_t dstSize, const i8_t* fmt, ...)
	{
		va_list myargs;
		va_start(myargs, fmt);
	#if _WIN32
		vsprintf_s(dst, dstSize, fmt, myargs);
	#else
		vsprintf(dst, fmt, myargs);
	#endif
		va_end(myargs);
		return true;
	}

	bool Platform::wasInitialized = false;
	std::mutex Platform::mapMutex;
	std::mutex Platform::logMutex;
	std::map<std::string, void*> Platform::name2RpcFunction;
}