#pragma once

#include "NetVar.h"
#include <vector>


namespace Zerodelay
{
	class NetVariable;

	class VariableGroup
	{
	public:
		VariableGroup(ZNode* znode);
		~VariableGroup();

		bool read( const i8_t*& data, i32_t& buffLen, u16_t groupBits );
		void addVariable( NetVariable* nv ) { m_Variables.emplace_back( nv ); }

		void setControl( EVarControl control )	{ m_Control = control; }
		EVarControl getVarControl() const	{ return m_Control; }

		void  setNetworkId( u32_t id );
		u32_t getNetworkId() const { return m_NetworkId; }
		bool  isNetworkIdValid() const { return m_NetworkId != -1; }

		void setOwner( const ZEndpoint* etp );
		const ZEndpoint* getOwner() const;
		
		// Group is broken if one the variable's destructors is called. In that case, the group is no longer complete/valid.
		void markBroken()	  { m_Broken = true; }
		bool isBroken() const { return m_Broken; }

		// If set dirty, it means that at least a single variable was changed in the group since the last time the group was submitted to the 
		// ConnectionNode using SendReliableNewest. It says nothing about whether the variable is succesfully synced to a remote machine. This is entirely
		// managed in the RUDPConnection.
		bool isDirty() const		{ return m_Dirty; }
		void setDirty(bool dirty)	{ m_Dirty = dirty; }

		bool isDestroySent() const	{ return m_DestroySent; }
		void markDestroySent()		{ m_DestroySent = true; }

		void sendGroup(ZNode* node);
		void unrefVariables(); // decouples variables from as group is about to be deleted and marks it broken

		bool isRemoteCreated() const;

	private:
		ZNode* m_ZNode;
		bool m_Broken;
		bool m_DestroySent;
		bool m_Dirty;
	 	u32_t m_NetworkId;
		ZEndpoint m_Owner;
		EVarControl m_Control;
		std::vector<NetVariable*> m_Variables;
		ZAckTicket m_RemoteCreatedTicked;
		mutable bool m_RemoteCreated;

	public:
		static VariableGroup* Last;

		friend class VariableGroupNode;
	};
}