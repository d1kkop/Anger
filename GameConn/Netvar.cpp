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

	unsigned int NetVar::getNetworkGroupId() const
	{
		return p->getGroupId();
	}

	void NetVar::bindOnPreWriteCallback(std::function<void(const char*)> rawCallback)
	{
		p->bindOnPreWriteCallback( rawCallback );
	}

	void NetVar::bindOnPostUpdateCallback(std::function<void(const char*, const char*)> rawCallback)
	{
		p->bindOnPostUpdateCallback( rawCallback );
	}

	char* NetVar::data()
	{
		return p->data();
	}

	const char* NetVar::data() const
	{
		return p->data();
	}
}