#include "Util.h"
#include "Platform.h"

#include <cassert>


namespace Zerodelay
{

	ZEndpoint Util::toZpt(const EndPoint& r)
	{
		static_assert( sizeof(EndPoint) <= sizeof(ZEndpoint), "Zendpoint size is too small" );
		ZEndpoint z;
		memcpy( &z, &r, sizeof(EndPoint) ); 
		return z;

	}

	EndPoint Util::toEtp(const ZEndpoint& z)
	{
		static_assert( sizeof(EndPoint) <= sizeof(ZEndpoint), "ZEndpoint size to small" );
		EndPoint r;
		memcpy( &r, &z, sizeof(EndPoint) ); 
		return r;
	}

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
			return k+1;
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

	u32_t Util::htonl(u32_t val)
	{
	#if ZERODELAY_LIL_ENDIAN
		return ((val & 255)<<24) | (val>>24) | ((val & 0xFF0000)>>8) | ((val & 0xFF00)<<8);
	#else
		return val;
	#endif
	}

	u16_t Util::htons(u16_t val)
	{
	#if ZERODELAY_LIL_ENDIAN
		return ((val & 255)<<8) | (val>>8);
	#else
		return val;
	#endif
	}

	i32_t Util::timeNow()
	{
		clock_t now = ::clock();
		return now;
	}

	i32_t Util::getTimeSince(i32_t timestamp)
	{
		clock_t now = ::clock();
		float elapsedSeconds = float(now - timestamp) / (float)CLOCKS_PER_SEC;
		return i32_t(elapsedSeconds * 1000.f);
	}


	bool Util::deserializeMap(std::map<std::string, std::string>& data, const i8_t* payload, i32_t payloadLen)
	{
		i32_t readBytes = 0;
		const i32_t kBuffSize = ZERODELAY_BUFF_RECV_SIZE;
		while (payloadLen)
		{
			// read key
			i8_t key[2][kBuffSize];
			for (auto k : key)
			{
				i32_t res = Util::readString( k, kBuffSize, payload + readBytes, payloadLen );
				if (res < 0 )
				{
					return false;
				}
				readBytes  += res;
				payloadLen -= res;
				if ( payloadLen < 0 ) return false;
			}
			data.insert(std::make_pair(key[0], key[1]));
		}
		return true;
	}

	void Util::addTraceCallResult(std::vector<ZAckTicket>* deliveryTraceOut, const EndPoint& etp,
								  ETraceCallResult trCallRes, u32_t sequence, u32_t numFragments, i8_t channel)
	{
		if ( !deliveryTraceOut ) return;
		ZAckTicket t;
		t.endpoint = toZpt(etp);
		t.traceCallResult = trCallRes;
		t.sequence = sequence;
		t.numFragments = numFragments;
		t.channel  = channel;
		deliveryTraceOut->emplace_back(t);
	}

}