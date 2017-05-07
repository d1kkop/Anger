#pragma once

#include "NetVar.h"
#include <vector>

namespace Zerodelay
{
	class NetVariable;


	class VariableGroup
	{
	public:
		VariableGroup(unsigned int localId);
		virtual ~VariableGroup();

		bool sync( bool isWriting, char* data, int buffLen );

		void addVariable( NetVariable* nv ) { m_Variables.emplace_back( nv ); }
		EVarControl getVarControl() const	{ return m_Control; }

	private:
		void incBitCounterAndWrap(int &kBit, int &kByte);

		int m_NumPreBytes;
		unsigned int m_LocalId;
		unsigned int m_NetworkId;
		EVarControl m_Control;
		std::vector<NetVariable*> m_Variables;

	public:
		static VariableGroup* Last;
	};
}