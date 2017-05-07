#pragma once

#include "NetVar.h"
#include <vector>

namespace Zerodelay
{
	class NetVariable;

	class VariableGroup
	{
	public:
		VariableGroup();
		virtual ~VariableGroup();

		bool sync( bool isWriting, char* data, int buffLen );
		void addVariable( NetVariable* nv ) { m_Variables.emplace_back( nv ); }
		EVarControl getVarControl() const	{ return m_Control; }
		unsigned int getNetworkId() const   { return m_NetworkId; }
		bool isNetworkIdValid() const		{ return m_NetworkId != 0; }
		void setNetworkId( unsigned int id );

	private:
		void incBitCounterAndWrap(int &kBit, int &kByte);

		int m_NumPreBytes;
		unsigned int m_NetworkId;
		EVarControl m_Control;
		std::vector<NetVariable*> m_Variables;

	public:
		static VariableGroup* Last;
	};
}