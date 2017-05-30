#pragma once

#include "VariableGroup.h"
#include "NetVariable.h"
#include "Platform.h"

#include <cassert>


namespace Zerodelay
{
	VariableGroup::VariableGroup(char channel, EPacketType type):
		m_Channel(channel),
		m_NetworkId(0),
		m_Broken(false),
		m_DestroySent(false),
		m_Control(EVarControl::Full),
		m_Type(type)
	{
	}

	VariableGroup::~VariableGroup()
	{
	}

	bool VariableGroup::read(const char* data, int buffLen, unsigned short groupBits )
	{
		assert ( (int)m_Variables.size() <= 16 );
		for (int i = 0; i < (int)m_Variables.size() ; i++)
		{
			auto* v = m_Variables[i];
			bool isWritten = (groupBits & (1 << i)) != 0;
			if ( isWritten && !v->read( data, buffLen ) )
			{
				// TODO emit error, because data cannot be read, yet sender may not resend data anymore (so not reliable!)
				return false;
			}
		}
		return true;
	}

	void VariableGroup::setNetworkId(unsigned int id)
	{
		assert ( m_NetworkId == 0 && "network id already set" );
		if ( m_NetworkId != 0 )
			return;
		m_NetworkId = id;
	}

	void VariableGroup::sendGroup(ZNode* node)
	{
		for (int i = 0; i < (int)m_Variables.size(); i++)
		{
			auto* v = m_Variables[i];
			v->sendNewest( node, i );
		}
		setDirty(false);
	}

	void VariableGroup::unrefGroup()
	{
		for (auto* v : m_Variables)
		{
			if ( v )
			{
				v->unrefGroup();
			}
		}
	}

	VariableGroup* VariableGroup::Last = nullptr;
}