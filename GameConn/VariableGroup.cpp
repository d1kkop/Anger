#pragma once

#include "VariableGroup.h"
#include "NetVariable.h"

#include <cassert>


namespace Zerodelay
{
	VariableGroup::VariableGroup():
		m_NumPreBytes(0),
		m_NetworkId(0),
		m_Broken(false),
		m_Control(EVarControl::Full)
	{
	}

	VariableGroup::~VariableGroup()
	{
	}

	bool VariableGroup::sync(bool isWriting, char* data, int buffLen, int& nOperations)
	{
		const int maxVars = 1024;
		nOperations = 0;

		// Compute num prebytes that indicate that can hold a bit
		// for every variable that is been written
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
		if ( m_NumPreBytes < buffLen )
		{
			Platform::log( "serialization error in: %s", __FUNCTION__ );
			return false;
		}
		
		int kByte = 0;
		int kBit  = 0;
		char* varBits = data;
		data += m_NumPreBytes;
		
		// In begin, always numPreBytes are nOperations, and buffLength is provided minus that
		nOperations = m_NumPreBytes;
		buffLen -= nOperations;

		// If is writing, skip the pre-bytes and first write all requested data
		if ( isWriting )
		{
			memset( varBits, 0, m_NumPreBytes );
			for (auto* v : m_Variables)
			{
				if ( v->wantsSync() )
				{
					if ( v->sync( true, data, buffLen, nOperations ) )
					{
						varBits[kByte] |= (1 << kBit);
					}
					else
					{
						// not enough buff length
						return false;
					}
				}
				incBitCounterAndWrap(kBit, kByte);
			}
		}
		else // Is reading..
		{
			for ( auto* v : m_Variables )
			{
				bool isWritten = (varBits[kBit] & (1 << kByte)) != 0;
				if ( isWritten )
				{
					if ( !v->sync( false, data, buffLen, nOperations ) )
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