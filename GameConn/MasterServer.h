#pragma once

#include "Zerodelay.h"

namespace Zerodelay
{
	constexpr i8_t MSChannel = 6;

	struct ServerPlayer
	{
		ZEndpoint endpoint;
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

		// Connection layer events
		void onNewConnection(bool directLink, const ZEndpoint& etp, const std::map<std::string, std::string>& additionalData);
		void onDisconnect(bool directLink, const ZEndpoint&, EDisconnectReason reason);

		// Sends
		void sendServerListTo(const ZEndpoint& etp);
		void sendServerlistRequest(const ZEndpoint& etp);

		// Recvs
		bool processPacket(const struct Packet& pack, const struct EndPoint& etp);
		void recvServerListBegin(const struct Packet& pack, const struct EndPoint& etp);
		void recvServerList(const struct Packet& pack, const struct EndPoint& etp);
		void recvServerListEnd(const struct Packet& pack, const struct EndPoint& etp);
					   
	private:
		ZNode* m_Z;
		CoreNode* m_CoreNode;
		bool m_Initialized;
		MasterServerReceiveState m_MasterReceiveState;
		std::map<ZEndpoint, ServerPlayer, ZEndpoint> m_ServerPlayers;
		std::map<ZEndpoint, ServerPlayer, ZEndpoint> m_ServerPlayersClient;
		std::map<ZEndpoint, ServerPlayer, ZEndpoint> m_ServerPlayersClientTemp;
	};

}