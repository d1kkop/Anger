#pragma once

#include "Zerodelay.h"
#include "Util.h"

#include <atomic>


namespace Zerodelay
{
	enum class ECriticalError	// bitfield
	{
		None = 0,
		SerializationError = 1,
		CannotFindExternalCFunction = 2,
		SocketIsNull = 4,
		TooMuchDataToSend = 8
	};


	class CoreNode
	{
		typedef std::function<void (const struct EndPoint&, u8_t, const i8_t*, i32_t, u8_t)>	CustomDataCallback;


	public:
		CoreNode(class ZNode* zn, class RecvNode* rn, class ConnectionNode* cn, class VariableGroupNode* vgn);
		~CoreNode();
		void bindOnCustomData(const CustomDataCallback& cb)				{ Util::bindCallback(m_CustomDataCallbacks, cb); }

		// Packets whose handle location is not really clear such as RPC and User data.
		void recvRpcPacket( const i8_t* payload, i32_t len );
		void recvUserPacket( const struct Packet& pack, const struct EndPoint& etp );
		void processUnhandledPacket( struct Packet& pack, const struct EndPoint& etp );

		bool isSuperPeer() const { return m_IsSuperPeer; }
		bool isP2P() const { return m_IsP2P; }

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
		bool m_IsP2P;
		bool m_IsSuperPeer; // server or super peer in p2p
		std::string m_FunctionInError;
		std::atomic_uint32_t m_CriticalErrors;
		class ZNode* m_ZNode;
		class RecvNode* m_RecvNode;
		class ConnectionNode* m_ConnectionNode;
		class VariableGroupNode* m_VariableGroupNode;
		std::vector<CustomDataCallback>	m_CustomDataCallbacks;
	};
}