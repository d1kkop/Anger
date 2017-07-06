#include "Netvar.h"
#include "NetVariable.h"


namespace Zerodelay
{
	NetVar::NetVar(i32_t nBytes):
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

	u32_t NetVar::getNetworkGroupId() const
	{
		return p->getGroupId();
	}

	void NetVar::markChanged()
	{
		p->markChanged();
	}

	void NetVar::bindOnPostUpdateCallback(const std::function<void(const i8_t*, const i8_t*)>& rawCallback)
	{
		p->bindOnPostUpdateCallback( rawCallback );
	}

	i8_t* NetVar::data()
	{
		return p->data();
	}

	const i8_t* NetVar::data() const
	{
		return p->data();
	}
}