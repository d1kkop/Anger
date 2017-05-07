#pragma once

#include "Netvar.h"

namespace Zerodelay
{
	class NetVariable
	{
	public:
		NetVariable(int nBytes);
		virtual ~NetVariable();

		EVarControl getVarControl() const;
		bool sync(bool writing, char*& buff, int buffLen);
		bool wantsSync() const { return true; }

	private:
		class VariableGroup* m_Group;
		char* m_Data;
		int m_Length;
	};
}