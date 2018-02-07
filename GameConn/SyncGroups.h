#pragma once

#include <cassert>
#include "RpcMacros.h"


#define DECL_VAR_GROUP_0( name, _zn) \
	void __sgp_call_local_##name( ZNode* _zn );\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNode* gn, const i8_t* data, i32_t len ) \
		{ \
			__sgp_call_local_##name( gn ); \
		}\
	}\
	void create_##name( ZNode* gn )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		gn->deferredCreateVariableGroup(data, (i32_t)(p-data)); \
	}\
	void __sgp_call_local_##name( ZNode* _zn )


#define DECL_VAR_GROUP_1( name, _zn, a, at) \
	void __sgp_call_local_##name( ZNode* _zn, a at );\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNode* gn, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			__sgp_call_local_##name( gn, at ); \
		}\
	}\
	void create_##name( ZNode* gn, a at )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		gn->deferredCreateVariableGroup(data, (i32_t)(p-data)); \
	}\
	void __sgp_call_local_##name( ZNode* _zn, a at )


#define DECL_VAR_GROUP_2( name, _zn, a, at, b, bt ) \
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt );\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNode* gn, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			__sgp_call_local_##name( gn, at, bt ); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		gn->deferredCreateVariableGroup(data, (i32_t)(p-data)); \
	}\
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt )


#define DECL_VAR_GROUP_3( name, _zn, a, at, b, bt, c, ct ) \
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct );\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNode* gn, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			__sgp_call_local_##name( gn, at, bt, ct ); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		__sr::write(p, ct); \
		gn->deferredCreateVariableGroup(data, (i32_t)(p-data)); \
	}\
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct )


#define DECL_VAR_GROUP_4( name, _zn, a, at, b, bt, c, ct, d, dt ) \
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt );\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNode* gn, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			__sgp_call_local_##name( gn, at, bt, ct, dt ); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		__sr::write(p, ct); \
		__sr::write(p, dt); \
		gn->deferredCreateVariableGroup(data, (i32_t)(p-data)); \
	}\
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt )


#define DECL_VAR_GROUP_5( name, _zn, a, at, b, bt, c, ct, d, dt, e, et ) \
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt, e et );\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNode* gn, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			e et; __sr::read(data, et); \
			__sgp_call_local_##name( gn, at, bt, ct, dt, et ); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, e et )\
	{\
		i8_t data[RPC_DATA_MAX]; \
		i8_t* p=data;\
		__sr::writeStr(p, #name); \
		__sr::write(p, at); \
		__sr::write(p, bt); \
		__sr::write(p, ct); \
		__sr::write(p, dt); \
		__sr::write(p, et); \
		gn->deferredCreateVariableGroup(data, (i32_t)(p-data)); \
	}\
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt, e et )


#define DECL_VAR_GROUP_6( name, _zn, a, at, b, bt, c, ct, d, dt, e, et, f, ft ) \
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft );\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNode* gn, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			e et; __sr::read(data, et); \
			f ft; __sr::read(data, ft); \
			__sgp_call_local_##name( gn, at, bt, ct, dt, et, ft ); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft )\
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
		gn->deferredCreateVariableGroup(data, (i32_t)(p-data)); \
	}\
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft )


#define DECL_VAR_GROUP_7( name, _zn, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht ) \
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht );\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNode* gn, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			e et; __sr::read(data, et); \
			f ft; __sr::read(data, ft); \
			h ht; __sr::read(data, ht); \
			__sgp_call_local_##name( gn, at, bt, ct, dt, et, ft, ht ); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht)\
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
		gn->deferredCreateVariableGroup(data, (i32_t)(p-data)); \
	}\
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht )


#define DECL_VAR_GROUP_8( name, _zn, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht, i, it ) \
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it );\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNode* gn, const i8_t* data, i32_t len ) \
		{ \
			a at; __sr::read(data, at); \
			b bt; __sr::read(data, bt); \
			c ct; __sr::read(data, ct); \
			d dt; __sr::read(data, dt); \
			e et; __sr::read(data, et); \
			f ft; __sr::read(data, ft); \
			h ht; __sr::read(data, ht); \
			i it; __sr::read(data, it); \
			__sgp_call_local_##name( gn, at, bt, ct, dt, et, ft, ht, it ); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it )\
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
		gn->deferredCreateVariableGroup(data, (i32_t)(p-data)); \
	}\
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it )


#define DECL_VAR_GROUP_9( name, _zn, a, at, b, bt, c, ct, d, dt, e, et, f, ft, h, ht, i, it, j, jt ) \
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt );\
	extern "C" {\
		RPC_EXPORT void __sgp_deserialize_##name( ZNode* gn, const i8_t* data, i32_t len ) \
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
			__sgp_call_local_##name( gn, at, bt, ct, dt, et, ft, ht, it, jt ); \
		}\
	}\
	void create_##name( ZNode* gn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt )\
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
		gn->deferredCreateVariableGroup(data, (i32_t)(p-data)); \
	}\
	void __sgp_call_local_##name( ZNode* _zn, a at, b bt, c ct, d dt, e et, f ft, h ht, i it, j jt )
