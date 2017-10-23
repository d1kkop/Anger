#pragma once

#include "Zerodelay.h"
#include <ctime>
#include <functional>


namespace Zerodelay
{
	class NetVariable
	{
	public:
		NetVariable(i32_t nBytes, void* data, void* prevData);
		~NetVariable();

		enum class EVarControl getVarControl() const;
		i32_t getGroupId() const;
		bool read( const i8_t*& buff, i32_t& buffLen);
		i8_t* data();
		const i8_t* data() const;
		i32_t length() const { return m_Length; }
		void unrefGroup() { m_Group = nullptr; }

		// Mark changed when variable changes data.
		void markChanged();
		void markUnchanged(); // mark unchanged when variable is written to network stream
		bool isInNetwork() const;
		bool isChanged() const { return m_Changed; }
		void sendNewest(ZNode* node, i32_t groupBit);
		void bindOnPostUpdateCallback( const std::function<void (const i8_t*, const i8_t*)>& callback );

	private:
		class VariableGroup* m_Group;
		i8_t* m_Data;
		i8_t* m_PrevData;
		i32_t m_Length;
		bool  m_Changed;
		std::function<void (const i8_t*, const i8_t*)> m_PostUpdateCallback;
	};
}