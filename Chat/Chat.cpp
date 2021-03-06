// Chat.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "Chat.h"
#include <vector>
#include <string>
#include <thread>
#include <cstdlib>

#include "RpcMacros.h"
#include "Netvar.h"
#include "Zerodelay.h"


#define MAX_LOADSTRING 100

using namespace Zerodelay;

void				RefreshTextField();

ZNode* g_Node;
std::vector<std::string> g_lines;
int g_NumClients =0;
const int g_MaxNames = 10;
struct ChatLobby
{
	char name[g_MaxNames][64];
	ChatLobby() 
	{
		memset(this, 0, sizeof(*this));
	}
};



int g_MsgId = 100;
int g_LobbyId  = 101;
int g_MaxLines = 20;
bool g_Done = false;
bool g_AutoChat = false;

ChatLobby g_lobby;

class ConnectionListener : public IConnectionListener
{
	/*	This event occurs when as a result of calling: 'connect', or 'connectToServer'. */
	void onConnectResult( const struct ZEndpoint& etp, EConnectResult res ) override 
	{
		switch ( res )
		{
		case EConnectResult::Succes:
			g_lines.emplace_back( "connection: " + etp.toIpAndPort() + " connected succesful" );
			break;
		case EConnectResult::Timedout:
			g_lines.emplace_back( "connection: " + etp.toIpAndPort() + " connecting timed out" );
			break;
		case EConnectResult::InvalidPassword:
			g_lines.emplace_back( "connection: " + etp.toIpAndPort() + " connecting invalid pw" );
			break;
		case EConnectResult::MaxConnectionsReached:
			g_lines.emplace_back( "connection: " + etp.toIpAndPort() + " connecting max conns reached" );
			break;
		}
		RefreshTextField();
	}
	void onNewConnection( bool directLink, const struct ZEndpoint& etp,  const std::map<std::string, std::string>& metaData ) override 
	{
		if ( g_Node->isAuthorative() )
		{
			g_NumClients++;
			for (int i = 0; i < g_MaxNames; i++)
			{
				if ( g_lobby.name[i][0] == '\0' )
				{
					strcpy_s( g_lobby.name[i], 64, etp.toIpAndPort().c_str());
					break;
				}
			}
		}
		g_lines.emplace_back(" new connection: " + etp.toIpAndPort() );
		RefreshTextField();
	}
	void onDisconnect( bool directLink, const struct ZEndpoint& etp, EDisconnectReason reason ) override 
	{
		if ( reason == EDisconnectReason::Lost )
		{
			g_lines.emplace_back( "connection " + etp.toIpAndPort() + " lost connection" );
		}
		else
		{
			g_lines.emplace_back( "connection " + etp.toIpAndPort() + " closed connection" );
		}
		if ( g_Node->isAuthorative() )
		{
			g_NumClients--;
			for (int i = 0; i < g_MaxNames; i++)
			{
				if ( strcmp( g_lobby.name[i], etp.toIpAndPort().c_str() ) == 0)
				{
					g_lobby.name[i][0] = '\0';
					break;
				}
			}
		}
		RefreshTextField();
	}
};

ConnectionListener* g_ConListener;


void SendFinal( const char* txt, int len );


HWND TextField;
HWND SendButton;
HWND TextBox;
HWND Lobby;

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
void				RefreshLobby();
void				RefreshTextField();
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void				InitNetwork(bool isServ);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);


    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CHAT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

	char buff[2048];
	wcstombs_s( nullptr, buff,  lpCmdLine, 2048 );
	bool isServ = strstr( buff, "-serv") != nullptr;
	g_AutoChat  = strstr( buff, "-auto") != nullptr;
	InitNetwork( isServ );

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CHAT));
    MSG msg;
	memset(&msg, 0, sizeof(msg));
    // Main message loop:
	while (!g_Done)
	{
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	    {
		    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		g_Node->update();
		std::this_thread::sleep_for( std::chrono::milliseconds(100) );

		if ( isServ && g_NumClients > 0 )
		{
			g_Node->sendReliableOrdered( g_LobbyId, (const char*)&g_lobby, sizeof(g_lobby) );
		}

		if ( g_AutoChat )
		{
			int l = rand() % 10 + 10;
			char buff[1024];
			for (int i = 0; i < l ; i++)
			{
				buff[i] = rand() % 255;
			}
			buff[l] = '\0';
			SendFinal( buff, l );
		}

		RefreshLobby();
    }

	g_Node->disconnect();
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	delete g_Node;
    return (int) msg.wParam;
}


