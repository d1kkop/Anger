#pragma once

#include "Platform.h"
#include "EndPoint.h"

#include <mutex>
#include <deque>
#include <atomic>


namespace Zerodelay
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
		virtual bool bind(u16_t port) = 0;
		virtual bool close() = 0;
		virtual ESendResult send( const struct EndPoint& endPoint, const i8_t* data, i32_t len ) = 0;
		virtual ERecvResult recv( i8_t* buff, i32_t& rawSize, struct EndPoint& endpointOut ) = 0; // buffSize in, received size out
		virtual i32_t getUnderlayingSocketError() const = 0;
		virtual bool isBlocking() const = 0;
		virtual bool isOpen() const = 0;
		virtual bool isBound() const = 0;

		static bool readString( i8_t* dst, i32_t dstSize, const i8_t* buffIn, i32_t buffInSize );
		static bool readFixed( i8_t* dst, i32_t dstSize, const i8_t* buffIn, i32_t buffInSize );

	protected:
		IPProto m_IpProto;
	};

#ifdef ZNETWORK_DEBUG
	class FakeSocket: public ISocket
	{
		struct FakePacket
		{
			i8_t* data;
			i32_t len;
			EndPoint ep;
		};

	public:
		virtual ~FakeSocket();

		virtual bool open(IPProto ipProto, bool reuseAddr) override { return true; }
		virtual bool bind(u16_t port) override { return true; }
		virtual bool close() override { return true; }
		virtual ESendResult send( const struct EndPoint& endPoint, const i8_t* data, i32_t len) override;
		virtual ERecvResult recv( i8_t* buff, i32_t& rawSize, struct EndPoint& endPoint ) override;
		virtual i32_t getUnderlayingSocketError() const override { return 0; }
		virtual bool isBlocking() const override { return false; }
		virtual bool isOpen() const { return true; }
		virtual bool isBound() const { return true; }

		void storeData( const i8_t* data, i32_t len, const struct EndPoint& endPoint );

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
		virtual bool bind(u16_t port) override;
		virtual bool close() override;
		virtual ESendResult send( const struct EndPoint& endPoint, const i8_t* data, i32_t len) override;
		virtual ERecvResult recv( i8_t* buff, i32_t& rawSize, struct EndPoint& endPoint ) override;
		virtual i32_t getUnderlayingSocketError() const override;
		virtual bool isBlocking() const override { return true; }
		virtual bool isOpen() const { return m_Open; }
		virtual bool isBound() const { return m_Bound; }

	protected:
		void setLastError();
		i32_t m_LastError;
		bool m_Open;
		bool m_Bound;
#ifdef _WIN32
		SOCKET m_Socket;
#endif
	};
}