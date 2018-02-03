#pragma once

#include "Zerodelay.h"


namespace Zerodelay
{
	struct Util
	{
		static i8_t* appendString2(i8_t* dst, i32_t& dstSize, const i8_t* src, bool& bSucces);
		static i8_t* appendString( i8_t* dst, i32_t dstSize, const i8_t* src );

		// NOTE, returns the amount of read bytes (includes the zero terminating char!)
		// So if str is: "lala", it returns 5.
		// If error occurs, returns -1.
		static i32_t readString( i8_t* dst, i32_t dstSize, const i8_t* buffIn, i32_t buffInSize );


		static bool  readFixed( i8_t* dst, i32_t dstSize, const i8_t* buffIn, i32_t buffInSize );
		static u32_t hton( u32_t val );
		static u16_t hton( u16_t val );
		static i32_t getTimeSince(i32_t timestamp);  // in milliseconds

		static bool deserializeMap( std::map<std::string, std::string>& data, const i8_t* source, i32_t payloadLenIn );

		template <typename List, typename Callback>
		static void bindCallback( List& list, const Callback& cb );
		template <typename List, typename Callback>
		static void forEachCallback( const List& list, const Callback& cb );
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