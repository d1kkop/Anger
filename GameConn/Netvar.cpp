#include "Netvar.h"
#include "NetVariable.h"


namespace Zerodelay
{
	NetVar::NetVar(i32_t nBytes, void* data, void* prevDat):
		p(new NetVariable(nBytes, data, prevDat))
	{
	}

	NetVar::~NetVar()
	{
		delete p;
	}

	const ZEndpoint* NetVar::getOwner() const
	{
		return p->getOwner();
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

	bool NetVar::isInNetwork() const
	{
		return p->isInNetwork();
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