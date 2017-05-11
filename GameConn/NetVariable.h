#pragma once

#include "Zerodelay.h"
#include <functional>


namespace Zerodelay
{
	class NetVariable
	{
	public:
		NetVariable(int nBytes);
		virtual ~NetVariable();

		enum class EVarControl getVarControl() const;
		int getGroupId() const;
		bool sync(bool writing, char*& buff, int& buffLen);
		bool wantsSync() const { return true; }
		char* data();
		const char* data() const;
		void bindOnUpdateCallback( std::function<void (const char*, const char*)> callback );

	private:
		class VariableGroup* m_Group;
		char* m_Data;
		int m_Length;
		std::function<void (const char*, const char*)> m_Callback;
	};
}