#pragma once

#include "Zerodelay.h"

#include <atomic>


namespace Zerodelay
{
	enum class ECriticalError	// bitfield
	{
		None = 0,
		SerializationError = 1,
		CannotFindExternalCFunction = 2,
		SocketIsNull = 4
	};


	class CoreNode
	{
	public:
		CoreNode(class ZNode* zn, class RecvNode* rn, class ConnectionNode* cn, class VariableGroupNode* vgn);
		~CoreNode();

		bool isSuperPeer() const { return m_IsSuperPeer; }

		void setCriticalError(ECriticalError error, const char* fn);
		bool hasCriticalErrors() const { return m_CriticalErrors != 0; }
		const char* getCriticalErrorMsg() const;
		const char* getFunctionInError() const { return m_FunctionInError.c_str(); }

		void setUserDataPtr( void* ptr ) { m_UserPtr = ptr; }
		void* getUserDataPtr() const { return m_UserPtr; }

		class ZNode* zn() const { return m_ZNode; } // User
		class RecvNode* rn() const { return m_RecvNode; } // Internal (raw dispatch & send)
		class ConnectionNode* cn() const { return m_ConnectionNode; } // Controls state of connections
		class VariableGroupNode* vgn() const { return m_VariableGroupNode; } // Controls state of variable groups

	private:
		void* m_UserPtr;
		bool m_IsSuperPeer; // server or super peer in p2p
		std::string m_FunctionInError;
		std::atomic_uint32_t m_CriticalErrors;
		class ZNode* m_ZNode;
		class RecvNode* m_RecvNode;
		class ConnectionNode* m_ConnectionNode;
		class VariableGroupNode* m_VariableGroupNode;
	};
}