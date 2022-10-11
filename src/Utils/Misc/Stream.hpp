#pragma once


class CStream
{
public:
	CStream(void);
	CStream(int sizeBuffer);
	CStream(char* buffer, int sizeBuffer);
	~CStream(void);
	void clear(void);
	void reset(void);
	void skip(int bytes);
	void write(const char* data, int sizeData);
	void write_offset(const char* data, int sizeData, int offset);
	void peek(char* buffer, int sizeBuffer);
	void read(char* buffer, int sizeBuffer);
	void read_offset(char* buffer, int sizeBuffer, int offset);
	char* data(void) const;
	int size(void) const;
	int capacity(void) const;

	template<class TData>
	void write(TData data);

	template<class TData>
	void write_offset(TData data, int offset);

	template<class TData>
	void peek(TData data);

	template<class TData>
	void read(TData data);

	template<class TData>
	void read_offset(TData data, int offset);

private:
	void setFlag(unsigned long flag, bool bSet);
	bool isFlagSet(unsigned long flag) const;
	void writeRequest(const void* data, int dataSize);
	void writeRequest(const void* data, int dataSize, int offset);
	void readRequest(void* buffer, int bufferSize);
	void readRequest(void* buffer, int bufferSize, int offset);
	bool readwrite_subroutine(void* dst, void* src, int size);
	bool reallocBuffer(int reallocSize);
	void freeBuffer(void);
	int calcReallocSize(int size) const;

protected:
	char* m_pBuffer;
	int m_rwPos;
	int m_sizeBuffer;
	unsigned long m_flags;
};


template<class TData>
void CStream::write(TData data)
{
	write((char*)&data, sizeof(TData));
};


template<class TData>
void CStream::write_offset(TData data, int offset)
{
	write_offset((char*)&data, sizeof(TData), offset);
};


template<class TData>
void CStream::peek(TData data)
{
	peek((void*)data, sizeof(*data));
};


template<class TData>
void CStream::read(TData data)
{
	read((char*)data, sizeof(*data));
};


template<class TData>
void CStream::read_offset(TData data, int offset)
{
	read_offset((char*)data, sizeof(*data), offset);
};