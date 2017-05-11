#pragma once

#include "VariableGroup.h"
#include "NetVariable.h"
#include "Platform.h"

#include <cassert>


namespace Zerodelay
{
	VariableGroup::VariableGroup(char channel):
		m_Channel(channel),
		m_NumPreBytes(0),
		m_NetworkId(0),
		m_Broken(false),
		m_Control(EVarControl::Full)
	{
	}

	VariableGroup::~VariableGroup()
	{
	}

	bool VariableGroup::sync(bool isWriting, char* data, int buffLen)
	{
		const int maxVars = 1024;

		// Compute num prebytes in which for each variable that is written a abit is 
		// set whether it was transmitted or not.
		if ( m_NumPreBytes == 0)
		{
			m_NumPreBytes = (int(m_Variables.size()) + 7) / 8;
			if ( m_NumPreBytes > maxVars-1 )
			{
				Platform::log( "num max vars reached (%d) in %s", (maxVars-1), __FUNCTION__ );
				m_NumPreBytes = 0; // set to 0, so next time will hit the error again
				return false;
			}
		}

		// +4 for networkId
		buffLen -= (m_NumPreBytes+4);
		if ( buffLen < 0 )
		{
			Platform::log( "serialization error in: %s", __FUNCTION__ );
			return false;
		}
		
		int kByte = 0;
		int kBit  = 0;			// first 4 bytes are networkId
		char* varBits = data+4; // preBytes (indicate if variable is written or not)		

		// If is writing, skip the pre-bytes and first write all requested data
		if ( isWriting )
		{
			*(unsigned int*)data = m_NetworkId;
			memset( varBits, 0, m_NumPreBytes );
			for (auto* v : m_Variables)
			{
				if ( v->wantsSync() )
				{
					varBits[kByte] |= (1 << kBit);
					if ( !v->sync( true, data, buffLen ) )
					{
						// serialization error
						return false;
					}
				}
				incBitCounterAndWrap(kBit, kByte);
			}
		}
		else // Is reading..
		{
			// networkId already read.., skip it
			buffLen -= 4;
			for ( auto* v : m_Variables )
			{
				bool isWritten = (varBits[kBit] & (1 << kByte)) != 0;
				if ( isWritten )
				{
					if ( !v->sync( false, data, buffLen ) )
					{
						// not enough buff length
						return false;
					}
				}
				incBitCounterAndWrap( kBit, kByte );
			}
		}

		return true;
	}

	void VariableGroup::incBitCounterAndWrap(int &kBit, int &kByte)
	{
		if (++kBit == 8)
		{
			kBit = 0;
			kByte++;
		}
	}

	void VariableGroup::setNetworkId( unsigned int id )
	{
		assert ( m_NetworkId == 0 && "network id already set" );
		if ( m_NetworkId != 0 )
			return;
		m_NetworkId = id;
	}

	VariableGroup* VariableGroup::Last = nullptr;
}