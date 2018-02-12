#include "MasterServer.h"
#include "Platform.h"
#include "RpcMacros.h"
#include "BinSerializer.h"
#include "CoreNode.h"
#include "RecvNode.h"


namespace Zerodelay
{
	MasterServer::MasterServer():
		m_Z(nullptr),
		m_CoreNode(nullptr),
		m_Initialized(false),
		m_MasterReceiveState(MasterServerReceiveState::Begin)
	{
	}

	MasterServer::~MasterServer()
	{
	//	disconnect();
	}

	void MasterServer::postInitialize(class CoreNode* coreNode)
	{
		assert(!m_CoreNode);
		m_CoreNode = coreNode;
		m_Z = m_CoreNode->zn();
	}

	void MasterServer::initialize(u16_t port, u32_t maxServers)
	{
		assert(m_CoreNode && m_Z && !m_Initialized);
		m_Z->listen(port, "", maxServers);
		m_Z->setUserDataPtr(this);
		m_Z->bindOnNewConnection( [&](auto b, auto e, auto a) { onNewConnection(b, e, a); } );
		m_Z->bindOnDisconnect( [&](auto b, auto e, auto a) { onDisconnect(b, e, a); } );
		m_Initialized = true;
	}

	void MasterServer::disconnect()
	{
		if (m_Z && m_Initialized)
		{
			m_Z->disconnect(1000);
		}
	}

	void MasterServer::update()
	{
		m_Z->update();
	}

	void MasterServer::onNewConnection(bool directLink, const ZEndpoint& etp, const std::map<std::string, std::string>& additionalData)
	{
		ServerPlayer sp;
		sp.endpoint = etp;
		if ( m_ServerPlayers.count( etp) == 0 )
		{
			m_ServerPlayers[etp] = sp;
		}
		else
		{
			Platform::log("WARNING: Detected that server %s was already registered at master server.", etp.toIpAndPort().c_str());
		}
	}

	void MasterServer::onDisconnect(bool directLink, const ZEndpoint& etp, EDisconnectReason reason)
	{
		if ( m_ServerPlayers.count( etp) == 1 )
		{
			m_ServerPlayers.erase(etp);
		}
		else
		{
			Platform::log("WARNING: Received disconnect from %s while this server was not known to the master server.", etp.toIpAndPort().c_str());
		}
	}

	void MasterServer::sendServerListTo(const ZEndpoint& etp)
	{
		m_Z->sendReliableOrdered((i8_t) EDataPacketType::MSServerListBegin, nullptr, 0, &etp, false, MSChannel, false);

		BinSerializer stream;
		__CHECKED( stream.moveWrite(2) ); // reserver space for count
		u16_t numServers=0;
		for ( auto& kvp : m_ServerPlayers )
		{
			ServerPlayer& sp = kvp.second;
			__CHECKED( stream.write(sp.endpoint) );
			numServers++;
			if ( stream.length() >= RPC_DATA_MAX )
			{
				u32_t oldWrite = stream.getWrite();
				__CHECKED( stream.setWrite(0) );
				__CHECKED( stream.write(numServers) ); // count
				__CHECKED( stream.setWrite(oldWrite) ); // store write position
				m_Z->sendReliableOrdered((i8_t)EDataPacketType::MSServerList, stream.data(), stream.length(), &etp, false, MSChannel, false);

				// reset for next
				numServers = 0;
				__CHECKED( stream.setWrite(2) );
			}
		}
		if (numServers > 0)
		{
			u32_t oldWrite = stream.getWrite();
			__CHECKED( stream.setWrite(0) );
			__CHECKED( stream.write(numServers) ); // count
			__CHECKED( stream.setWrite(oldWrite) ); // store write position
			m_Z->sendReliableOrdered((i8_t)EDataPacketType::MSServerList, stream.data(), stream.length(), &etp, false, MSChannel, false);
		}

		m_Z->sendReliableOrdered((i8_t)EDataPacketType::MSServerListEnd, nullptr, 0, &etp, false, MSChannel, false);
	}

	void MasterServer::sendServerlistRequest(const ZEndpoint& etp)
	{
		m_Z->sendReliableOrdered( (i8_t)EDataPacketType::MSServerListRequest, nullptr, 0, &etp, false, MSChannel, false);
	}

	bool MasterServer::processPacket(const struct Packet& pack, const EndPoint& etp)
	{
		if ( pack.type == EHeaderPacketType::Reliable_Ordered )
		{
			EDataPacketType packType = (EDataPacketType)pack.data[0];
			switch ( packType )
			{
			case EDataPacketType::MSServerList:
				recvServerList( pack, etp );	
				break;
			default:
				// unhandled packet
				return false;
			}
			// packet was handled
			return true;
		}
		return false; // unhandled packet
	}

	void MasterServer::recvServerListBegin(const struct Packet& pack, const EndPoint& etp)
	{
		if ( m_MasterReceiveState != Begin )
		{
			Platform::log("WARNING: Received server list begin from master server while state was not 'Begin'.");
		}
		m_MasterReceiveState = List;
		m_ServerPlayersClientTemp.clear();
	}

	void MasterServer::recvServerList(const struct Packet& pack, const EndPoint& etp)
	{
		if ( m_MasterReceiveState != List )
		{
			Platform::log("WARNING: Received server list from master server while state was not 'List'. Ignoring data.");
			return;
		}

		BinSerializer stream(pack.data, pack.len, pack.len);
		__CHECKED( stream.moveWrite(1) ); // skip id

		i16_t numServers;
		__CHECKED( stream.read(numServers) );
		while (numServers-- != 0)
		{
			ZEndpoint endpoint;
			__CHECKED( stream.read(endpoint) );
			ServerPlayer sp;
			if (0 == m_ServerPlayersClientTemp.count(endpoint))
			{
				m_ServerPlayersClientTemp[endpoint] = sp;
			}
			else
			{
				Platform::log("WARNING: Received endpoint %s in master server list which is already known.", endpoint.toIpAndPort().c_str());
			}
		}
	}

	void MasterServer::recvServerListEnd(const struct Packet& pack, const EndPoint& etp)
	{
		if ( m_MasterReceiveState != List )
		{
			Platform::log("WARNING: Received server list end from master server while state was not 'List'.");
		}
		m_MasterReceiveState = Begin;
		// now patch the entire list
		m_ServerPlayersClient = m_ServerPlayersClientTemp;
		m_ServerPlayersClientTemp.clear();
	}

}