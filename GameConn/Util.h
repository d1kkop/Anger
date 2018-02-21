#pragma once

#include "Zerodelay.h"
#include "EndPoint.h"


namespace Zerodelay
{
	struct Util
	{
		template <typename T>
		static T min(T a, T b) { return a < b ? a : b; }

		template <typename T>
		static T max(T a, T b) { return a > b ? a : b; }

		static ZEndpoint toZpt( const EndPoint& r );
		static EndPoint toEtp( const ZEndpoint& z );

		static i8_t* appendString2(i8_t* dst, i32_t& dstSize, const i8_t* src, bool& bSucces);
		static i8_t* appendString( i8_t* dst, i32_t dstSize, const i8_t* src );

		// NOTE, returns the amount of read bytes (includes the zero terminating char!)
		// So if str is: "lala", it returns 5.
		// If error occurs, returns -1.
		static i32_t readString( i8_t* dst, i32_t dstSize, const i8_t* buffIn, i32_t buffInSize );


		static bool  readFixed( i8_t* dst, i32_t dstSize, const i8_t* buffIn, i32_t buffInSize );
		static u32_t htonl( u32_t val );
		static u16_t htons( u16_t val );
		static u32_t ntohl( u32_t val ) { return htonl(val); }
		static u16_t ntohs( u16_t val ) { return htons(val); }
		static i32_t timeNow();
		static i32_t getTimeSince(i32_t timestamp);  // in milliseconds

		static bool deserializeMap( std::map<std::string, std::string>& data, const i8_t* source, i32_t payloadLenIn );

		static void addTraceCallResult( std::vector<ZAckTicket>* deliveryTraceOut, const EndPoint& etp, ETraceCallResult, u32_t sequence, u32_t numFragments,  i8_t channel );

		template <typename List, typename Callback>
		static void bindCallback( List& list, const Callback& cb );
		template <typename List, typename Callback>
		static void forEachCallback( const List& list, const Callback& cb );

		static void write8(i8_t* buff, i8_t b)				{ *buff = b; }
		static void write16(i8_t* buff, i16_t b)			{ *(i16_t*)(buff) = htons(b); }
		static void write32(i8_t* buff, i32_t b)			{ *(i32_t*)(buff) = htonl(b); }
		static void read8(const i8_t* buff, i8_t& b)		{ b = *buff; }
		static void read16(const i8_t* buff, i16_t& b)		{ b = ntohs( *(i16_t*)buff ); }
		static void read32(const i8_t* buff, i32_t& b)		{ b = ntohl( *(i32_t*)buff ); }

		template <typename T> 
		static void write(i8_t* buff, const T& val) { static_assert(false, "Invalid parameter for T"); return false; }
		template <typename T>
		static void read(const i8_t* buff, T& val) { static_assert(false, "Invalid parameter for T"); return false; }

		template <> static void write(i8_t* buff, const bool& b)  { write8(buff, (i8_t&)b); }
		template <> static void write(i8_t* buff, const i8_t& b)  { write8(buff, b); }
		template <> static void write(i8_t* buff, const i16_t& b) { write16(buff, b); }
		template <> static void write(i8_t* buff, const i32_t& b) { write32(buff, b); }
		template <> static void write(i8_t* buff, const u8_t& b)  { write8(buff, (const i8_t&)b); }
		template <> static void write(i8_t* buff, const u16_t& b) { write16(buff, (const i16_t&)b); }
		template <> static void write(i8_t* buff, const u32_t& b) { write32(buff, (const i32_t&)b); }
					 
		template <> static void read(const i8_t* buff, bool& b)  { read8(buff, (i8_t&)b); }
		template <> static void read(const i8_t* buff, i8_t& b)  { read8(buff, b); }
		template <> static void read(const i8_t* buff, i16_t& b) { read16(buff, b); }
		template <> static void read(const i8_t* buff, i32_t& b) { read32(buff, b); }
		template <> static void read(const i8_t* buff, u8_t& b)  { read8(buff, (i8_t&)b); }
		template <> static void read(const i8_t* buff, u16_t& b) { read16(buff, (i16_t&)b); }
		template <> static void read(const i8_t* buff, u32_t& b) { read32(buff, (i32_t&)b); }
	};


	template <typename List, typename Callback>
	void Util::bindCallback(List& list, const Callback& cb)
	{
		list.emplace_back( cb );
	}

	template <typename List, typename Callback>
	void Util::forEachCallback(const List& list, const Callback& cb)
	{
		for (auto& fcb : list)
		{
			cb(fcb);
		}
	}
}