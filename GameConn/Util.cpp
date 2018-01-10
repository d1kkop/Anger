#include "Util.h"
#include "Platform.h"

#include <cassert>

namespace Zerodelay
{

	i8_t* Util::appendString2(i8_t* dst, i32_t& dstSize, const i8_t* src, bool& bSucces)
	{
		bSucces = true;
		i32_t k = 0;
		for (; src[k]!='\0'; ++k) 
		{
			if ( k>=(dstSize-1) )
			{
				bSucces = false;
				break;
			}
			dst[k] = src[k];
		}
		if (dstSize > 0) dst[k++]='\0';
		dstSize -= k;
		return dst+k;
	}

	i8_t* Util::appendString(i8_t* dst, i32_t dstSize, const i8_t* src)
	{
		i32_t k = 0;
		for (; src[k]!='\0' && k<(dstSize-1); ++k) dst[k] = src[k];
		dst[k]='\0';
		return dst+k;
	}

	i32_t Util::readString(i8_t* buff, i32_t buffSize, const i8_t* buffIn, i32_t buffInSize)
	{
		if ( !buff || !buffIn )
		{
			assert(false);
			return -1;
		}
		i32_t k = 0;
		while ((*buffIn != '\0') && (k < buffSize-1) && (k < buffInSize))
		{
			*buff++ = *buffIn++;
			++k;
		}
		if ( buffSize > 0 )
		{
			*buff = '\0';
			if ( (*buff=='\0') )
				return k;
		}
		assert(false);
		return -1;
	}

	bool Util::readFixed(i8_t* dst, i32_t dstSize, const i8_t* buffIn, i32_t buffInSize)
	{
		if ( !dst || !buffIn || buffInSize < dstSize )
		{
			assert(false);
			return false;
		}
		memcpy( dst, buffIn, dstSize );
		return true;
	}	

	u32_t Util::swap32(u32_t val)
	{
	#if ZERODELAY_LIL_ENDIAN
		return ((val & 255)<<24) | (val>>24) | ((val & 0xFF0000)>>8) | ((val & 0xFF00)<<8);
	#else
		return val;
	#endif
	}

	u16_t Util::swap16(u16_t val)
	{
	#if ZERODELAY_LIL_ENDIAN
		return ((val & 255)<<8) | (val>>8);
	#else
		return val;
	#endif
	}


	i32_t Util::getTimeSince(i32_t timestamp)
	{
		clock_t now = ::clock();
		float elapsedSeconds = float(now - timestamp) / (float)CLOCKS_PER_SEC;
		return i32_t(elapsedSeconds * 1000.f);
	}
}