#pragma once

#include <cassert>
#include "RpcMacros.h"

//#define RPC_NAME_MAX_LENGTH 32
//
//
//#ifdef _WIN32
//#define RPC_EXPORT __declspec(dllexport)
//#else
//		RPC_EXPORT
//#endif
//

//
//#define RPC_ASSERT_N_CPY \
//			assert( len == sizeof(temp) && "invalid struct size" ); \
//			memcpy(&t, data, len);


//#define RPC_FUNC_0( name) \
//	void name( );\
//	extern "C" {\
//		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
//		{ \
//			name(); \
//		}\
//	}\
//	void rpc_##name( ZNode* gn, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
//	{\
//		struct temp  { char rpc_name[RPC_NAME_MAX_LENGTH]; }; temp t;\
//		::memset( t.rpc_name, '\0', RPC_NAME_MAX_LENGTH ); \
//		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
//		RPC_CPY_N_SEND \
//		if ( localCall ) { name(); } \
//	}\
//	void name()
//
//
//#define RPC_FUNC_1( name, a, at ) \
//	void name( a at );\
//	extern "C" {\
//		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
//		{ \
//			struct temp { a _a; }; temp t;\
//			RPC_ASSERT_N_CPY \
//			name( t._a ); \
//		}\
//	}\
//	void rpc_##name( ZNode* gn, a at, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
//	{\
//		struct temp  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; }; temp t;\
//		t._a = at; \
//		::memset( t.rpc_name, '\0', RPC_NAME_MAX_LENGTH ); \
//		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
//		RPC_CPY_N_SEND \
//		if ( localCall ) { name( at ); } \
//	}\
//	void name( a at )


#define SYNC_GROUP_2( name, a, at, b, bt ) \
	void name( a at, b bt );\
	struct sgp_struct_##name { char sgp_name[RPC_NAME_MAX_LENGTH]; unsigned int nId; a _a; b _b; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, char* data, int len, const ZEndpoint& ztp ) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( t->_a, t->_b ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt )\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		t._a = at; t._b = bt; \
		gn->beginVariableGroup((const char*)&t, sizeof(t)); \
		name( at, bt ); \
		gn->endVariableGroup(); \
	}\
	void name( a at, b bt )

