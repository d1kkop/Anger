#pragma once

#include "NetVariable.h"
#include "Netvar.h"
#include "Platform.h"
#include "VariableGroup.h"
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
		if ( m_Group )
		{
			m_Group->markBroken();
		}
		//delete [] m_PrevData;
		//delete [] m_Data;
	}

	Zerodelay::EVarControl NetVariable::getVarControl() const
	{
		if ( !m_Group )
			return EVarControl::Full;
		return m_Group->getVarControl();
	}

	i32_t NetVariable::getGroupId() const
	{
		if ( !m_Group )
			return 0;
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
			//if ( !m_PrevData )
			//{
			//	m_PrevData = new i8_t[m_Length];
			//}
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

	void NetVariable::sendNewest(ZNode* node, i32_t groupBit)
	{
		assert( groupBit >= 0 && groupBit < 16 );
		node->sendReliableNewest( (u8_t)EDataPacketType::VariableGroupUpdate, getGroupId(), groupBit, m_Data, m_Length, nullptr, false );
	}

	void NetVariable::bindOnPostUpdateCallback(const std::function<void(const i8_t*, const i8_t*)>& callback)
	{
		m_PostUpdateCallback = callback;
	}

}