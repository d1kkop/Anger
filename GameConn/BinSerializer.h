#pragma once

#include "Zerodelay.h"
#include "Platform.h"
#include "EndPoint.h"
#include "Util.h"


#define __CHECKED( expr ) if (!(expr)) { m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE); return; }
#define __CHECKEDR( expr ) if (!(expr)) { m_RecvNode->getCoreNode()->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE); return; }
#define __CHECKEDNS( expr ) if (!(expr)) { m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE); return ESendCallResult::NotSent; }
#define __CHECKEDMSE( expr ) if (!(expr)) { m_CoreNode->setCriticalError(ECriticalError::SerializationError, ZERODELAY_FUNCTION_LINE); return ERegisterServerCallResult::SerializationError; }


namespace Zerodelay
{
	class ZDLL_DECLSPEC BinSerializer
	{
	public:
		BinSerializer();
		~BinSerializer();

		void reset();
		void resetTo(const i8_t* streamIn, i32_t buffSize, i32_t writePos);

		bool setRead(i32_t r);
		bool setWrite(i32_t w);
		bool moveRead(i32_t r);
		bool moveWrite(i32_t w);
		i32_t getRead() const  { return m_ReadPos; }
		i32_t getWrite() const { return m_WritePos; }
		i32_t length() const { return getWrite(); }

		bool write8(i8_t b);
		bool write16(i16_t b);
		bool write32(i32_t b);

		bool read8(i8_t& b);
		bool read16(i16_t& b);
		bool read32(i32_t& b);

		template <typename T> 
		bool write(const T& val) { static_assert(false, "Invalid parameter for T"); return false; }

		template <typename T>
		bool read(T& val) { static_assert(false, "Invalid parameter for T"); return false; }

		template <> bool write(const bool& b)  { return write8(b); }
		template <> bool write(const i8_t& b)  { return write8(b); }
		template <> bool write(const i16_t& b) { return write16(b); }
		template <> bool write(const i32_t& b) { return write32(b); }
		template <> bool write(const u8_t& b)  { return write8((const i8_t&)b); }
		template <> bool write(const u16_t& b) { return write16((const i16_t&)b); }
		template <> bool write(const u32_t& b) { return write32((const i32_t&)b); }
		template <> bool write(const EndPoint& b) 
		{
			i32_t kWrite = b.write(pr_data()+m_WritePos, m_MaxSize-m_WritePos);
			if (kWrite < 0) return false;
			return moveWrite(kWrite);
		}
		template <> bool write(const ZEndpoint& b)
		{
			EndPoint etp = Util::toEtp(b);
			return write(etp);
		}
		template <> bool write(const std::string& b)
		{
			if ( b.length() > UINT16_MAX ) return false;
			if ( !write<u16_t>((u16_t)b.size()) ) return false;
			return write(b.c_str(), (i32_t)b.length());
		}
		template <> bool write(const std::map<std::string, std::string>& b)
		{
			if ( b.size() > UINT16_MAX ) return false;
			if ( !write<u16_t>((u16_t)b.size()) ) return false;
			for ( auto& kvp : b ) 
			{
				if ( !write(kvp.first) ) return false;
				if ( !write(kvp.second) ) return false;
			}
			return true;
		}

		template <> bool read(bool& b)  { return read8((i8_t&)b); }
		template <> bool read(i8_t& b)  { return read8(b); }
		template <> bool read(i16_t& b) { return read16(b); }
		template <> bool read(i32_t& b) { return read32(b); }
		template <> bool read(u8_t& b)  { return read8((i8_t&)b); }
		template <> bool read(u16_t& b) { return read16((i16_t&)b); }
		template <> bool read(u32_t& b) { return read32((i32_t&)b); }
		template <> bool read(EndPoint& b) 
		{
			i32_t kRead = b.read(pr_data()+m_ReadPos, m_WritePos-m_ReadPos);
			if (kRead < 0) return false;
			return moveRead(kRead);
		}
		template <> bool read(ZEndpoint& b) 
		{
			EndPoint etp;
			if (read(etp)) 
			{
				b = Util::toZpt(etp);
				return true;
			}
			return false;
		}
		template <> bool read(std::string& b) 
		{
			u16_t slen;
			if (!read<u16_t>(slen)) return false;
			if ( slen + m_ReadPos > m_WritePos ) return false;
			b.resize(slen);
			bool bRes = Platform::memCpy((void*)b.data(), slen, data()+m_ReadPos, slen);
			return bRes && moveRead(slen);
		}
		template <> bool read(std::map<std::string, std::string>& b) 
		{
			u16_t mlen;
			if (!read<u16_t>(mlen)) return false;
			std::string key, value;
			for (u16_t i=0; i<mlen; i++)
			{
				if ( !read(key) ) return false;
				if ( !read(value) ) return false;
				b.insert(std::make_pair(key, value));
			}
			return true;
		}

		bool read(i8_t* b, u16_t buffSize, u16_t& len)
		{
			if ( m_ReadPos + buffSize > m_MaxSize ) return false;
			if ( !read(len) ) return false;
			if ( len > buffSize ) return false;
			if ( !Platform::memCpy(b, buffSize, data()+m_ReadPos, len) ) return false;
			return moveRead(len);
		}

		bool write(const i8_t* b, u16_t buffSize)
		{
			growTo(m_WritePos + buffSize);
			if ( m_WritePos + buffSize > m_MaxSize ) return false;
			if ( buffSize > UINT16_MAX ) return false;
			if ( !write(buffSize) ) return false;
			bool bRes = Platform::memCpy((void*)(data()+m_WritePos), m_MaxSize-buffSize, b, buffSize);
			return bRes && moveWrite(buffSize);
		}

		void copyAsRaw(i8_t*& ptr, i32_t& len);

		i32_t getMaxSize() const;
		const i8_t* data() const { return (pr_data()); }

	private:
		i8_t* pr_data() const;
		void growTo(i32_t maxSize);

		i8_t* m_DataPtr, * m_OrigPtr;
		i32_t m_WritePos;
		i32_t m_ReadPos;
		i32_t m_MaxSize, m_OrigSize;
		bool  m_Owns;
	};



}