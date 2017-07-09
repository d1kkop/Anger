#pragma once

#include <cassert>
#include "RpcMacros.h"


#define DECL_VAR_GROUP_0( name, _zn) \
	void name( ZNode* _zn );\
	ALIGN(8) struct sgp_struct_##name { i8_t sgp_name[RPC_NAME_MAX_LENGTH]; u32_t nId; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, i8_t* data, i32_t len, const ZEndpoint& ztp) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( gn->priv_getUserNode() ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, u8_t channel=1 )\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		gn->beginVariableGroup((const i8_t*)&t, sizeof(t), channel); \
		name( gn ); \
		gn->endVariableGroup(); \
	}\
	void name( ZNode* _zn )


#define DECL_VAR_GROUP_1( name, _zn, a, at) \
	void name( ZNode* _zn, a at );\
	ALIGN(8) struct sgp_struct_##name { i8_t sgp_name[RPC_NAME_MAX_LENGTH]; u32_t nId; a _a; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, i8_t* data, i32_t len, const ZEndpoint& ztp ) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( gn->priv_getUserNode(), t->_a ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, a at, u8_t channel=1 )\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		t._a = at; \
		gn->beginVariableGroup((const i8_t*)&t, sizeof(t), channel); \
		name( gn, at ); \
		gn->endVariableGroup(); \
	}\
	void name( ZNode* _zn, a at )


#define DECL_VAR_GROUP_2( name, _zn, a, at, b, bt ) \
	void name( ZNode* _zn, a at, b bt );\
	ALIGN(8) struct sgp_struct_##name { i8_t sgp_name[RPC_NAME_MAX_LENGTH]; u32_t nId; a _a; b _b; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, i8_t* data, i32_t len, const ZEndpoint& ztp ) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( gn->priv_getUserNode(), t->_a, t->_b ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, u8_t channel=1 )\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		t._a = at; t._b = bt; \
		gn->beginVariableGroup((const i8_t*)&t, sizeof(t), channel); \
		name( gn, at, bt ); \
		gn->endVariableGroup(); \
	}\
	void name( ZNode* _zn, a at, b bt )


#define DECL_VAR_GROUP_3( name, _zn, a, at, b, bt, c, ct ) \
	void name( ZNode* _zn, a at, b bt, c ct );\
	ALIGN(8) struct sgp_struct_##name { i8_t sgp_name[RPC_NAME_MAX_LENGTH]; u32_t nId; a _a; b _b; c _c; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, i8_t* data, i32_t len, const ZEndpoint& ztp) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( gn->priv_getUserNode(), t->_a, t->_b, t->_c ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, u8_t channel=1 )\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		t._a = at; t._b = bt; t._c = ct; \
		gn->beginVariableGroup((const i8_t*)&t, sizeof(t), channel); \
		name( gn, at, bt, ct ); \
		gn->endVariableGroup(); \
	}\
	void name( ZNode* _zn, a at, b bt, c ct )


#define DECL_VAR_GROUP_4( name, _zn, a, at, b, bt, c, ct, d, dt ) \
	void name( ZNode* _zn, a at, b bt, c ct, d dt );\
	ALIGN(8) struct sgp_struct_##name { i8_t sgp_name[RPC_NAME_MAX_LENGTH]; u32_t nId; a _a; b _b; c _c; d _d; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, i8_t* data, i32_t len, const ZEndpoint& ztp ) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( gn->priv_getUserNode(), t->_a, t->_b, t->_c, t->_d ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, u8_t channel=1 )\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		t._a = at; t._b = bt; t._c = ct; t._d = dt; \
		gn->beginVariableGroup((const i8_t*)&t, sizeof(t), channel); \
		name( gn, at, bt, ct, dt ); \
		gn->endVariableGroup(); \
	}\
	void name( ZNode* _zn, a at, b bt, c ct, d dt )


#define DECL_VAR_GROUP_5( name, _zn, a, at, b, bt, c, ct, d, dt, e, et ) \
	void name( ZNode* _zn, a at, b bt, c ct, d dt, e et );\
	ALIGN(8) struct sgp_struct_##name { i8_t sgp_name[RPC_NAME_MAX_LENGTH]; u32_t nId; a _a; b _b; c _c; d _d; e _e; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, i8_t* data, i32_t len, const ZEndpoint& ztp ) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( gn->priv_getUserNode(), t->_a, t->_b, t->_c, t->_d, t->_e ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, u8_t channel=1 )\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; \
		gn->beginVariableGroup((const i8_t*)&t, sizeof(t), channel); \
		name( gn, at, bt, ct, dt, et ); \
		gn->endVariableGroup(); \
	}\
	void name( ZNode* _zn, a at, b bt, c ct, d dt, e et )


