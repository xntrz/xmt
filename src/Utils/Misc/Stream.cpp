#include "Stream.hpp"


namespace STREAM
{
	inline int ADJUST_ALLOC_SIZE(int size)
	{
		static const unsigned long ALLOC_RESERVE = 1024;
		return (size + (ALLOC_RESERVE - 1)) & ~(ALLOC_RESERVE - 1);
	};

	static const unsigned long FLAG_NONE = 0x0;
	static const unsigned long FLAG_READ = 0x1;
	static const unsigned long FLAG_WRITE = 0x2;
	static const unsigned long FLAG_ATTACH = 0x4;
	static const unsigned long FLAG_ALLOC = 0x8;
	static const unsigned long FLAG_PEEK = 0x10;
};


CStream::CStream(void)
: m_pBuffer(nullptr)
, m_sizeBuffer(0)
, m_rwPos(0)
, m_flags(STREAM::FLAG_NONE)
{
	;
};


CStream::CStream(int sizeBuffer)
: m_pBuffer(nullptr)
, m_sizeBuffer(0)
, m_rwPos(0)
, m_flags(STREAM::FLAG_NONE)
{
	reallocBuffer(sizeBuffer);
};


CStream::CStream(char* buffer, int sizeBuffer)
: m_pBuffer(buffer)
, m_sizeBuffer(sizeBuffer)
, m_rwPos(0)
, m_flags(STREAM::FLAG_NONE)
{
	setFlag(STREAM::FLAG_ATTACH, true);
};


CStream::~CStream(void)
{
	freeBuffer();
};


void CStream::clear(void)
{
	freeBuffer();
	reset();
};


void CStream::reset(void)
{
	m_rwPos = 0;
};


void CStream::skip(int bytes)
{
	m_rwPos += bytes;
	if (m_rwPos > m_sizeBuffer)
		m_rwPos = m_sizeBuffer;
};


void CStream::write(const char* data, int sizeData)
{
	writeRequest(data, sizeData);
};


void CStream::write_offset(const char* data, int sizeData, int offset)
{
	int rwPos = m_rwPos;
	m_rwPos = offset;
	writeRequest(data, sizeData, offset);
	m_rwPos = rwPos;
};


void CStream::peek(char* buffer, int sizeBuffer)
{
	setFlag(STREAM::FLAG_PEEK, true);
	read(buffer, sizeBuffer);
	setFlag(STREAM::FLAG_PEEK, false);
};


void CStream::read(char* buffer, int sizeBuffer)
{
	readRequest(buffer, sizeBuffer);
};


void CStream::read_offset(char* buffer, int sizeBuffer, int offset)
{
	int rwPos = m_rwPos;
	m_rwPos = offset;
	readRequest(buffer, sizeBuffer, offset);
	m_rwPos = rwPos;
};


char* CStream::data(void) const
{
	return m_pBuffer;
};


int CStream::size(void) const
{
	return m_rwPos;
};


int CStream::capacity(void) const
{
	return m_sizeBuffer;
};


void CStream::setFlag(unsigned long flag, bool bSet)
{
	if (bSet)
		m_flags |= flag;
	else
		m_flags &= ~flag;
};


bool CStream::isFlagSet(unsigned long flag) const
{
	return (m_flags & flag) == flag;
};


void CStream::writeRequest(const void* data, int dataSize)
{
	writeRequest(data, dataSize, m_rwPos);
};


void CStream::writeRequest(const void* data, int dataSize, int offset)
{
	setFlag(STREAM::FLAG_WRITE, true);
	readwrite_subroutine(&m_pBuffer[offset], (void*)data, dataSize);
	setFlag(STREAM::FLAG_WRITE, false);
};


void CStream::readRequest(void* buffer, int bufferSize)
{
	readRequest(buffer, bufferSize, m_rwPos);
};


void CStream::readRequest(void* buffer, int bufferSize, int offset)
{
	if (m_rwPos + bufferSize > m_sizeBuffer)
		bufferSize = m_sizeBuffer - m_rwPos;

	setFlag(STREAM::FLAG_READ, true);
	readwrite_subroutine(buffer, &m_pBuffer[offset], bufferSize);
	setFlag(STREAM::FLAG_READ, false);
};


bool CStream::readwrite_subroutine(void* dst, void* src, int size)
{
	bool bResult = false;

	do
	{
		if (isFlagSet(STREAM::FLAG_READ) &&
			isFlagSet(STREAM::FLAG_PEEK))
			bResult = true;
		else
			bResult = (m_rwPos + size) <= m_sizeBuffer;

		if (bResult)
		{
			std::memcpy(dst, src, size);

			if (!isFlagSet(STREAM::FLAG_PEEK))
				m_rwPos += size;

			break;
		}
		else
		{
			if (isFlagSet(STREAM::FLAG_WRITE))
			{
				int reallocSize = calcReallocSize(size);
				if (reallocBuffer(reallocSize))
				{
					dst = &m_pBuffer[m_rwPos];
					continue;
				};
			};

			break;
		};
	} while (true);

	return bResult;
};


bool CStream::reallocBuffer(int reallocSize)
{
	if (isFlagSet(STREAM::FLAG_ATTACH))
		return false;

	char* pNewBuffer = new char[reallocSize];
	if (!pNewBuffer)
		return false;

	if (isFlagSet(STREAM::FLAG_ALLOC))
	{
		std::memcpy(pNewBuffer, m_pBuffer, m_rwPos);

		delete[] m_pBuffer;
		m_pBuffer = pNewBuffer;
		m_sizeBuffer = reallocSize;
	}
	else
	{
		m_pBuffer = pNewBuffer;
		m_sizeBuffer = reallocSize;
		setFlag(STREAM::FLAG_ALLOC, true);
	};

	return true;
};


void CStream::freeBuffer(void)
{
	if (m_pBuffer)
	{
		if (isFlagSet(STREAM::FLAG_ALLOC))
			delete[] m_pBuffer;
		m_pBuffer = nullptr;
	};

	m_sizeBuffer = 0;
};


int CStream::calcReallocSize(int size) const
{
	return STREAM::ADJUST_ALLOC_SIZE(m_sizeBuffer + size);
};