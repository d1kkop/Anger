#pragma once

#include "Zerodelay.h"
#include "Util.h"

#include <atomic>


#define CHECK_SERIALIZE( expr ) if ( !(expr) ) { m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE); return false; }


namespace Zerodelay
{
	// Not be confused with EDataPacketType
	enum class EHeaderPacketType : u8_t
	{
		Reliable_Ordered,
		Unreliable_Sequenced,
		Reliable_Newest,
		Ack,
		Ack_Reliable_Newest
	};


	class CoreNode
	{
		using CustomDataCallback = std::function<void (const struct ZEndpoint&, u8_t, const i8_t*, i32_t, u8_t)>;


	public:
		CoreNode(class ZNode* zn, class RecvNode* rn, class ConnectionNode* cn, class VariableGroupNode* vgn, class MasterServer* ms);
		~CoreNode();
		void reset();

		void bindOnCustomData(const CustomDataCallback& cb)				{ Util::bindCallback(m_CustomDataCallbacks, cb); }

		// Packets whose handle location is not really clear such as RPC and User data.
		void recvRpcPacket( const i8_t* payload, i32_t len, const struct EndPoint& etp );
		void recvUserPacket( const struct Packet& pack, const struct EndPoint& etp );
		void processUnhandledPacket( struct Packet& pack, const struct EndPoint& etp );

		void setIsListening(bool isListening) { m_IsListening = isListening; }
		bool isListening() const { return m_IsListening; } // client-server has a server, in p2p everyone is also a listener

		void setIsP2P(bool isP2p) { m_IsP2P = isP2p; }
		bool isP2P() const { return m_IsP2P; }

		void setIsSuperPeer(bool superPeer) { m_IsSuperPeer = superPeer; }
		bool isSuperPeer() const { return m_IsSuperPeer; } // server or authorative peer in p2p

		void setCriticalError(ECriticalError error, const i8_t* fn, u32_t line);
		bool hasCriticalErrors() const { return m_CriticalErrors != 0; }
		const char* getCriticalErrorMsg(ECriticalError error) const;
		const char* getFunctionInError() const { return m_FunctionInError.c_str(); }

		void setUserDataPtr( void* ptr ) { m_UserPtr = ptr; }
		void* getUserDataPtr() const { return m_UserPtr; }

		class ZNode* zn() const { return m_ZNode; } // User
		class RecvNode* rn() const { return m_RecvNode; } // Internal (raw dispatch & send)
		class ConnectionNode* cn() const { return m_ConnectionNode; } // Controls state of connections
		class VariableGroupNode* vgn() const { return m_VariableGroupNode; } // Controls state of variable groups
		class MasterServer* ms() const { return m_MasterServer; }

	private:
		void* m_UserPtr;
		bool m_IsP2P;
		bool m_IsListening;
		bool m_IsSuperPeer;
		std::string m_FunctionInError;
		std::atomic_uint32_t m_CriticalErrors;
		class ZNode* m_ZNode;
		class RecvNode* m_RecvNode;
		class ConnectionNode* m_ConnectionNode;
		class VariableGroupNode* m_VariableGroupNode;
		class MasterServer* m_MasterServer;
		std::vector<CustomDataCallback>	m_CustomDataCallbacks;
	};
}