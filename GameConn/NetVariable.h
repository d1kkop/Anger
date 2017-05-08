#pragma once

#include "Zerodelay.h"


namespace Zerodelay
{
	class ZDLL_DECLSPEC NetVariable
	{
	public:
		NetVariable(int nBytes);
		virtual ~NetVariable();

		enum class EVarControl getVarControl() const;
		int getGroupId() const;
		bool sync(bool writing, char*& buff, int& buffLen, int& nOperations);
		bool wantsSync() const { return true; }
		char* data();
		const char* data() const;

	private:
		class VariableGroup* m_Group;
		char* m_Data;
		int m_Length;
	};
}