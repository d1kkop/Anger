#pragma once

#include "Platform.h"
#include "EndPoint.h"

#include <mutex>
#include <deque>
#include <atomic>


namespace Zerodelay
{
	enum SocketError
	{
		Succes = 0,
		NotOpened,
		NotBound,
		CannotCreate,
		CannotOpen,
		CannotCreateSet,
		CannotAddToSet,
		CannotBind,
		PortAlreadyInUse,
		SendFailure,
		RecvFailure,
		CannotResolveLocalAddress
	};

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
	protected:
		ISocket();

	public:
		static ISocket* create();
		virtual ~ISocket() = default;

		// Interface
		virtual bool open(IPProto ipProto = IPProto::Ipv4, bool reuseAddr=false) = 0;
		virtual bool bind(u16_t port) = 0;
		virtual bool close() = 0;
		virtual ESendResult send( const struct EndPoint& endPoint, const i8_t* data, i32_t len ) = 0;
		virtual ERecvResult recv( i8_t* buff, i32_t& rawSize, struct EndPoint& endpointOut ) = 0; // buffSize in, received size out

		// Shared
		bool isOpen() const  { return m_Open; }
		bool isBound() const { return m_Bound; }
		bool isBlocking() const { return m_Blocking; }
		IPProto getIpProtocol() const { return m_IpProto; }
		i32_t getUnderlayingSocketError() const { return (i32_t) m_LastError; }

	protected:
		bool m_Open;
		bool m_Bound;
		bool m_Blocking;
		IPProto m_IpProto;
		SocketError m_LastError;
	};

#if ZERODELAY_FAKESOCKET
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

		void storeData( const i8_t* data, i32_t len, const struct EndPoint& endPoint );

		std::string m_Name;
		EndPoint m_SourceEndPoint;

	protected:
		std::mutex m_Mutex;
		std::deque<FakePacket> m_Packets;
	};
#endif

#if ZERODELAY_WIN32SOCKET
	class BSDSocket: public ISocket
	{
	public:
		BSDSocket();

		virtual bool open(IPProto ipProto, bool reuseAddr) override;
		virtual bool bind(u16_t port) override;
		virtual bool close() override;
		virtual ESendResult send( const struct EndPoint& endPoint, const i8_t* data, i32_t len) override;
		virtual ERecvResult recv( i8_t* buff, i32_t& rawSize, struct EndPoint& endPoint ) override;

	protected:
		void setLastError();
		SOCKET m_Socket;
	};
#endif


#if ZERODELAY_SDLSOCKET
	class SDLSocket: public ISocket
	{
	public:
		SDLSocket();
		~SDLSocket() override;

		// ISocket
		virtual bool open(IPProto ipProto, bool reuseAddr) override;
		virtual bool bind(u16_t port) override;
		virtual bool close() override;
		virtual ESendResult send( const struct EndPoint& endPoint, const i8_t* data, i32_t len) override;
		virtual ERecvResult recv( i8_t* buff, i32_t& rawSize, struct EndPoint& endPoint ) override;

	protected:
		UDPsocket m_Socket;
		SDLNet_SocketSet m_SocketSet;
	};
#endif
}