void InitNetwork(bool isServ)
{
	g_ConListener = new ConnectionListener;
	g_Node = new ZNode( 300, -1 );
	g_Node->addConnectionListener(g_ConListener);

	g_Node->bindOnCustomData( [] (auto etp, auto id, auto data, auto len, auto channel) 
	{
		// textfield update
		if ( id == g_MsgId )
		{
			char str [2048];
			if ( len > 2047 )
				len = 2047;
			memcpy( str, data, len );
			str[len] = '\0';
			g_lines.emplace_back( str );
			RefreshTextField();
		}
		// lobby update
		if ( id == g_LobbyId )
		{
			if ( len == sizeof(ChatLobby) )
			{
				memcpy( &g_lobby, data, len );
			}
			RefreshLobby();
		}
	});

	if ( isServ )
	{
		strcpy_s( g_lobby.name[0], 64, "server" );
		g_Node->listen(27000, "auto", 9);
		g_lines.emplace_back( "started as server..." );
	}
	else
	{
		g_Node->connect( "localhost", 27000, "auto" );
		g_lines.emplace_back( "connecting..." );
	}
	RefreshTextField();
}


void RefreshLobby()
{
	std::string total;
	for (int i = 0; i < g_MaxNames ; ++i)
	{
		if ( g_lobby.name[i][0] != '\0' )
		{
			total += g_lobby.name[i] + std::string("\r\n");
		}
	}
	SetWindowText( Lobby, total.c_str() );
}

void RefreshTextField()
{
	while ( g_lines.size() > g_MaxLines )
	{
		g_lines.erase(g_lines.begin());
	}
	// total text
	std::string total;
	for (int i = (int)g_lines.size() - 1; i >= 0 ; i--)
	{
		auto& s = g_lines[i];
		total += s + std::string("\r\n");
	}
	SetWindowText( TextField, total.c_str() );
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CHAT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CHAT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 860, 320, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

void SendFinal( const char* text, int len )
{
	g_Node->sendReliableOrdered(100, text, len + 1);
	g_lines.emplace_back(text);
	RefreshTextField();
}

void Send()
{
	char text[2048];
	GetWindowText(TextBox, text, 2047);
	SetWindowText(TextBox, "");
	int kLen = (int)strlen(text);
	if ( kLen <= 0 )
		return;
	SendFinal( text, kLen );
}



//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_CREATE:
	{
		TextBox = CreateWindow(TEXT("EDIT"),
							   TEXT(""),
							   WS_BORDER | WS_CHILD | WS_VISIBLE | ES_MULTILINE,
							   10, 220, 590, 20,
							   hWnd, (HMENU) 1, NULL, NULL);
		TextField = CreateWindow(TEXT("EDIT"),
								 TEXT(""),
								 WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_READONLY | ES_MULTILINE | ES_NUMBER,
								 10, 10, 590, 200,
								 hWnd, (HMENU) 3, NULL, NULL);
		SendButton = CreateWindow(TEXT("BUTTON"),
								  TEXT("Send"),
								  WS_VISIBLE | WS_CHILD | WS_BORDER,
								  610, 220, 200, 20,
								  hWnd, (HMENU) 2, NULL, NULL);
		Lobby = CreateWindow(TEXT("EDIT"),
							   TEXT(""),
							   WS_BORDER | WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | WS_VSCROLL,
							   610, 10, 200, 200,
							   hWnd, (HMENU) 4, NULL, NULL);
	}
	break;

	case WM_KEYDOWN:
		if ( LOWORD(wParam) == VK_RETURN )
		{
			Send();
		}
		break;

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
			int hid = HIWORD(wParam);

			if ( wmId == 2 || hid == 1281 )
				Send();

            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
		g_Done = true;
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
