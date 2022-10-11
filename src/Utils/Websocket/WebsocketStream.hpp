#pragma once


class CWebsocketStream
{
private:
	class CCompressor;
	class CMasker;

public:
	static const int BITS_DEFAULT;

	static const unsigned FEATURE_NONE;
	static const unsigned FEATURE_MASKING;
	static const unsigned FEATURE_COMPRESS;

	static const int OPCODE_CONTINUE= 0x0;
	static const int OPCODE_TEXT	= 0x1;
	static const int OPCODE_BINARY	= 0x2;
	static const int OPCODE_CLOSE	= 0x8;
	static const int OPCODE_PING	= 0x9;
	static const int OPCODE_PONG	= 0xA;

	static const int RESULT_FATAL	= -1;
	static const int RESULT_OK		= 0;

	struct MESSAGE
	{
		int m_opcode;
		std::vector<unsigned char> m_data;
	};

public:
	CWebsocketStream(void);
	~CWebsocketStream(void);
	bool begin(int bits, unsigned feature = FEATURE_NONE);
	void end(void);

	//
	//	Transforms outgoing data to an websocket frame data
	//	On success returns RESULT_OK, otherwise RESULT_FATAL and stream should be closed
	//
	int write(const void* data, int dataSize, std::vector<unsigned char>& output, int opcode);

	//
	//	Transforms incoming websocket frame data to user data.
	//	On success returns values >= 0, return code description:
	//		= 0 - more data required
	//		> 0 - more data required AND there is some completed messages, call read_message() for getting them
	//		< 0 - fatal error occurred, stream should be closed
	//
	int read(const void* data, int dataSize);

	//
	//	On success returns TRUE and fills "msg" with data
	//
	bool read_message(MESSAGE& msg);

private:
	bool m_bBegin;
	unsigned m_features;
	std::vector<unsigned char> m_payload;
	std::deque<MESSAGE> m_queueMessages;
	CCompressor* m_pCompressor;
};