#define DECL_VAR_GROUP_6( name, _zn, a, at, b, bt, c, ct, d, dt, e, et, f, ft ) \
	void name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft );\
	ALIGN(8) struct sgp_struct_##name { i8_t sgp_name[RPC_NAME_MAX_LENGTH]; u32_t nId; a _a; b _b; c _c; d _d; e _e; f _f; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, i8_t* data, i32_t len, const ZEndpoint& ztp ) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( gn->priv_getUserNode(), t->_a, t->_b, t->_c, t->_d, t->_e, t->_f ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, u8_t channel=1 )\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; \
		gn->beginVariableGroup((const i8_t*)&t, sizeof(t), channel); \
		name( gn, at, bt, ct, dt, et, ft ); \
		gn->endVariableGroup(); \
	}\
	void name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft )


#define DECL_VAR_GROUP_7( name, _zn, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht ) \
	void name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht );\
	ALIGN(8) struct sgp_struct_##name { i8_t sgp_name[RPC_NAME_MAX_LENGTH]; u32_t nId; a _a; b _b; c _c; d _d; e _e; f _f; h _h; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, i8_t* data, i32_t len, const ZEndpoint& ztp) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( gn->priv_getUserNode(), t->_a, t->_b, t->_c, t->_d, t->_e, t->_f, t->_h ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, u8_t channel=1)\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; t._h = ht; \
		gn->beginVariableGroup((const i8_t*)&t, sizeof(t), channel); \
		name( gn, at, bt, ct, dt, et, ft, ht ); \
		gn->endVariableGroup(); \
	}\
	void name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht )


#define DECL_VAR_GROUP_8( name, _zn, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht, i, it ) \
	void name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it );\
	ALIGN(8) struct sgp_struct_##name { i8_t sgp_name[RPC_NAME_MAX_LENGTH]; u32_t nId; a _a; b _b; c _c; d _d; e _e; f _f; h _h; i _i; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, i8_t* data, i32_t len, const ZEndpoint& ztp ) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( gn->priv_getUserNode(), t->_a, t->_b, t->_c, t->_d, t->_e, t->_f, t->_h, t->_i ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, u8_t channel=1 )\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; t._h = ht; t._i = it; \
		gn->beginVariableGroup((const i8_t*)&t, sizeof(t), channel); \
		name( gn, at, bt, ct, dt, et, ft, ht, it ); \
		gn->endVariableGroup(); \
	}\
	void name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it )


#define DECL_VAR_GROUP_9( name, _zn, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht, i, it, j, jt ) \
	void name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt );\
	ALIGN(8) struct sgp_struct_##name { i8_t sgp_name[RPC_NAME_MAX_LENGTH]; u32_t nId; a _a; b _b; c _c; d _d; e _e; f _f; h _h; i _i; j _j; };\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNodePrivate* gn, i8_t* data, i32_t len, const ZEndpoint& ztp ) \
		{ \
			sgp_struct_##name *t = reinterpret_cast<sgp_struct_##name*>(data); \
			assert( len == sizeof(sgp_struct_##name) && "invalid size" ); \
			memcpy(t, data, len); \
			gn->priv_beginVarialbeGroupRemote(t->nId, ztp); \
			name( gn->priv_getUserNode(), t->_a, t->_b, t->_c, t->_d, t->_e, t->_f, t->_h, t->_i, t->_j ); \
			gn->priv_endVariableGroup(); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt, u8_t channel=1 )\
	{\
		sgp_struct_##name t;\
		::sprintf_s( t.sgp_name, RPC_NAME_MAX_LENGTH, "%s", #name ); \
		t._a = at; t._b = bt; t._c = ct; t._d = dt; t._e = et; t._f = ft; t._h = ht; t._i = it; t._j = jt; \
		gn->beginVariableGroup((const i8_t*)&t, sizeof(t), channel); \
		name( gn, at, bt, ct, dt, et, ft, ht, it, jt ); \
		gn->endVariableGroup(); \
	}\
	void name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt )
