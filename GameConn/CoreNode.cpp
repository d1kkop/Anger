#include "CoreNode.h"
#include "RecvNode.h"
#include "ConnectionNode.h"
#include "VariableGroupNode.h"


namespace Zerodelay
{
	CoreNode::CoreNode(class ZNode* zn, class RecvNode* rn, class ConnectionNode* cn, class VariableGroupNode* vgn):
		m_UserPtr(nullptr),
		m_IsSuperPeer(false),
		m_ZNode(zn),
		m_RecvNode(rn), 
		m_ConnectionNode(cn),
		m_VariableGroupNode(vgn)
	{
		m_CriticalErrors = 0;
		assert(m_ZNode && m_RecvNode && m_ConnectionNode && m_VariableGroupNode && "Not all Ptrs set");
		m_RecvNode->postInitialize(this);
		m_ConnectionNode->postInitialize(this);
		m_VariableGroupNode->postInitialize(this);
	}

	CoreNode::~CoreNode()
	{
		delete m_RecvNode;
		delete m_ConnectionNode;
		delete m_VariableGroupNode;
	}

	void CoreNode::setCriticalError(ECriticalError error, const char* fn)
	{
		m_CriticalErrors |= (u32_t)error;
		m_FunctionInError = fn;
	}

	const char* CoreNode::getCriticalErrorMsg() const
	{
		// If multiple are set, simply return the first bit that is set.
		ECriticalError err = (ECriticalError)(u32_t)m_CriticalErrors;
		switch (err)
		{
			case ECriticalError::SerializationError:
			return "Serialization Error";
			case ECriticalError::CannotFindExternalCFunction:
			return "Cannot find external C function";
			case ECriticalError::SocketIsNull:
			return "Socket is NULL";
		}
		return "";
	}

}