//
//#define RPC_FUNC_3( name, a, at, b, bt, c, ct ) \
//	void name( a at, b bt, c ct );\
//	extern "C" {\
//		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
//		{ \
//			struct temp { a _a; b _b; c _c; }; temp t;\
//			RPC_ASSERT_N_CPY \
//			name( t._a, t._b, t._c ); \
//		}\
//	}\
//	void rpc_##name( ZNode* gn, a at, b bt, c ct, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
//	{\
//		struct temp  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; }; temp t;\
//		t._a = at; t._b = bt; t._c = ct;\
//		::memset( t.rpc_name, '\0', RPC_NAME_MAX_LENGTH ); \
//		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
//		RPC_CPY_N_SEND \
//		if ( localCall ) { name( at, bt, ct ); } \
//	}\
//	void name( a at, b bt, c ct )
//
//
//#define RPC_FUNC_4( name, a, at, b, bt, c, ct, d, dt ) \
//	void name( a at, b bt, c ct, d dt );\
//	extern "C" {\
//		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
//		{ \
//			struct temp { a _a; b _b; c _c; d _d; }; temp t;\
//			RPC_ASSERT_N_CPY \
//			name( t._a, t._b, t._c, t._d ); \
//		}\
//	}\
//	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
//	{\
//		struct temp  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; }; temp t;\
//		t._a = at; t._b = bt; t._c = ct; t._d = dt; \
//		::memset( t.rpc_name, '\0', RPC_NAME_MAX_LENGTH ); \
//		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
//		RPC_CPY_N_SEND \
//		if ( localCall ) { name( at, bt, ct, dt ); } \
//	}\
//	void name( a at, b bt, c ct, d dt )
//
//
//#define RPC_FUNC_5( name, a, at, b, bt, c, ct, d, dt, e, et ) \
//	void name( a at, b bt, c ct, d dt, e et );\
//	extern "C" {\
//		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
//		{ \
//			struct temp { a _a; b _b; c _c; d _d; e _e; }; temp t;\
//			RPC_ASSERT_N_CPY \
//			name( t._a, t._b, t._c, t._d, t._e ); \
//		}\
//	}\
//	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
//	{\
//		struct temp  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; e _e; }; temp t;\
//		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; \
//		::memset( t.rpc_name, '\0', RPC_NAME_MAX_LENGTH ); \
//		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
//		RPC_CPY_N_SEND \
//		if ( localCall ) { name( at, bt, ct, dt, et ); } \
//	}\
//	void name( a at, b bt, c ct, d dt, e et )
//
//#define RPC_FUNC_6( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft ) \
//	void name( a at, b bt, c ct, d dt, e et, f ft );\
//	extern "C" {\
//		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
//		{ \
//			struct temp { a _a; b _b; c _c; d _d; e _e; f _f; }; temp t;\
//			RPC_ASSERT_N_CPY \
//			name( t._a, t._b, t._c, t._d, t._e, t._f ); \
//		}\
//	}\
//	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
//	{\
//		struct temp  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; e _e; f _f; }; temp t;\
//		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; \
//		::memset( t.rpc_name, '\0', RPC_NAME_MAX_LENGTH ); \
//		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
//		RPC_CPY_N_SEND \
//		if ( localCall ) { name( at, bt, ct, dt, et, ft ); } \
//	}\
//	void name( a at, b bt, c ct, d dt, e et, f ft )
//
//
//#define RPC_FUNC_7( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht ) \
//	void name( a at, b bt, c ct, d dt, e et, f ft, h ht );\
//	extern "C" {\
//		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
//		{ \
//			struct temp { a _a; b _b; c _c; d _d; e _e; f _f; h _h; }; temp t;\
//			RPC_ASSERT_N_CPY \
//			name( t._a, t._b, t._c, t._d, t._e, t._f, t._h ); \
//		}\
//	}\
//	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
//	{\
//		struct temp  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; e _e; f _f; h _h; }; temp t;\
//		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; t._h = ht; \
//		::memset( t.rpc_name, '\0', RPC_NAME_MAX_LENGTH ); \
//		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
//		RPC_CPY_N_SEND \
//		if ( localCall ) { name( at, bt, ct, dt, et, ft, ht ); } \
//	}\
//	void name( a at, b bt, c ct, d dt, e et, f ft, h ht )
//
//
//#define RPC_FUNC_8( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht, i, it ) \
//	void name( a at, b bt, c ct, d dt, e et, f ft, h ht, i it );\
//	extern "C" {\
//		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
//		{ \
//			struct temp { a _a; b _b; c _c; d _d; e _e; f _f; h _h; i _i; }; temp t;\
//			RPC_ASSERT_N_CPY \
//			name( t._a, t._b, t._c, t._d, t._e, t._f, t._h, t._i ); \
//		}\
//	}\
//	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
//	{\
//		struct temp  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; e _e; f _f; h _h; i _i; }; temp t;\
//		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; t._h = ht; t._i = it; \
//		::memset( t.rpc_name, '\0', RPC_NAME_MAX_LENGTH ); \
//		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
//		RPC_CPY_N_SEND \
//		if ( localCall ) { name( at, bt, ct, dt, et, ft, ht, it ); } \
//	}\
//	void name( a at, b bt, c ct, d dt, e et, f ft, h ht, i it )
//
//
//#define RPC_FUNC_9( name, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht, i, it, j, jt ) \
//	void name( a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt );\
//	extern "C" {\
//		RPC_EXPORT void __rpc_deserialize_##name( const char* data, int len ) \
//		{ \
//			struct temp { a _a; b _b; c _c; d _d; e _e; f _f; h _h; i _i; j _j; }; temp t;\
//			RPC_ASSERT_N_CPY \
//			name( t._a, t._b, t._c, t._d, t._e, t._f, t._h, t._i, t._j ); \
//		}\
//	}\
//	void rpc_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt, bool localCall = true, const ZEndpoint* specific=nullptr, bool exclude=false, EPacketType transType=EPacketType::Reliable_Ordered, unsigned char channel=0, bool relay=true )\
//	{\
//		struct temp  { char rpc_name[RPC_NAME_MAX_LENGTH]; a _a; b _b; c _c; d _d; e _e; f _f; h _h; i _i; j _j; }; temp t;\
//		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; t._h = ht; t._i = it; t._j = jt; \
//		::memset( t.rpc_name, '\0', RPC_NAME_MAX_LENGTH ); \
//		::sprintf_s( t.rpc_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
//		RPC_CPY_N_SEND \
//		if ( localCall ) { name( at, bt, ct, dt, et, ft, ht, it, jt ); } \
//	}\
//	void name( a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt )
