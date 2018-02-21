#include "BinSerializer.h"
#include "Util.h"
#include <cassert>


namespace Zerodelay
{
	BinSerializer::BinSerializer():
		m_Owns(false),
		m_OrigPtr(nullptr),
		m_OrigSize(0)
	{
		reset();
	}

	BinSerializer::~BinSerializer()
	{
		reset();
		free(m_DataPtr);
	}

	void BinSerializer::reset()
	{
		if (!m_Owns) 
		{
			m_MaxSize = m_OrigSize;
			m_DataPtr = m_OrigPtr;
		}
		m_WritePos = 0;
		m_ReadPos  = 0;
		m_Owns = true;
	}

	void BinSerializer::resetTo(const i8_t* streamIn, i32_t buffSize, i32_t writePos)
	{
		if ( m_Owns )
		{
			m_OrigPtr  = m_DataPtr;
			m_OrigSize = m_MaxSize;
		}
		m_DataPtr  = (i8_t*) streamIn;
		m_WritePos = writePos;
		m_MaxSize = buffSize;
		m_ReadPos = 0;
		m_Owns = false;
		assert(m_WritePos>=0 && m_WritePos<=m_MaxSize && m_DataPtr!=nullptr);
	}

	bool BinSerializer::setRead(i32_t r)
	{
		if ( r < 0 || r > m_WritePos ) return false;
		m_ReadPos = r;
		return true;
	}

	bool BinSerializer::setWrite(i32_t w)
	{
		if ( w < 0 || w > m_MaxSize ) return false;
		m_WritePos = w;
		return true;
	}

	bool BinSerializer::moveRead(i32_t r)
	{
		return setRead( m_ReadPos + r );
	}

	bool BinSerializer::moveWrite(i32_t w)
	{
		return setWrite( m_WritePos + w );
	}

	bool BinSerializer::write8(i8_t b)
	{
		growTo(m_WritePos + 1);
		if ( m_WritePos + 1 > m_MaxSize ) return false;
		pr_data()[m_WritePos++] = b;
		return true;
	}

	bool BinSerializer::write16(i16_t b)
	{
		growTo(m_WritePos + 2);
		if ( m_WritePos + 2 > m_MaxSize ) return false;
		*(u16_t*)(pr_data() + m_WritePos) = Util::htons(b);
		m_WritePos += 2;
		return true;
	}

	bool BinSerializer::write32(i32_t b)
	{
		growTo(m_WritePos + 4);
		if ( m_WritePos + 4 > m_MaxSize ) return false;
		*(u32_t*)(pr_data() + m_WritePos) = Util::htonl(b);
		m_WritePos += 4;
		return true;
	}

	bool BinSerializer::read8(i8_t& b)
	{
		if ( m_ReadPos + 1 > m_WritePos ) return false;
		b = pr_data()[m_ReadPos++];
		return true;
	}

	bool BinSerializer::read16(i16_t& b)
	{
		if ( m_ReadPos + 2 > m_WritePos ) return false;
		b = Util::ntohs(*(u16_t*)(pr_data() + m_ReadPos));
		m_ReadPos += 2;
		return true;
	}

	bool BinSerializer::read32(i32_t& b)
	{
		if ( m_ReadPos + 4 > m_WritePos ) return false;
		b = Util::ntohl(*(u32_t*)(pr_data() + m_ReadPos));
		m_ReadPos += 4;
		return true;
	}

	void BinSerializer::copyAsRaw(i8_t*& ptr, i32_t& len)
	{
		len = m_WritePos;
		ptr = new i8_t[len];
		Platform::memCpy(ptr, len, m_DataPtr, len);
	}

	i32_t BinSerializer::getMaxSize() const
	{
		return m_MaxSize;
	}

	i8_t* BinSerializer::pr_data() const
	{
		return m_DataPtr;
	}

	void BinSerializer::growTo(i32_t maxSize)
	{
		if (!m_Owns) return;
		assert(maxSize > m_MaxSize && maxSize > 0);
		m_MaxSize = i32_t(maxSize * 1.2f);
		assert(m_MaxSize > maxSize);
		i8_t* pNew = (i8_t*)malloc(m_MaxSize);
		Platform::memCpy(pNew, m_MaxSize, m_DataPtr, maxSize);
		free(m_DataPtr);
		m_DataPtr = pNew;		
	}

}