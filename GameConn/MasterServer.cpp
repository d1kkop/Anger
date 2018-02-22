#include "MasterServer.h"
#include "Platform.h"
#include "RpcMacros.h"
#include "CoreNode.h"
#include "RecvNode.h"
#include "RUDPLink.h"


namespace Zerodelay
{
	constexpr u32_t ms_MinNameLength  = 3;
	constexpr u32_t ms_ConnectTimeout = 8;


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
		m_Initialized = true;
	}

	void MasterServer::disconnect()
	{
		if (m_Z && m_Initialized)
		{
			m_Z->disconnect(1000);
		}
		m_Initialized = false;
	}

	void MasterServer::update()
	{
		m_Z->update();
	}

	ERegisterServerCallResult MasterServer::registerAsServer(const ZEndpoint& masterEtp, const std::string& name, const std::string& pw, bool isP2p, const std::map<std::string, std::string>& metaData)
	{
		if ( name.length() < ms_MinNameLength ) return ERegisterServerCallResult::NameTooShort;

		RUDPLink* link = m_CoreNode->rn()->getLinkAndPinIt(Util::toEtp(masterEtp));
		if ( link && link->isPendingDelete() )
		{
			link->unpinWithLock();
			return ERegisterServerCallResult::AlreadyConnected;
		}

		bs.reset();
		__CHECKEDMSE( bs.write(isP2p) )
		__CHECKEDMSE( bs.write(name) );
		__CHECKEDMSE( bs.write(metaData) );

		link->addToSendQueue( (i8_t)EDataPacketType::MSRegisterServer, bs.data(), bs.length(), EHeaderPacketType::Reliable_Ordered, MSChannel );
		link->unpinWithLock();

		return ERegisterServerCallResult::Succes;
	}

	ESendCallResult MasterServer::connectToServer(const ZEndpoint& masterServerIp, const std::string& name, const std::string& pw, const std::map<std::string, std::string>& metaData)
	{
		if ( name.length() < ms_MinNameLength )  return ESendCallResult::NotSent;

		bs.reset();
		__CHECKEDNS( bs.write(false) ); // has no IP but name attached
		__CHECKEDNS( bs.write(name) );
		__CHECKEDNS( bs.write(pw) );
		__CHECKEDNS( bs.write(metaData) );

		return m_Z->sendReliableOrdered( (i8_t)EDataPacketType::MSConnectToServer, bs.data(), bs.length(), &masterServerIp, false, MSChannel, false, false, nullptr );
	}

	ESendCallResult MasterServer::connectToServer(const ZEndpoint& masterServerIp, const ZEndpoint& serverIp, const std::string& pw, const std::map<std::string, std::string>& metaData)
	{
		if ( !serverIp.isValid() ) return ESendCallResult::NotSent;

		bs.reset();
		__CHECKEDNS( bs.write(true) ); // has IP attached (no name)
		__CHECKEDNS( bs.write(serverIp) );
		__CHECKEDNS( bs.write(pw) );
		__CHECKEDNS( bs.write(metaData) );

		return m_Z->sendReliableOrdered( (i8_t)EDataPacketType::MSConnectToServer, bs.data(), bs.length(), &masterServerIp, false, MSChannel, false, false, nullptr );
	}

	void MasterServer::sendServerRegisterResult(const ZEndpoint& recipient, EServerRegisterResult result)
	{
		bs.reset();
		__CHECKED( bs.write((u8_t)result) );
		m_Z->sendReliableOrdered((i8_t) EDataPacketType::MSRegisterServerResult, bs.data(), bs.length(), &recipient, false, MSChannel, false, false, nullptr );
	}

	void MasterServer::sendForwardConnect(const ZEndpoint& sourceIp, const ZEndpoint& destinationIp, const std::string& pw)
	{
		bs.reset();
		__CHECKED( bs.write(destinationIp) );
		__CHECKED( bs.write(pw) );
		m_Z->sendReliableOrdered((i8_t) EDataPacketType::MSForwardConnect, bs.data(), bs.length(), &sourceIp, false, MSChannel, false);
	}

	void MasterServer::sendServerConnectResult(const ZEndpoint& recipient, const ZEndpoint& serverEtp, EServerConnectResult serverConnRes)
	{
		bs.reset();
		__CHECKED( bs.write(serverEtp) );
		__CHECKED( bs.write((i8_t)serverConnRes) );
		m_Z->sendReliableOrdered((i8_t) EDataPacketType::MSServerConnectResult, bs.data(), bs.length(), &recipient, false, MSChannel, false);
	}

	void MasterServer::sendServerListRequest(const ZEndpoint& masterEtp, const std::string& name)
	{
		bs.reset();
		__CHECKED( bs.write(name) );
		m_Z->sendReliableOrdered((i8_t) EDataPacketType::MSServerListRequest, bs.data(), bs.length(), &masterEtp, false, MSChannel, false);
	}

	bool MasterServer::processPacket(const struct Packet& pack, const EndPoint& etp)
	{
		if ( pack.type == EHeaderPacketType::Reliable_Ordered )
		{
			auto ztp = Util::toZpt(etp);
			EDataPacketType packType = (EDataPacketType)pack.data[0];
			switch ( packType )
			{
			case EDataPacketType::MSRegisterServer:
				recvRegisterServer(pack, ztp);
				break;
			case EDataPacketType::MSRegisterServerResult:
				recvRegisterServerResult(pack, ztp);
				break;
			case EDataPacketType::MSConnectToServer:
				recvConnectToServer(pack, ztp);
				break;
			case EDataPacketType::MSForwardConnect:
				recvForwardConnect(pack, ztp);
				break;
			case EDataPacketType::MSServerConnectResult:
				recvServerConnectResult(pack, ztp);
				break;
			case EDataPacketType::MSServerListRequest:
				recvServerServerListRequest(pack, ztp);
				break;
			case EDataPacketType::MSServerList:
				recvServerServerList(pack, ztp);
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

	void MasterServer::recvRegisterServer(const struct Packet& pack, const struct ZEndpoint& etp)
	{
		bs.resetTo (pack.data, pack.len, pack.len);
		std::string name;
		std::map<std::string, std::string> metaData;
		ZEndpoint servIp;
		bool isP2p;
		__CHECKED(bs.moveRead(1)); // skip dataid
		__CHECKED(bs.read(isP2p));
		__CHECKED(bs.read(name));
		__CHECKED(bs.read(metaData));

		if ( name.length() < ms_MinNameLength ) 
		{
			sendServerRegisterResult( etp, EServerRegisterResult::NameTooShort );
			return;
		}

		if ( m_ServerPlayers.count( etp ) != 0 )
		{
			sendServerRegisterResult( etp, EServerRegisterResult::AlreadyRegistered );
			return;
		}

		LocalServer sp;
		sp.endpoint = etp;
		sp.metaData = metaData;
		m_ServerPlayers[etp] = sp;
		
		sendServerRegisterResult( etp, EServerRegisterResult::Succes );
	}

	void MasterServer::recvRegisterServerResult(const struct Packet& pack, const struct ZEndpoint& etp)
	{
		bs.resetTo (pack.data, pack.len, pack.len);
		EServerRegisterResult res;
		__CHECKED(bs.moveRead(1)); // skip data-id
		__CHECKED(bs.read((u8_t&)(res)));
		for ( auto l : m_Listeners ) l->onServerRegisterResult(etp, res);
	}

	void MasterServer::recvConnectToServer(const struct Packet& pack, const struct ZEndpoint& etp)
	{
		bs.resetTo (pack.data, pack.len, pack.len);
		ZEndpoint serverEtp;
		std::string name;
		bool hasIp;
		__CHECKED( bs.moveRead(1) );
		__CHECKED( bs.read(hasIp) );
		if ( hasIp )
		{
			__CHECKED( bs.read(serverEtp) );
		}
		else
		{
			__CHECKED( bs.read(name) );
		}

		LocalServer* chosenServer = nullptr;
		if ( !hasIp )
		{
			auto serversIt  = m_ServerPlayersByName.find(name);
			if ( serversIt != m_ServerPlayersByName.end() )
			{
				std::vector<LocalServer*>& servers = serversIt->second;
				u32_t playerCnt = UINT_MAX;
				for ( auto s : servers )
				{
					if ( s->activePlayers < playerCnt )
					{
						playerCnt = s->activePlayers;
						chosenServer = s;
					}
				}
			}
		}
		else
		{
			auto serverIpIt = m_ServerPlayers.find(serverEtp);
			if ( serverIpIt != m_ServerPlayers.end() )
			{
				chosenServer = &serverIpIt->second;
			}
		}

		if ( chosenServer )
		{
			sendServerConnectResult(etp, chosenServer->endpoint, EServerConnectResult::Succes );
		}
		else
		{
			ZEndpoint empty;
			sendServerConnectResult(etp, empty, EServerConnectResult::CannotFind );
		}
	}

	void MasterServer::recvForwardConnect(const struct Packet& pack, const struct ZEndpoint& etp)
	{
		bs.resetTo(pack.data, pack.len, pack.len);
		ZEndpoint destinationIp;
		std::string pw;
		__CHECKED(bs.moveRead(1)); // skip dataid
		__CHECKED(bs.read(destinationIp));
		__CHECKED(bs.read(pw));
		EConnectCallResult callRes = m_Z->connect(destinationIp, pw, 15);
		if ( callRes != EConnectCallResult::Succes )
		{
			for (auto l : m_Listeners) l->onForwardConnectFail(etp, destinationIp, callRes);
		}
	}

	void MasterServer::recvServerConnectResult(const struct Packet& pack, const struct ZEndpoint& etp)
	{
		bs.resetTo(pack.data, pack.len, pack.len);
		ZEndpoint serverIp;
		EServerConnectResult serverConnRes;
		__CHECKED(bs.moveRead(1));
		__CHECKED(bs.read(serverIp));
		__CHECKED(bs.read((i8_t&)serverConnRes));
		for (auto l : m_Listeners) l->onServerConnectResult(etp, serverIp, serverConnRes);
		if ( serverConnRes == EServerConnectResult::Succes )
		{
			
		}
	}

	void MasterServer::recvServerServerListRequest(const struct Packet& pack, const struct ZEndpoint& etp)
	{
		bs.resetTo(pack.data, pack.len, pack.len);
		std::string name;
		__CHECKED(bs.moveRead(1));
		__CHECKED(bs.read(name));

	}

	void MasterServer::recvServerServerList(const struct Packet& pack, const struct ZEndpoint& etp)
	{

	}
}