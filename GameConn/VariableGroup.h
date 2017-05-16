#pragma once

#include "NetVar.h"
#include <vector>

namespace Zerodelay
{
	class NetVariable;

	class VariableGroup
	{
	public:
		VariableGroup(char channel);
		virtual ~VariableGroup();

		bool sync( bool isWriting, char* data, int& buffLen );
		void addVariable( NetVariable* nv ) { m_Variables.emplace_back( nv ); }
		EVarControl getVarControl() const	{ return m_Control; }
		unsigned int getNetworkId() const   { return m_NetworkId; }

		// Only when networkId is assigned, the group can be submitted network wide
		bool isNetworkIdValid() const		{ return m_NetworkId != 0; }
		void setNetworkId( unsigned int id );

		// Group is broken if one the variable's destructors is called. In that case, the group is no longer complete/valid.
		void markBroken()	  { m_Broken = true; }
		bool isBroken() const { return m_Broken; }

		bool isDirty() const		{ return m_Dirty; }
		void setDirty(bool dirty)	{ m_Dirty = dirty; }

		bool isDestroySent() const	{ return m_DestroySent; }
		void markDestroySent()		{ m_DestroySent = true; }

		char getChannel() const		{ return m_Channel; }
		bool isRemote() const		{ return m_Channel<0; }

	private:
		void incBitCounterAndWrap(int &kBit, int &kByte);

		char m_Channel;
		int  m_NumPreBytes;
		bool m_Broken;
		bool m_DestroySent;
	 	unsigned int m_NetworkId;
		EVarControl m_Control;
		bool m_Dirty;
		std::vector<NetVariable*> m_Variables;

	public:
		static VariableGroup* Last;
	};
}