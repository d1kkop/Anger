#pragma once

#include <cassert>
#include "Platform.h"

#define RPC_NAME_MAX_LENGTH 32


#define RPC_CPY_N_SEND2(name) \
	gn->beginSend(specific, exclude);\
	gn->send((unsigned char)EGameNodePacketType::Rpc, (const char*)&t, sizeof(name), transType, channel, relay);\
	gn->endSend();

#define RPC_ASSERT_N_CPY2(name) \
	assert( len == (sizeof(name)) && "invalid struct size" ); \
	memcpy(&t, data, len);


#define RPC_FUNC_0( name) \
	void name( );\
	ALIGN(8) struct rpc_struct_##name { char rpc_name[RPC_NAME_MAX_LENGTH]; }; \
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
		{ \
			name(); \
		}\
	}\
	void rpc_##name( ZNode* gn, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
	{\
		rpc_struct_##name t; \
		t.rpc_name[0]='\0'; \
		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		RPC_CPY_N_SEND2(rpc_struct_##name) \
		if ( localCall ) { name(); } \
	}\
	void name()


#define RPC_FUNC_1( name, a, at ) \
	void name( a at );\
	ALIGN(8) struct rpc_struct_##name  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; }; \
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
		{ \
			rpc_struct_##name t; \
			RPC_ASSERT_N_CPY2(rpc_struct_##name) \
			name( t._a ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
	{\
		rpc_struct_##name t; \
		t._a = at; \
		t.rpc_name[0]='\0'; \
		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		RPC_CPY_N_SEND2(rpc_struct_##name) \
		if ( localCall ) { name( at ); } \
	}\
	void name( a at )


#define RPC_FUNC_2( name, a, at, b, bt ) \
	void name( a at, b bt );\
	ALIGN(8) struct rpc_struct_##name { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; }; \
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
		{ \
			rpc_struct_##name t; \
			RPC_ASSERT_N_CPY2(rpc_struct_##name) \
			name( t._a, t._b ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
	{\
		rpc_struct_##name t; \
		t._a = at; t._b = bt; \
		t.rpc_name[0] = '\0'; \
		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		RPC_CPY_N_SEND2(rpc_struct_##name) \
		if ( localCall ) { name( at, bt ); } \
	}\
	void name( a at, b bt )


#define RPC_FUNC_3( name, a, at, b, bt, c, ct ) \
	void name( a at, b bt, c ct );\
	ALIGN(8) struct rpc_struct_##name  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; }; \
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
		{ \
			rpc_struct_##name t;\
			RPC_ASSERT_N_CPY2(rpc_struct_##name) \
			name( t._a, t._b, t._c ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
	{\
		rpc_struct_##name t;\
		t._a = at; t._b = bt; t._c = ct;\
		t.rpc_name[0] = '\0'; \
		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		RPC_CPY_N_SEND2(rpc_struct_##name) \
		if ( localCall ) { name( at, bt, ct ); } \
	}\
	void name( a at, b bt, c ct )


#define RPC_FUNC_4( name, a, at, b, bt, c, ct, d, dt ) \
	ALIGN(8) struct rpc_struct_##name { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; }; \
	void name( a at, b bt, c ct, d dt );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
		{ \
			rpc_struct_##name t;\
			RPC_ASSERT_N_CPY2(rpc_struct_##name) \
			name( t._a, t._b, t._c, t._d ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
	{\
		rpc_struct_##name t;\
		t._a = at; t._b = bt; t._c = ct; t._d = dt; \
		t.rpc_name[0] = '\0'; \
		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		RPC_CPY_N_SEND2(rpc_struct_##name) \
		if ( localCall ) { name( at, bt, ct, dt ); } \
	}\
	void name( a at, b bt, c ct, d dt )


#define RPC_FUNC_5( name, a, at, b, bt, c, ct, d, dt, e, et ) \
	ALIGN(8) struct rpc_struct_##name  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; e _e; }; \
	void name( a at, b bt, c ct, d dt, e et );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
		{ \
			rpc_struct_##name t;\
			RPC_ASSERT_N_CPY2(rpc_struct_##name) \
			name( t._a, t._b, t._c, t._d, t._e ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
	{\
		rpc_struct_##name t;\
		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; \
		t.rpc_name[0] = '\0'; \
		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		RPC_CPY_N_SEND2(rpc_struct_##name) \
		if ( localCall ) { name( at, bt, ct, dt, et ); } \
	}\
	void name( a at, b bt, c ct, d dt, e et )

#define RPC_FUNC_6( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft ) \
	ALIGN(8) struct rpc_struct_##name  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; e _e; f _f; }; \
	void name( a at, b bt, c ct, d dt, e et, f ft );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
		{ \
			rpc_struct_##name t;\
			RPC_ASSERT_N_CPY2(rpc_struct_##name) \
			name( t._a, t._b, t._c, t._d, t._e, t._f ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
	{\
		rpc_struct_##name t;\
		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; \
		t.rpc_name[0] ='\0'; \
		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		RPC_CPY_N_SEND2(rpc_struct_##name) \
		if ( localCall ) { name( at, bt, ct, dt, et, ft ); } \
	}\
	void name( a at, b bt, c ct, d dt, e et, f ft )


#define RPC_FUNC_7( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht ) \
	ALIGN(8) struct rpc_struct_##name  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; e _e; f _f; h _h; } ;\
	void name( a at, b bt, c ct, d dt, e et, f ft, h ht );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
		{ \
			rpc_struct_##name t;\
			RPC_ASSERT_N_CPY2(rpc_struct_##name) \
			name( t._a, t._b, t._c, t._d, t._e, t._f, t._h ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
	{\
		rpc_struct_##name t; \
		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; t._h = ht; \
		t.rpc_name[0] = '\0'; \
		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		RPC_CPY_N_SEND2(rpc_struct_##name) \
		if ( localCall ) { name( at, bt, ct, dt, et, ft, ht ); } \
	}\
	void name( a at, b bt, c ct, d dt, e et, f ft, h ht )


#define RPC_FUNC_8( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht, i, it ) \
	ALIGN(8) struct rpc_struct_##name  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; e _e; f _f; h _h; i _i; }; \
	void name( a at, b bt, c ct, d dt, e et, f ft, h ht, i it );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
		{ \
			rpc_struct_##name t; \
			RPC_ASSERT_N_CPY2(rpc_struct_##name) \
			name( t._a, t._b, t._c, t._d, t._e, t._f, t._h, t._i ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
	{\
		rpc_struct_##name t; \
		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; t._h = ht; t._i = it; \
		t.rpc_name[0] = '\0'; \
		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		RPC_CPY_N_SEND2(rpc_struct_##name) \
		if ( localCall ) { name( at, bt, ct, dt, et, ft, ht, it ); } \
	}\
	void name( a at, b bt, c ct, d dt, e et, f ft, h ht, i it )


#define RPC_FUNC_9( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht, i, it, j, jt ) \
	ALIGN(8) struct rpc_struct_##name  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; e _e; f _f; h _h; i _i; j _j; }; \
	void name( a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt );\
	extern "C" {\
		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
		{ \
			rpc_struct_##name t;\
			RPC_ASSERT_N_CPY2(rpc_struct_##name) \
			name( t._a, t._b, t._c, t._d, t._e, t._f, t._h, t._i, t._j ); \
		}\
	}\
	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
	{\
		rpc_struct_##name t;\
		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; t._h = ht; t._i = it; t._j = jt; \
		t.rpc_name[0] = '\0'; \
		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		RPC_CPY_N_SEND2(rpc_struct_##name) \
		if ( localCall ) { name( at, bt, ct, dt, et, ft, ht, it, jt ); } \
	}\
	void name( a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt )
