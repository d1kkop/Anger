#pragma once

#include "NetVariable.h"
#include "Netvar.h"
#include "Platform.h"
#include "VariableGroup.h"
#include <cassert>


namespace Zerodelay
{
	NetVariable::NetVariable(int nBytes):
		m_Group(VariableGroup::Last),
		m_Data(new char[nBytes]),
		m_PrevData(nullptr),
		m_Length(nBytes)
	{
		assert( m_Group != nullptr && "VariableGroup::Last" );
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
		delete [] m_PrevData;
		delete [] m_Data;
	}

	Zerodelay::EVarControl NetVariable::getVarControl() const
	{
		if ( !m_Group )
			return EVarControl::Full;
		return m_Group->getVarControl();
	}

	int NetVariable::getGroupId() const
	{
		if ( !m_Group )
			return 0;
		return m_Group->getNetworkId();
	}

	bool NetVariable::sync(bool writing, char*& buff, int& buffLen)
	{
		if ( buffLen < m_Length )
		{
			Platform::log( "serialize error in: %f", __FUNCTION__ );
			return false;
		}
		if ( writing )
		{
			if ( m_PreWriteCallback )
			{
				m_PreWriteCallback( m_Data );
			}
			memcpy( buff, m_Data, m_Length );
		}
		else
		{
			if ( m_PostUpdateCallback )
			{
				if ( !m_PrevData )
				{
					m_PrevData = new char[m_Length];
				}
				memcpy( m_PrevData, m_Data, m_Length );
			}
			memcpy( m_Data, buff, m_Length );
			if ( m_PostUpdateCallback && memcmp( m_Data, m_PrevData, m_Length ) != 0 )
			{
				m_PostUpdateCallback( m_PrevData, m_Data );
			}
		}
		buff += m_Length;
		buffLen -= m_Length;
		return true;
	}

	char* NetVariable::data()
	{
		return m_Data;
	}

	const char* NetVariable::data() const
	{
		return m_Data;
	}

	void NetVariable::bindOnPreWriteCallback(const std::function<void(const char*)>& callback)
	{
		m_PreWriteCallback = callback;
	}

	void NetVariable::bindOnPostUpdateCallback(const std::function<void(const char*, const char*)>& callback)
	{
		m_PostUpdateCallback = callback;
	}

}