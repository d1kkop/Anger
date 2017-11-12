#pragma once

#include <cassert>
#include "Zerodelay.h"

using namespace Zerodelay;

#define RPC_NAME_MAX_LENGTH 32
#define RPC_DATA_MAX 1500

#if _WIN32
	#define RPC_EXPORT __declspec(dllexport)
	#define ALIGN(n) __declspec(align(n))
	#pragma comment(lib, "User32.lib")
#else
	#define RPC_EXPORT
	#define ALIGN(n)
#endif

namespace __sr
{
	template <typename T>
	void read(const i8_t*& data, T& to) 
	{
		to = *(T*)data;
		data += sizeof(T);
	}

	template <typename T>
	void write(i8_t*& data, T& from)
	{
		*(T*)data = from;
		data += sizeof(T);
	}

	inline void writeStr(i8_t*& data, const i8_t* _c)
	{
		while (*_c!='\0') *data++=*_c++;
		*data++='\0';
	}
}


#define RPC_SEND(len) \
	assert(len <= RPC_DATA_MAX); \
	gn->sendReliableOrdered((u8_t)EDataPacketType::Rpc, data, (i32_t)(len), specific, exclude, channel, relay)


#define RPC_FUNC_0( name ) \
	void name( const ZEndpoint* etp, void* userData );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const ZEndpoint& etp, void* userData, const i8_t* data, i32_t len ) \
		{ \
			name( &etp, userData ); \
		}\
	}\
	void rpc_##name( ZNode* gn, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true ) \
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data; \
		__sr::writeStr(p, #name); \
		RPC_SEND(p - data); \
		if ( localCall ) { name( nullptr, gn->getUserDataPtr() ); } \
	}\
	void name( const ZEndpoint* etp, void* userData )


#define RPC_FUNC_1( name, a, at ) \
	void name( const ZEndpoint* etp, void* userData, a at );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const ZEndpoint& etp, void* userData, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			name( &etp, userData, at ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		RPC_SEND(p - data); \
		if ( localCall ) { name( nullptr, gn->getUserDataPtr(), at ); } \
	}\
	void name( const ZEndpoint* etp, void* userData, a at )


#define RPC_FUNC_2( name, a, at, b, bt ) \
	void name( const ZEndpoint* etp, void* userData, a at, b bt );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const ZEndpoint& etp, void* userData, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			name( &etp, userData, at, bt ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		RPC_SEND(p - data); \
		if ( localCall ) { name( nullptr, gn->getUserDataPtr(), at, bt ); } \
	}\
	void name( const ZEndpoint* etp, void* userData, a at, b bt )


#define RPC_FUNC_3( name, a, at, b, bt, c, ct ) \
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const ZEndpoint& etp, void* userData, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			name( &etp, userData, at, bt, ct ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		__sr::write(p, ct); \
		RPC_SEND(p - data); \
		if ( localCall ) { name( nullptr, gn->getUserDataPtr(), at, bt, ct ); } \
	}\
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct )


#define RPC_FUNC_4( name, a, at, b, bt, c, ct, d, dt ) \
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const ZEndpoint& etp, void* userData, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			name( &etp, userData, at, bt, ct, dt ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		__sr::write(p, ct); \
		__sr::write(p, dt); \
		RPC_SEND(p - data); \
		if ( localCall ) { name( nullptr, gn->getUserDataPtr(), at, bt, ct, dt ); } \
	}\
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt )


#define RPC_FUNC_5( name, a, at, b, bt, c, ct, d, dt, e, et ) \
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt, e et );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const ZEndpoint& etp, void* userData, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			e et; __sr::read(data, et); \
			name( &etp, userData, at, bt, ct, dt, et ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		__sr::write(p, ct); \
		__sr::write(p, dt); \
		__sr::write(p, et); \
		RPC_SEND(p - data); \
		if ( localCall ) { name( nullptr, gn->getUserDataPtr(), at, bt, ct, dt, et ); } \
	}\
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt, e et )

#define RPC_FUNC_6( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft ) \
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt, e et, f ft );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const ZEndpoint& etp, void* userData, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			e et; __sr::read(data, et); \
			f ft; __sr::read(data, ft); \
			name( &etp, userData, at, bt, ct, dt, et, ft ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		__sr::write(p, ct); \
		__sr::write(p, dt); \
		__sr::write(p, et); \
		__sr::write(p, ft); \
		RPC_SEND(p - data); \
		if ( localCall ) { name( nullptr, gn->getUserDataPtr(), at, bt, ct, dt, et, ft ); } \
	}\
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt, e et, f ft )


#define RPC_FUNC_7( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht ) \
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt, e et, f ft, h ht );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const ZEndpoint& etp, void* userData, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			e et; __sr::read(data, et); \
			f ft; __sr::read(data, ft); \
			h ht; __sr::read(data, ht); \
			name( &etp, userData, at, bt, ct, dt, et, ft, ht ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		__sr::write(p, ct); \
		__sr::write(p, dt); \
		__sr::write(p, et); \
		__sr::write(p, ft); \
		__sr::write(p, ht); \
		RPC_SEND(p - data); \
		if ( localCall ) { name( nullptr, gn->getUserDataPtr(), at, bt, ct, dt, et, ft, ht ); } \
	}\
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt, e et, f ft, h ht )


#define RPC_FUNC_8( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht, i, it ) \
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt, e et, f ft, h ht, i it );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const ZEndpoint& etp, void* userData, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			e et; __sr::read(data, et); \
			f ft; __sr::read(data, ft); \
			h ht; __sr::read(data, ht); \
			i it; __sr::read(data, it); \
			name( &etp, userData, at, bt, ct, dt, et, ft, ht, it ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		__sr::write(p, ct); \
		__sr::write(p, dt); \
		__sr::write(p, et); \
		__sr::write(p, ft); \
		__sr::write(p, ht); \
		__sr::write(p, it); \
		RPC_SEND(p - data); \
		if ( localCall ) { name( nullptr, gn->getUserDataPtr(), at, bt, ct, dt, et, ft, ht, it ); } \
	}\
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt, e et, f ft, h ht, i it )


#define RPC_FUNC_9( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht, i, it, j, jt ) \
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const ZEndpoint& etp, void* userData, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			e et; __sr::read(data, et); \
			f ft; __sr::read(data, ft); \
			h ht; __sr::read(data, ht); \
			i it; __sr::read(data, it); \
			j jt; __sr::read(data, jt); \
			name( &etp, userData, at, bt, ct, dt, et, ft, ht, it, jt ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, u8_t channel=0, bool relay=true )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		__sr::write(p, ct); \
		__sr::write(p, dt); \
		__sr::write(p, et); \
		__sr::write(p, ft); \
		__sr::write(p, ht); \
		__sr::write(p, it); \
		__sr::write(p, jt); \
		RPC_SEND(p - data); \
		if ( localCall ) { name( nullptr, gn->getUserDataPtr(), at, bt, ct, dt, et, ft, ht, it, jt ); } \
	}\
	void name( const ZEndpoint* etp, void* userData, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt )
