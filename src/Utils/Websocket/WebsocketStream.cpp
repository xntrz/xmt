#include "WebsocketStream.hpp"

#include "Shared/Common/Random.hpp"

#include "zlib.h"


static inline void WsMaskUnmask(
	unsigned char*			Buffer,
	const unsigned char* 	UnMaskedBuffer,
	int 					Length,
	char 					Mask[4]
)
{
	for (int i = 0; i < Length; ++i)
		Buffer[i] = UnMaskedBuffer[i] ^ Mask[i % 4];
};


class CWebsocketStream::CCompressor final
{
private:
	static const int BUFFSIZE = 256 * 128;

public:
	CCompressor(void);
	~CCompressor(void);
	bool Start(int bits);
	void Stop(void);
	std::vector<unsigned char> Deflate(const void* data, int dataSize);
	std::vector<unsigned char> Inflate(const void* data, int dataSize);

private:
	z_stream m_streamInflate;
	z_stream m_streamDeflate;
	bool m_bInitFlag;
};


CWebsocketStream::CCompressor::CCompressor(void)
: m_bInitFlag(false)
{
	;
};


CWebsocketStream::CCompressor::~CCompressor(void)
{
	if (m_bInitFlag)
		Stop();
};


bool CWebsocketStream::CCompressor::Start(int bits)
{
	m_streamDeflate = { 0 };
	m_streamInflate = { 0 };

	int iResult = deflateInit2(&m_streamDeflate, Z_DEFAULT_COMPRESSION, Z_DEFLATED, bits, 8, Z_DEFAULT_STRATEGY);
	if (iResult != Z_OK)
		return false;

	iResult = inflateInit2(&m_streamInflate, bits);
	if (iResult != Z_OK)
	{
		deflateEnd(&m_streamDeflate);
		return false;
	};

	m_bInitFlag = true;

	return true;
};


void CWebsocketStream::CCompressor::Stop(void)
{
	m_bInitFlag = false;

	deflateEnd(&m_streamDeflate);
	inflateEnd(&m_streamInflate);
};


std::vector<unsigned char> CWebsocketStream::CCompressor::Deflate(const void* data, int dataSize)
{
	assert(m_bInitFlag);

	int iResult = Z_OK;
	unsigned char buffer[BUFFSIZE] = { 0 };
	std::vector<unsigned char> output;

	m_streamDeflate.avail_in = dataSize;
	m_streamDeflate.next_in = (Bytef*)data;

	do
	{
		m_streamDeflate.avail_out = sizeof(buffer);
		m_streamDeflate.next_out = buffer;

		iResult = deflate(&m_streamDeflate, Z_SYNC_FLUSH);
		switch (iResult)
		{
		case Z_OK:
			output.insert(
				output.end(),
				buffer,
				buffer + (sizeof(buffer) - m_streamDeflate.avail_out)
			);
			break;

		case Z_STREAM_END:
			iResult = deflateReset(&m_streamDeflate);
			break;

		case Z_BUF_ERROR:
			break;

		case Z_DATA_ERROR:
		default:
			assert(false);
			break;
		};

	} while (iResult == Z_OK);

	if (output.size() >= 4)
		output.erase(output.end() - 4, output.end());

	return output;
};


std::vector<unsigned char> CWebsocketStream::CCompressor::Inflate(const void* data, int dataSize)
{
	assert(m_bInitFlag);

	int iResult = Z_OK;
	unsigned char buffer[BUFFSIZE] = { 0 };
	std::vector<unsigned char> output;
	std::vector<unsigned char> input;

	input.reserve(dataSize + 4);
	input.insert(input.end(), (unsigned char*)data, (unsigned char*)data + dataSize);
	input.insert(input.end(), 0x00);
	input.insert(input.end(), 0x00);
	input.insert(input.end(), 0xFF);
	input.insert(input.end(), 0xFF);

	m_streamInflate.avail_in = input.size();
	m_streamInflate.next_in = &input[0];

	do
	{
		m_streamInflate.avail_out = sizeof(buffer);
		m_streamInflate.next_out = buffer;

		iResult = inflate(&m_streamInflate, Z_SYNC_FLUSH);
		switch (iResult)
		{
		case Z_OK:
			output.insert(
				output.end(),
				buffer,
				buffer + (sizeof(buffer) - m_streamInflate.avail_out)
			);
			break;

		case Z_STREAM_END:
			iResult = inflateReset(&m_streamInflate);
			break;

		case Z_BUF_ERROR:
			break;

		case Z_DATA_ERROR:
			break;

		default:
			assert(false);
			break;
		};
	} while (iResult == Z_OK);

	return output;
};


