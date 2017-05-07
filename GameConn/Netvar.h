#pragma once

#include "RUDPConnection.h"
#include "NetVariable.h"

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


		/*	The NetVar is part of a group that has an id. This id is network wide unique.
			Use this networkGroupId to target specific group's remotely. */
		unsigned int getNetworkGroupId() const;


	protected:
		char* data();
		const char* data() const;
		class NetVariable* p;
	};

	
	/*	The GenericNetVar is nothing more than a POD (plain old data) stucture that can hold
		native types and structures.
		It performs no Endiannes converions. So if this is not handled at the application level, 
		transport between different Endiannes machines will fail. 
		
		Examples:
			Note that the cast operator returns a reference to the data.
			
			--- Ex 1 ----------------------------------------------------------

			GenericNetVar<int> nInt;
			(int)nInt  = 0;
			(int)nInt += 22;
			int myLocalInt = nInt; // -> myLocalInt is 22

			----------------------------------------------------------

			GenericNetVar<double> nDouble;
			(double)nDouble = 0.0;
			(double)nDouble = myMathFunction(...);
			double myLocalDouble = nDouble; // -> has local copy of myMathFunction(..)		
			
			--- Ex 2 ----------------------------------------------------------
			
			struct MyLobby
			{
				char playerNames[10][64];
			};

			GenericNetVar<MyLobby> nLobby;
			(MyLobby)nLobby.playerNames[2] = "my nickname";			*/
	template <typename T>
	class GenericNetVar : public NetVar
	{
	public:
		GenericNetVar(): NetVar( sizeof(T) ) { }

		operator T& () { return *(T*)p->data(); }
		operator const T&() const { return *(const T*)p->data(); }
	};

}