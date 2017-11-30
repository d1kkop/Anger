#include "CoreNode.h"
#include "RecvNode.h"
#include "Connection.h"
#include "RpcMacros.h"
#include "ConnectionNode.h"
#include "VariableGroupNode.h"



namespace Zerodelay
{
	extern ZEndpoint toZpt( const EndPoint& r );
	extern EndPoint  toEtp( const ZEndpoint& z );


	CoreNode::CoreNode(class ZNode* zn, class RecvNode* rn, class ConnectionNode* cn, class VariableGroupNode* vgn):
		m_UserPtr(nullptr),
		m_IsP2P(false),
		m_IsListening(false),
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
		delete m_ConnectionNode;
		delete m_VariableGroupNode;
		delete m_RecvNode;
	}

	void CoreNode::recvRpcPacket(const i8_t* payload, i32_t len, const EndPoint& etp)
	{
		i8_t funcName[RPC_NAME_MAX_LENGTH*2];
		auto* ptrNext = Util::appendString(funcName, RPC_NAME_MAX_LENGTH*2, "__rpc_deserialize_");
		i32_t kRead   = Util::readString(ptrNext, RPC_NAME_MAX_LENGTH, payload, len);
		if ( kRead < 0 )
		{
			setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION);
			return;
		}
		void* pf = Platform::getPtrFromName( funcName );
		if ( pf )
		{
			// function signature
			void (*pfunc)(const ZEndpoint&, void*, const i8_t*, i32_t);
			pfunc = (decltype(pfunc)) pf;
			ZEndpoint ztp = toZpt(etp);
			pfunc( ztp, m_UserPtr, payload+kRead+1, len );
		}
		else
		{
			setCriticalError(ECriticalError::CannotFindExternalCFunction, ZERODELAY_FUNCTION);
			Platform::log("CRITICAL: Cannot find external C function %s", funcName);
		}
	}

	void CoreNode::recvUserPacket(const Packet& pack, const EndPoint& etp)
	{
		if ( pack.relay && isSuperPeer() && !isP2P() ) // send through to others
		{
			// except self
			m_RecvNode->send( pack.data[0], pack.data+1, pack.len-1, &etp, true, pack.type, pack.channel, false /* relay only once */ );
		}
		ZEndpoint ztp = toZpt(etp);
		Util::forEachCallback(m_CustomDataCallbacks, [&](auto& fcb)
		{
			(fcb)(ztp, pack.data[0], pack.data+1, pack.len-1, pack.channel);
		});
	}

	void CoreNode::processUnhandledPacket(Packet& pack, const EndPoint& etp)
	{
		EDataPacketType packType = (EDataPacketType)pack.data[0];
		const i8_t* payload  = pack.data+1;		// first byte is PacketType
		i32_t payloadLen	 = pack.len-1;		// len includes the packetType byte
												
		if ( (u8_t)packType >= USER_ID_OFFSET )
		{
			recvUserPacket(pack, etp);
			return;
		}

		switch (packType)
		{
		case EDataPacketType::Rpc:
			recvRpcPacket(payload, payloadLen, etp);
			break;
		default:
			Platform::log("Received unhandled packet from: %s", etp.asString());	
			break;
		}
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
			case ECriticalError::TooMuchDataToSend:
			return "Too much data to send";
		}
		return "";
	}

}