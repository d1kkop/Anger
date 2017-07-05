#pragma once

#include "NetVar.h"
#include <vector>


namespace Zerodelay
{
	class NetVariable;

	class VariableGroup
	{
	public:
		VariableGroup(char channel, EPacketType type);
		virtual ~VariableGroup();

		bool read( const char* data, int buffLen, unsigned short groupBits );
		void addVariable( NetVariable* nv ) { m_Variables.emplace_back( nv ); }
		EVarControl getVarControl() const	{ return m_Control; }
		unsigned int getNetworkId() const   { return m_NetworkId; }

		// Only when networkId is assigned, the group can be submitted network wide
		void setNetworkId( unsigned int id );
		bool isNetworkIdValid() const			{ return m_NetworkId != 0; }

		void setControl( EVarControl control )	{ m_Control = control; }

		// Group is broken if one the variable's destructors is called. In that case, the group is no longer complete/valid.
		void markBroken()	  { m_Broken = true; /* lose all refs */ m_Variables.clear(); }
		bool isBroken() const { return m_Broken; }

		// If set dirty, it means that at least a single variable was changed in the group since the last time the group was submitted to the 
		// ConnectionNode using SendReliableNewest. It says nothing about whether the variable is succesfully synced to a remote machine. This is entirely
		// managed in the RUDPConnection.
		bool isDirty() const		{ return m_Dirty; }
		void setDirty(bool dirty)	{ m_Dirty = dirty; }

		bool isDestroySent() const	{ return m_DestroySent; }
		void markDestroySent()		{ m_DestroySent = true; }

		EPacketType getType() const	{ return m_Type; }

		void sendGroup(ZNode* node);

		void unrefGroup();

	private:
		char m_Channel;
		bool m_Broken;
		bool m_DestroySent;
	 	unsigned int m_NetworkId;
		EVarControl m_Control;
		EPacketType m_Type;
		bool m_Dirty;
		std::vector<NetVariable*> m_Variables;

	public:
		static VariableGroup* Last;
	};
}