#include "Netvar.h"
#include "NetVariable.h"


namespace Zerodelay
{
	NetVar::NetVar(int nBytes):
		p(new NetVariable(nBytes))
	{
	}

	NetVar::~NetVar()
	{
		delete p;
	}


	EVarControl NetVar::getVarConrol() const
	{
		return p->getVarControl();
	}

	// -------- NetInt ----------------------------------------------------------------------------------------------

	NetByte::NetByte() :
		NetVar(1)
	{

	}

	NetInt16::NetInt16():
		NetVar(2)
	{

	}

}