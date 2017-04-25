#pragma once

#include "Platform.h"


namespace Zeroone
{
	int Platform::initialize()
	{
#if _WIN32

		WORD wVersionRequested;
		WSADATA wsaData;
		int err;

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


		return 0;
	}

	void Platform::shutdown()
	{
#if _WIN32
		WSACleanup();
#endif
		name2RpcFunction.clear();
	}

	void* Platform::getPtrFromName(const char* name)
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

	void Platform::log(const char* fmt, ...)
	{
		std::lock_guard<std::mutex> lock(logMutex);

		/* Declare a va_list type variable */
		char buff[2048];
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
		FILE* f;
		fopen_s( &f, "AngerLog.txt", "a+" );
		if ( f )
		{
			fprintf_s( f, "%s", buff );
			fflush(f);
			fclose(f);
		}
		::OutputDebugString(buff);
#endif
	}

	std::mutex Platform::mapMutex;
	std::mutex Platform::logMutex;
	std::map<std::string, void*> Platform::name2RpcFunction;
}