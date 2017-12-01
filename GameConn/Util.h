#pragma once

#include "Zerodelay.h"


namespace Zerodelay
{
	struct Util
	{
		static i8_t* appendString( i8_t* dst, i32_t dstSize, const i8_t* src );
		static i32_t readString( i8_t* dst, i32_t dstSize, const i8_t* buffIn, i32_t buffInSize );
		static bool  readFixed( i8_t* dst, i32_t dstSize, const i8_t* buffIn, i32_t buffInSize );
		static u32_t swap32( u32_t val );
		static u16_t swap16( u16_t val );
		static i32_t getTimeSince(i32_t timestamp);  // in milliseconds

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