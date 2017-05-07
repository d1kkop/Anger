#pragma once

#include "RUDPConnection.h"

#include <mutex>


namespace Zerodelay
{
	enum class EVarControl
	{
		Full,
		Semi,
		Remote
	};

	/*	Currently, all variables are interpreted as POD (plain old data), just an array of bytes.
		Therefore any type/structure can be used as a NetVar but no network to host byte ordering is done.
		This means that BigEndian to LittleEndian traffic will fail if not accounted for at the application level. */
	class ZDLL_DECLSPEC NetVar
	{
	public:
		NetVar(int nBytes);
		virtual ~NetVar();


		/*	Specifies how this variable is controlled.
			[Full]		This means that the variable is controlled locally, therefore any writes to this variable from this machine
						will be synchronized to others. The variable will never be overwritten from the network. 
			[Semi]		This means that the variable is controlled locally, that is, it can be written to. But, it functions as a prediction,
						the variable may be overwritten later by a remote connection if the remote (say: 'server') did not accept the change
						or wants the value to be different.
			[Remote]	The variable is completely owned remotely, that is, if you write to the variable, it will automatically be 
						overwritten by updates from the network. Furthermore, local changes will not be submitted to the network. */
		EVarControl getVarConrol() const;


	private:
		class NetVariable* p;
	};


	class ZDLL_DECLSPEC NetByte : public NetVar
	{
	public:
		NetByte();
		virtual ~NetByte() { }
	};

	class ZDLL_DECLSPEC NetInt16: public NetVar
	{
	public:
		NetInt16();
		virtual ~NetInt16() { }
	};


	//// 16 bit signed integer type.
	//class NetInt16: public INetVar
	//{
	//	// 16 bit in length
	//	virtual void sync(char* buffer, int buffSize) override;
	//};


	//class NetInt32: public INetVar
	//{
	//	// 32 bit in length
	//	virtual void sync(char* buffer, int buffSize) override;
	//};


}