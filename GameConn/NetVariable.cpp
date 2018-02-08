#pragma once

#include "NetVariable.h"
#include "Netvar.h"
#include "Platform.h"
#include "VariableGroup.h"
#include "RUDPLink.h"

#include <cassert>


namespace Zerodelay
{
	NetVariable::NetVariable(i32_t nBytes, void* data, void* prevData):
		m_Group(VariableGroup::Last),
		m_Data((i8_t*)data),
		m_PrevData((i8_t*)prevData),
		m_Length(nBytes),
		m_Changed(false)
	{
		assert( m_Group != nullptr && "VariableGroup::Last" );
		assert( data && prevData );
		if ( m_Group == nullptr )
		{
			Platform::log("VariableGroup::Last not set before creating variable group in %s", __FUNCTION__ );
		}
		else
		{
			m_Group->addVariable( this );
		}
	}

	NetVariable::~NetVariable()
	{
		// Group becomes null if groups gest destroyed remotely.
		if ( m_Group )
		{
			m_Group->markBroken();
		}
	}

	const ZEndpoint* NetVariable::getOwner() const
	{
		if ( !m_Group ) return nullptr;
		return m_Group->getOwner();
	}

	Zerodelay::EVarControl NetVariable::getVarControl() const
	{
		// Group becomes null if groups gest destroyed remotely.
		if ( !m_Group )
			return EVarControl::Full;
		return m_Group->getVarControl();
	}

	u32_t NetVariable::getGroupId() const
	{
		// Group becomes null if groups gest destroyed remotely.
		if ( !m_Group )
			return -1;
		return m_Group->getNetworkId();
	}

	bool NetVariable::read(const i8_t*& buff, i32_t& buffLen)
	{
		if ( buffLen < m_Length )
		{
			Platform::log( "serialize error in: %f", __FUNCTION__ );
			return false;
		}
		
		if ( m_PostUpdateCallback )
		{
			assert( m_PrevData );
			Platform::memCpy( m_PrevData, m_Length, m_Data, m_Length );
		}
		Platform::memCpy( m_Data, m_Length, buff, m_Length );
		if ( m_PostUpdateCallback && memcmp( m_Data, m_PrevData, m_Length ) != 0 )
		{
			m_PostUpdateCallback( (i8_t*)m_PrevData, m_Data );
		}
		
		buff += m_Length;
		buffLen -= m_Length;
		return true;
	}

	i8_t* NetVariable::data()
	{
		return m_Data;
	}

	const i8_t* NetVariable::data() const
	{
		return m_Data;
	}

	void NetVariable::markChanged()
	{
		// Group becomes null if groups gest destroyed remotely.
		if ( m_Group )
		{
			m_Group->setDirty( true );
		}
		m_Changed = true;
	}

	void NetVariable::markUnchanged()
	{
		m_Changed = false;
	}

	bool NetVariable::isRemoteCreated() const
	{
		return m_Group && m_Group->isRemoteCreated();
	}

	void NetVariable::sendNewest(ZNode* node, i32_t groupBit)
	{
		assert( groupBit >= 0 && groupBit < RUDPLink::sm_MaxItemsPerGroup );
		if ( !(groupBit >= 0 && groupBit < RUDPLink::sm_MaxItemsPerGroup) )
		{
			Platform::log("ERROR: groupBit must be >= 0 && less than %d.", RUDPLink::sm_MaxItemsPerGroup);
		}
		node->sendReliableNewest( (u8_t)EDataPacketType::VariableGroupUpdate, getGroupId(), groupBit, m_Data, m_Length, nullptr, false );
	}

	void NetVariable::bindOnPostUpdateCallback(const std::function<void(const i8_t*, const i8_t*)>& callback)
	{
		m_PostUpdateCallback = callback;
	}

}