#include "Util.h"
#include "Platform.h"

namespace Zerodelay
{

	i8_t* Util::appendString(i8_t* dst, i32_t dstSize, const i8_t* src)
	{
		i32_t k = 0;
		for (; src[k]!='\0' && k<(dstSize-1); ++k) dst[k] = src[k];
		dst[k]='\0';
		return dst+k;
	}

	bool Util::readString(i8_t* buff, i32_t buffSize, const i8_t* buffIn, i32_t buffInSize)
	{
		if ( !buff || !buffIn )
			return false;
		i32_t k = 0;
		while ((*buffIn != '\0') && (k < buffSize-1) && (k < buffInSize))
		{
			*buff++ = *buffIn++;
			++k;
		}
		if ( buffSize > 0 )
		{
			*buff = '\0';
			return true;
		}
		return false;
	}

	bool Util::readFixed(i8_t* dst, i32_t dstSize, const i8_t* buffIn, i32_t buffInSize)
	{
		if ( !dst || !buffIn || buffInSize < dstSize )
			return false;
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

}