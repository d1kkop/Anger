#pragma once

#include "Platform.h"


namespace Motor
{
	namespace Anger
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
		}

	}
}