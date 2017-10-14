#pragma once

#include "VariableGroup.h"
#include "NetVariable.h"
#include "Platform.h"

#include <cassert>


namespace Zerodelay
{
	VariableGroup::VariableGroup():
		m_NetworkId(0),
		m_Broken(false),
		m_DestroySent(false),
		m_Dirty(false),
		m_Control(EVarControl::Full)
	{
	}

	VariableGroup::~VariableGroup()
	{
	}

	bool VariableGroup::read(const i8_t*& data, i32_t& buffLen, u16_t groupBits )
	{
		assert ( (i32_t)m_Variables.size() <= 16 );
		for (i32_t i = 0; i < (i32_t)m_Variables.size() ; i++)
		{
			auto* v = m_Variables[i];
			bool isWritten = (groupBits & (1 << i)) != 0;
			if ( isWritten && !v->read( data, buffLen ) )
			{
				Platform::log("WARNING: serialization error in variable group detected, synchronization may end up different than expected!");
				return false;
			}
		}
		return true;
	}

	void VariableGroup::setNetworkId(u32_t id)
	{
		assert ( m_NetworkId == 0 && "network id already set" );
		if ( m_NetworkId != 0 )
			return;
		m_NetworkId = id;
	}

	void VariableGroup::sendGroup(ZNode* node)
	{
		for (i32_t i = 0; i < (i32_t)m_Variables.size(); i++)
		{
			auto* v = m_Variables[i];
			if ( v->isChanged() )
			{
				v->sendNewest( node, i );
			}
			v->markUnchanged();
		}
		setDirty(false);
	}

	void VariableGroup::unrefGroup()
	{
		for (auto* v : m_Variables)
		{
			if ( v )
			{
				v->unrefGroup(); // sets group ptr to null as group is about to be deleted
			}
		}
		markBroken();
	}

	VariableGroup* VariableGroup::Last = nullptr;
}