namespace WSTYPES
{
	struct FRAME_HDR
	{
		unsigned char opcode : 4;
		bool rsv3 : 1;
		bool rsv2 : 1;
		bool rsv1 : 1;	// Per-Message Compressed flag
		bool fin : 1;

		unsigned char length : 7;
		bool mask : 1;
	};


	static const int FRAME_HDR_MAX_SIZE = sizeof(FRAME_HDR) + 8 + 4;	// hdr + max len + mask


	static const int LEN16		= 0x7E;
	static const int LEN64		= 0x7F;
};


/*static*/ const int CWebsocketStream::BITS_DEFAULT			= -15;

/*static*/ const unsigned CWebsocketStream::FEATURE_NONE	= 0x0;
/*static*/ const unsigned CWebsocketStream::FEATURE_MASKING	= 0x1;
/*static*/ const unsigned CWebsocketStream::FEATURE_COMPRESS= 0x2;


CWebsocketStream::CWebsocketStream(void)
: m_bBegin(false)
, m_features(FEATURE_NONE)
, m_pCompressor(nullptr)
{
	;
};


CWebsocketStream::~CWebsocketStream(void)
{
	end();
};


bool CWebsocketStream::begin(int bits, unsigned features)
{
	if (m_bBegin)
		return false;

	if (IS_FLAG_SET(features, FEATURE_COMPRESS))
	{
		m_pCompressor = new CCompressor();
		if (!m_pCompressor->Start(bits))
		{
			delete m_pCompressor;
			m_pCompressor = nullptr;
			return false;
		};
	};

	m_features = features;
	m_bBegin = true;

	return true;
};


void CWebsocketStream::end(void)
{
	if (m_bBegin)
	{
		m_payload.clear();
		m_bBegin = false;
		m_features = FEATURE_NONE;

		if (m_pCompressor)
		{
			m_pCompressor->Stop();

			delete m_pCompressor;
			m_pCompressor = nullptr;
		};		
	};
};


int CWebsocketStream::write(const void* data, int dataSize, std::vector<unsigned char>& output, int opcode)
{
	if (!m_bBegin)
		return RESULT_FATAL;

	output.reserve(WSTYPES::FRAME_HDR_MAX_SIZE + dataSize);
	
	std::vector<unsigned char> compressed;

	//
	//	Write header and if required compress payload for knowing exactly size
	//
	assert(opcode != OPCODE_CONTINUE);	// fragmented msg not supported
	WSTYPES::FRAME_HDR header = { 0 };
	header.fin = true;
	header.opcode = opcode;
	if ((m_features & FEATURE_COMPRESS) == FEATURE_COMPRESS)
	{
		header.rsv1 = true;
		assert(m_pCompressor);
		compressed = m_pCompressor->Deflate(data, dataSize);
		assert(!compressed.empty());
	}
	else
	{
		compressed = std::vector<unsigned char>((unsigned char*)data, (unsigned char*)data + dataSize);
	};

	if ((m_features & FEATURE_MASKING) == FEATURE_MASKING)
		header.mask = true;
	
	//
	//	Write payload size
	//
	if (dataSize < 126)
	{
		header.length = compressed.size();
		output.insert(output.end(), (unsigned char*)&header, (unsigned char*)&header + sizeof(header));
	}
	else if (dataSize < std::numeric_limits<unsigned short>::max())
	{
		header.length = WSTYPES::LEN16;
		unsigned short size = SWAP2(unsigned short(compressed.size()));
		output.insert(output.end(), (unsigned char*)&header, (unsigned char*)&header + sizeof(header));
		output.insert(output.end(), (unsigned char*)&size, (unsigned char*)&size + sizeof(unsigned short));
	}
	else
	{
		header.length = WSTYPES::LEN64;
		unsigned long long size = SWAP8(unsigned long long(compressed.size()));
		output.insert(output.end(), (unsigned char*)&header, (unsigned char*)&header + sizeof(header));
		output.insert(output.end(), (unsigned char*)&size, (unsigned char*)&size + sizeof(unsigned long long));
	};
	
	//
	//	Mask payload if required
	//
	if ((m_features & FEATURE_MASKING) == FEATURE_MASKING)
	{
		unsigned Mask = RndUInt32(0x10000000, 0xFFFFFFFF);
		output.insert(output.end(), (unsigned char*)&Mask, (unsigned char*)&Mask + sizeof(unsigned));
		WsMaskUnmask(&compressed[0], &compressed[0], compressed.size(), (char*)&Mask);
	};	

	output.insert(output.end(), compressed.begin(), compressed.end());

	return RESULT_OK;
};


