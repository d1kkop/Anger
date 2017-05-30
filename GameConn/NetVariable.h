#pragma once

#include "Zerodelay.h"
#include <ctime>
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
		bool read( const char*& buff, int& buffLen);
		char* data();
		const char* data() const;
		int length() const { return m_Length; }
		void unrefGroup() { m_Group = nullptr; }

		// If any of the variables in a group is changed, then the group becomes dirty and will write data
		void markChanged();
		void sendNewest(ZNode* node, int groupBit);
		void bindOnPostUpdateCallback( const std::function<void (const char*, const char*)>& callback );

	private:
		class VariableGroup* m_Group;
		char* m_Data;
		char* m_PrevData;
		int m_Length;
		std::function<void (const char*, const char*)> m_PostUpdateCallback;
	};
}