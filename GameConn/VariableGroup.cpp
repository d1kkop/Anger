#pragma once

#include "VariableGroup.h"
#include "NetVariable.h"
#include "Platform.h"
#include "RUDPLink.h"

#include <cassert>



namespace Zerodelay
{
	VariableGroup::VariableGroup(ZNode* znode):
		m_ZNode(znode),
		m_NetworkId(-1),
		m_Broken(false),
		m_DestroySent(false),
		m_Dirty(false),
		m_Control(EVarControl::Full),
		m_RemoteCreated(false)
	{
	}

	VariableGroup::~VariableGroup()
	{
		unrefVariables();
	//	Platform::log("VariableGroup %d is destructed", m_NetworkId);
	}

	bool VariableGroup::read(const i8_t*& data, i32_t& buffLen, u16_t groupBits )
	{
		assert ( (i32_t)m_Variables.size() <= RUDPLink::sm_MaxItemsPerGroup );
		if ( !((i32_t)m_Variables.size() <= RUDPLink::sm_MaxItemsPerGroup) )
		{
			Platform::log("ERROR: m_Variables.size() cannot exceed %d.", RUDPLink::sm_MaxItemsPerGroup-1);
			return false;
		}
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
		assert ( m_NetworkId == -1 && "network id already set" );
		if ( m_NetworkId != -1 )
			return;
		m_NetworkId = id;
	}

	void VariableGroup::setOwner(const ZEndpoint* ztp)
	{
		if ( ztp )
		{
			m_Owner = *ztp;
		}
	}

	const ZEndpoint* VariableGroup::getOwner() const
	{
		if (m_Owner.isValid()) return nullptr;
		return &m_Owner;
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

	void VariableGroup::unrefVariables()
	{
		for (auto* v : m_Variables)
		{
			if ( v )
			{
				v->unrefGroup(); // sets group ptr to null as group is about to be deleted
			}
		}
		m_Variables.clear();
		markBroken();
	}

	bool VariableGroup::isRemoteCreated() const
	{
		if ( !m_RemoteCreated && m_ZNode->isPacketDelivered( m_RemoteCreatedTicked ) )
		{
			m_RemoteCreated = true;
		}
		return m_RemoteCreated;
	}

	VariableGroup* VariableGroup::Last = nullptr;
}