int CWebsocketStream::read(const void* data, int dataSize)
{
	int iResult = RESULT_OK;

	m_payload.insert(m_payload.end(), (char*)data, (char*)data + dataSize);

	do
	{
		std::size_t offset = 0;

		//
		//	Read header
		//
		if (m_payload.size() < (offset + sizeof(WSTYPES::FRAME_HDR)))	
			break;

		WSTYPES::FRAME_HDR* pHdr = (WSTYPES::FRAME_HDR*)&m_payload[offset];
		offset += sizeof(*pHdr);
		if ((pHdr->fin != true) ||
			(pHdr->opcode == OPCODE_CONTINUE))
		{
			//
			//	TODO fragmented msg
			//
			iResult = RESULT_FATAL;
			break;
		};

		//
		//	Read payload size
		//
		unsigned long long Size = 0;
		if (pHdr->length == WSTYPES::LEN16)
		{
			if (m_payload.size() < (offset + 2))
				break;

			unsigned short Size2 = *(unsigned short*)&m_payload[offset];
			Size = SWAP2(Size2);
			offset += 2;
		}
		else if (pHdr->length == WSTYPES::LEN64)
		{
			if (m_payload.size() < (offset + 8))
				break;

			unsigned long long Size8 = SWAP8(*(unsigned long long*)&m_payload[offset]);
			Size = SWAP8(Size8);
			offset += 8;
		}
		else
		{
			Size = pHdr->length;
		};

		if (!Size)
		{
			m_queueMessages.push_back({ pHdr->opcode, {} });
			m_payload.erase(m_payload.begin(), m_payload.begin() + sizeof(*pHdr) + unsigned(Size));
			continue;
		};

		//
		//	Read masking key
		//
		unsigned Mask = 0;
		if (pHdr->mask)
		{
			if (m_payload.size() < (offset + 4))
				break;			

			Mask = (*(unsigned*)&m_payload[offset]);
			offset += 4;
		};

		//
		//	Waiting till receive all payload 
		//	Then unmask and decompress it if requied
		//
		if ((m_payload.size() - offset) < Size)
			break;

		if (pHdr->mask)
			WsMaskUnmask(&m_payload[offset], &m_payload[offset], int(Size), (char*)&Mask);

		std::vector<unsigned char> output;

		if (pHdr->rsv1)
		{
			assert(m_pCompressor);
			output = m_pCompressor->Inflate(&m_payload[offset], int(Size));
			assert(!output.empty());
		}
		else
		{
			output.insert(output.end(), &m_payload[offset], (unsigned char*)&m_payload[offset] + Size);
		};

		m_queueMessages.push_back({ pHdr->opcode, output });
		m_payload.erase(m_payload.begin(), m_payload.begin() + offset + uint32(Size));
	} while (true);

	if (iResult == RESULT_OK)
		iResult = m_queueMessages.size();
		
	return iResult;
};


bool CWebsocketStream::read_message(MESSAGE& msg)
{
	if (m_queueMessages.empty())
		return false;

	msg = m_queueMessages.front();
	m_queueMessages.pop_front();

	return true;
};