#pragma once

#include "Zerodelay.h"
#include "BinSerializer.h"
#include <algorithm>


namespace Zerodelay
{
	constexpr i8_t MSChannel = 6;

	struct LocalServer
	{
		ZEndpoint endpoint;
		u32_t activePlayers;
		std::map<std::string, std::string> metaData;

		LocalServer() : activePlayers(0) { }
	};

	enum MasterServerReceiveState
	{
		Begin,
		List,
		End
	};


	class MasterServer
	{
	public:
		MasterServer();
		~MasterServer();
		void postInitialize(class CoreNode* coreNode);

		void initialize(u16_t port, u32_t maxServers);
		void disconnect();
		void update();

		// Sends to master serv (from user)
		ERegisterServerCallResult registerAsServer( const ZEndpoint& masterEtp, const std::string& name, const std::string& pw, bool isP2p, const std::map<std::string, std::string>& metaData );
		ESendCallResult connectToServer( const ZEndpoint& masterEtp, const std::string& name, const std::string& pw, const std::map<std::string, std::string>& metaData );
		ESendCallResult connectToServer( const ZEndpoint& masterEtp, const ZEndpoint& servIp, const std::string& pw, const std::map<std::string, std::string>& metaData );

		// Sends internal
		void sendServerRegisterResult( const ZEndpoint& recipient, EServerRegisterResult result );
		void sendForwardConnect( const ZEndpoint& sourceIp, const ZEndpoint& destinationIp, const std::string& pw );
		void sendServerConnectResult( const ZEndpoint& recipient, const ZEndpoint& serverEtp, EServerConnectResult serverConnRes );
		void sendServerListRequest( const ZEndpoint& masterEtp, const std::string& name );

		// Recvs
		bool processPacket(const struct Packet& pack, const struct EndPoint& etp);
		void recvRegisterServer(const struct Packet& pack, const struct ZEndpoint& etp);
		void recvRegisterServerResult(const struct Packet& pack, const struct ZEndpoint& etp);
		void recvConnectToServer(const struct Packet& pack, const struct ZEndpoint& etp);
		void recvForwardConnect(const struct Packet& pack, const struct ZEndpoint& etp);
		void recvServerConnectResult(const struct Packet& pack, const struct ZEndpoint& etp);
		void recvServerServerListRequest(const struct Packet& pack, const struct ZEndpoint& etp);
		void recvServerServerList(const struct Packet& pack, const struct ZEndpoint& etp);

		// Events
		void addListener(IMasterServerListener* listener)				{ m_Listeners.emplace_back(listener); }
		void removeListener(const IMasterServerListener* listener)		{ std::remove(m_Listeners.begin(), m_Listeners.end(), listener); }


	private:
		ZNode* m_Z;
		CoreNode* m_CoreNode;
		bool m_Initialized;
		BinSerializer bs;
		MasterServerReceiveState m_MasterReceiveState;
		std::vector<IMasterServerListener*> m_Listeners;
		std::map<ZEndpoint, LocalServer, ZEndpoint> m_ServerPlayers;
		std::map<std::string, std::vector<LocalServer*>> m_ServerPlayersByName;
	};

}