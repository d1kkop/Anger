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
		CoreNode(ERoutingMethod routingMethod, class ZNode* zn, class RecvNode* rn, class ConnectionNode* cn, class VariableGroupNode* vgn);
		~CoreNode();

		ERoutingMethod getRoutingMethod() const { return m_RoutingMethod; }

		void setCriticalError(ECriticalError error, const char* fn);
		bool hasCriticalErrors() const { return m_CriticalErrors != 0; }
		const char* getCriticalErrorMsg() const;
		const char* getFunctionInError() const { return m_FunctionInError.c_str(); }

		void setUserDataPtr( void* ptr ) { m_UserPtr = ptr; }
		void* getUserDataPtr() const { return m_UserPtr; }

		class ZNode* zn() const { return m_ZNode; }
		class RecvNode* rn() const { return m_RecvNode; }
		class ConnectionNode* cn() const { return m_ConnectionNode; }
		class VariableGroupNode* vgn() const { return m_VariableGroupNode; }

	private:
		void* m_UserPtr;
		std::string m_FunctionInError;
		std::atomic_uint32_t m_CriticalErrors;
		ERoutingMethod m_RoutingMethod;
		class ZNode* m_ZNode;
		class RecvNode* m_RecvNode;
		class ConnectionNode* m_ConnectionNode;
		class VariableGroupNode* m_VariableGroupNode;
	};
}