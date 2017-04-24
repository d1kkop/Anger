

#pragma once


extern "C"
{
	using void* = angNode;
	using void* = angAddress;
	using void* = angConnection;

	angNode* angCreateNode();
	bool angConnect( angNode n, const char* endPoint, unsigned short port );
	bool angListen( angNode n, unsigned short port );
	
	void angBindOnConnectResult( angNode n, func );
	void angBindOnNewConnection( angNode n, func );
	void angBindOnDisconnect( angNode n, func );
	
	angCloseConnection( angNode n, angAddress a );
	angCloseConnection( angConnection c );
		
}
