#pragma once

#include "Platform.h"
#include "EndPoint.h"

#include <mutex>
#include <deque>
#include <atomic>


namespace Motor
{	
	namespace Anger
	{
		enum class IPProto
		{
			Ipv4,
			Ipv6
		};

		enum class ESendResult
		{
			Succes,
			Error,
			SocketClosed
		};

		enum class ERecvResult
		{
			Succes,
			NoData,
			Error,
			SocketClosed
		};


		class ISocket
		{
		public:
			static ISocket* create();
			virtual ~ISocket() { }

			virtual bool open(IPProto ipProto = IPProto::Ipv4, bool reuseAddr=false) = 0;
			virtual bool bind(unsigned short port) = 0;
			virtual bool close() = 0;
			virtual ESendResult send( const struct EndPoint& endPoint, const char* data, int len ) = 0;
			virtual ERecvResult recv( char* buff, int& rawSize, struct EndPoint& endpointOut ) = 0; // buffSize in, received size out
			virtual int getUnderlayingSocketError() const = 0;
			virtual bool isBlocking() const = 0;

		protected:
			IPProto m_IpProto;
		};

#ifdef MOTOR_NETWORK_DEBUG
		class FakeSocket: public ISocket
		{
			struct FakePacket
			{
				char* data;
				int len;
				EndPoint ep;
			};

		public:
			virtual ~FakeSocket();

			virtual bool open(IPProto ipProto, bool reuseAddr) override { return true; }
			virtual bool bind(unsigned short port) override { return true; }
			virtual bool close() override { return true; }
			virtual ESendResult send( const struct EndPoint& endPoint, const char* data, int len) override;
			virtual ERecvResult recv( char* buff, int& rawSize, struct EndPoint& endPoint ) override;
			virtual int getUnderlayingSocketError() const override { return 0; }
			virtual bool isBlocking() const override { return false; }

			void storeData( const char* data, int len, const struct EndPoint& endPoint );

			std::string m_Name;
			EndPoint m_SourceEndPoint;

		protected:
			std::mutex m_Mutex;
			std::deque<FakePacket> m_Packets;
		};
#endif

		class BSDSocket: public ISocket
		{
		public:
			BSDSocket();

			virtual bool open(IPProto ipProto, bool reuseAddr) override;
			virtual bool bind(unsigned short port) override;
			virtual bool close() override;
			virtual ESendResult send( const struct EndPoint& endPoint, const char* data, int len) override;
			virtual ERecvResult recv( char* buff, int& rawSize, struct EndPoint& endPoint ) override;
			virtual int getUnderlayingSocketError() const override;
			virtual bool isBlocking() const override { return true; }

		protected:
			void setLastError();
			int m_LastError;
#ifdef _WIN32
			SOCKET m_Socket;
#endif
		};
	}
}