#include "Ini.hpp"

#include <list>


static bool IsCharAlphabet(char ch);
static bool IsCharDigit(char ch);
static bool IsHexString(const char* str);
static std::string ValueToString(const char* pszFormat, ...);
static void StringToValue(const std::string& str, const char* pszFormat, ...);


class CIniContainer
{
public:
	static const char* SEGMENT_NULL;

	struct KEY
	{
		std::string m_name;
		std::string m_value;
	};

	struct SEGMENT
	{
		std::string m_name;
		std::list<KEY> m_listKey;
	};

public:
	CIniContainer(void);
	bool SegmentNew(const char* segment);
	SEGMENT* SegmentSearch(const char* segment);
	bool SegmentDelete(const char* segment);
	bool KeyNew(const char* segment, const char* key);
	KEY* KeySearch(const char* segment, const char* key);
	bool KeyDelete(const char* segment, const char* key);
	void KeyEnum(IniKeyEnumCallback_t pfnEnumCB, void* param = nullptr);
	std::string& KeyValue(const char* segment, const char* key);
	void WriteSegments(std::string& buffer, bool bWriteSegmentName) const;

private:
	std::list<SEGMENT> m_listSegment;
	SEGMENT* m_pCurrentSegment;
	KEY* m_pCurrentKey;
};


/*static*/ const char* CIniContainer::SEGMENT_NULL = "NULL";


class CIniReader
{
private:
	static const int32 WRITEBUF_MAX = 256;
	
	enum STATE
	{
		STATE_NONE,
		STATE_IDLE,
		STATE_KEY,
		STATE_VALUE,
		STATE_SEGMENT,
		STATE_COMMENT,

		STATE_MAX,
	};

public:
	CIniReader(CIniContainer& container);
	bool Import(const char* buffer, int32 bufferSize);
	bool Export(char* buffer, int32* bufferSize);
	
private:
	void onBegin(void);
	void onIdle(char ch);
	void onSegment(char ch);
	void onKey(char ch);
	void onValue(char ch);
	void onComment(char ch);
	void onEnd(void);
	void changeState(STATE state);
	STATE currentState(void) const;
	void clearWriteBuffer(void);
	void writeChr(char ch);

private:
	CIniContainer& m_container;
	STATE m_state;
	std::string m_CurrentSegment;
	std::string m_CurrentKey;
	std::string m_buffer;
	int32 m_nChrSkip;
	bool m_bBegin;
};


static bool IsCharAlphabet(char ch)
{
	return	(ch >= 'a' && ch <= 'z') ||
			(ch >= 'A' && ch <= 'Z');
};


static bool IsCharDigit(char ch)
{
	return	(ch >= '0' && ch <= '9');
};


static bool IsHexString(const char* str)
{
	if (str[0] == '0' &&
		str[1] == 'x')
	{
		while (*str)
		{
			char ch = *str++;

			bool bResult = IsCharDigit(ch) || IsCharAlphabet(ch);

			if (!bResult)
				return false;
		};
		
		return true;
	};

	return false;
};


static std::string ValueToString(const char* pszFormat, ...)
{
	char szBuff[1024];	
	szBuff[0] = '\0';
	
	va_list vl;
	va_start(vl, pszFormat);

	if (!std::strcmp(pszFormat, "%b"))
	{
		bool value = va_arg(vl, bool);
		std::strcpy(szBuff, value ? "true" : "false");
	}
	else
	{
		std::vsprintf(szBuff, pszFormat, vl);
	};

	va_end(vl);

	return std::string(szBuff);
};


static void StringToValue(const std::string& str, const char* pszFormat, ...)
{
	va_list vl;
	va_start(vl, pszFormat);

	if (!std::strcmp(pszFormat, "%b"))
	{
		bool* value = va_arg(vl, bool*);
		bool bResult = false;
		if (!std::strcmp(&str[0], "true"))
			bResult = true;
		*value = bResult;
	}
	else
	{
		std::vsscanf(&str[0], pszFormat, vl);
	};

	va_end(vl);
};


CIniContainer::CIniContainer(void)
: m_listSegment()
, m_pCurrentSegment(nullptr)
, m_pCurrentKey(nullptr)
{
	SegmentNew(SEGMENT_NULL);
};


bool CIniContainer::SegmentNew(const char* segment)
{
	if (SegmentSearch(segment))
		return false;

	m_listSegment.push_back({ segment, {} });
	m_pCurrentSegment = &m_listSegment.back();

	return true;
};


CIniContainer::SEGMENT* CIniContainer::SegmentSearch(const char* segment)
{
	if (!segment)
		segment = SEGMENT_NULL;

	if (m_pCurrentSegment && m_pCurrentSegment->m_name == segment)
		return m_pCurrentSegment;

	auto it = m_listSegment.begin();
	while (it != m_listSegment.end())
	{
		if ((*it).m_name == segment)
		{
			m_pCurrentSegment = &*it;
			return &*it;
		};

		++it;
	};

	return nullptr;
};


bool CIniContainer::SegmentDelete(const char* segment)
{
	if (!SegmentSearch(segment))
		return false;

	if (!segment || (segment && std::string(segment) == SEGMENT_NULL))
		return false;

	auto it = m_listSegment.begin();
	while (it != m_listSegment.end())
	{
		if ((*it).m_name == segment)
		{
			m_pCurrentSegment = nullptr;
			m_pCurrentKey = nullptr;

			m_listSegment.erase(it);
			return true;
		};

		++it;
	};

	return false;
};


bool CIniContainer::KeyNew(const char* segment, const char* key)
{
	if (KeySearch(segment, key))
		return false;

	m_pCurrentSegment->m_listKey.push_back(KEY({ key, "" }));
	m_pCurrentKey = &m_pCurrentSegment->m_listKey.back();

	return true;
};


CIniContainer::KEY* CIniContainer::KeySearch(const char* segment, const char* key)
{
	SEGMENT* pSegment = SegmentSearch(segment);
	if (!pSegment)
		return nullptr;

	auto it = pSegment->m_listKey.begin();
	while (it != pSegment->m_listKey.end())
	{
		if ((*it).m_name == key)
		{
			m_pCurrentKey = &*it;
			return &*it;
		};

		++it;
	};

	return nullptr;
};


bool CIniContainer::KeyDelete(const char* segment, const char* key)
{
	if (!KeySearch(segment, key))
		return false;

	auto it = m_pCurrentSegment->m_listKey.begin();
	while (it != m_pCurrentSegment->m_listKey.end())
	{
		if ((*it).m_name == key)
		{
			m_pCurrentKey = nullptr;

			m_pCurrentSegment->m_listKey.erase(it);

			return true;
		};

		++it;
	};

	return false;
};


void CIniContainer::KeyEnum(IniKeyEnumCallback_t pfnEnumCB, void* param)
{
	for (auto& itA : m_listSegment)
	{
		for (auto& itB : itA.m_listKey)
		{
			if (!pfnEnumCB(itA.m_name.c_str(), itB.m_name.c_str(), itB.m_value.c_str(), param))
				return;
		};
	};
};


std::string& CIniContainer::KeyValue(const char* segment, const char* key)
{
	return m_pCurrentKey->m_value;
};


void CIniContainer::WriteSegments(std::string& buffer, bool bWriteSegmentName) const
{
	for (auto& segment : m_listSegment)
	{
		if (bWriteSegmentName && segment.m_name != SEGMENT_NULL)
		{
			buffer += '[';
			buffer += segment.m_name;
			buffer += ']';
			buffer += '\n';
		};

		for (auto& key : segment.m_listKey)
		{
			buffer += key.m_name;
			buffer += " = ";
			buffer += key.m_value;
			buffer += '\n';
		};

		if (bWriteSegmentName)
			buffer += '\n';
	};
};


CIniReader::CIniReader(CIniContainer& container)
: m_container(container)
, m_state(STATE_NONE)
, m_CurrentSegment()
, m_CurrentKey()
, m_buffer()
, m_nChrSkip(0)
, m_bBegin(false)
{
	m_CurrentSegment.reserve(256);
	m_buffer.reserve(256);
};


bool CIniReader::Import(const char* buffer, int32 bufferSize)
{
	if (!buffer || !bufferSize)
		return false;

	onBegin();

	char* pBuffer = new char[bufferSize + 1];
	std::memcpy(pBuffer, buffer, bufferSize);

	if (bufferSize >= 1)
	{
		if (pBuffer[bufferSize - 1] != '\n')
			pBuffer[bufferSize] = '\n';
	};

	for (int32 i = 0; i < bufferSize + 1; i++)
	{
		char ch = pBuffer[i];

		switch (currentState())
		{
		case STATE::STATE_IDLE:
			onIdle(ch);
			break;

		case STATE::STATE_KEY:
			onKey(ch);
			break;

		case STATE::STATE_VALUE:
			onValue(ch);
			break;

		case STATE::STATE_SEGMENT:
			onSegment(ch);
			break;

		case STATE::STATE_COMMENT:
			onComment(ch);
			break;
		};
	};

	if (pBuffer)
	{
		delete[] pBuffer;
		pBuffer = nullptr;
	};

	onEnd();

	return true;
};


bool CIniReader::Export(char* buffer, int32* bufferSize)
{
	std::string strBuffer;
	strBuffer.reserve(4096);

	m_container.WriteSegments(strBuffer, true);

	if (*bufferSize < int32(strBuffer.size()))
	{
		*bufferSize = strBuffer.size();
		return true;
	};

	if (!buffer)
		return false;

	*bufferSize = strBuffer.size();
	std::memcpy(&buffer[0], &strBuffer[0], strBuffer.size());

	return true;
};


void CIniReader::onBegin(void)
{
	changeState(STATE_IDLE);
};


void CIniReader::onIdle(char ch)
{
	if (ch == '[')
	{
		clearWriteBuffer();
		changeState(STATE_SEGMENT);
	}
	else if (ch == ';')
	{
		changeState(STATE_COMMENT);
	}
	else if (IsCharAlphabet(ch) || IsCharDigit(ch))
	{
		clearWriteBuffer();
		changeState(STATE_KEY);
		writeChr(ch);
	};
};


void CIniReader::onSegment(char ch)
{
	if (IsCharAlphabet(ch) || IsCharDigit(ch))
	{
		writeChr(ch);
	}
	else if (ch == ']')
	{
		writeChr('\0');

		m_container.SegmentNew(&m_buffer[0]);
		m_CurrentSegment = m_buffer;

		clearWriteBuffer();
		changeState(STATE_IDLE);
	};
};


void CIniReader::onKey(char ch)
{
	if (ch != '=')
	{
		writeChr(ch);
	}
	else
	{
		m_buffer[m_buffer.size() - m_nChrSkip] = '\0';

		if (m_CurrentSegment.empty())
			m_container.KeyNew(nullptr, &m_buffer[0]);
		else
			m_container.KeyNew(&m_CurrentSegment[0], &m_buffer[0]);

		m_CurrentKey = m_buffer;

		clearWriteBuffer();
		changeState(STATE_VALUE);
	};
};


void CIniReader::onValue(char ch)
{
	if (ch != '\n' &&
		ch != '\r' &&
		ch != ';')
	{
		writeChr(ch);
	}
	else
	{
		m_buffer[m_buffer.size() - m_nChrSkip] = '\0';

		if (m_CurrentSegment.empty())
			m_container.KeyValue(nullptr, &m_CurrentKey[0]) = m_buffer;
		else
			m_container.KeyValue(&m_CurrentSegment[0], &m_CurrentKey[0]) = m_buffer;

		if (ch == ';')
			changeState(STATE_COMMENT);
		else
			changeState(STATE_IDLE);
	};
};


void CIniReader::onComment(char ch)
{
	if (ch == '\n')
		changeState(STATE_IDLE);
};


void CIniReader::onEnd(void)
{
	;
};


void CIniReader::changeState(STATE state)
{
	m_state = state;
};


CIniReader::STATE CIniReader::currentState(void) const
{
	return m_state;
};


void CIniReader::clearWriteBuffer(void)
{
	m_buffer.clear();
	m_bBegin = false;
	m_nChrSkip = 0;
};


void CIniReader::writeChr(char ch)
{
	switch (ch)
	{
	case ' ':
	case '\t':
		if (m_bBegin)
		{
			m_buffer += ch;
			++m_nChrSkip;
		};
		break;

	default:
		m_buffer += ch;
		if (!m_bBegin)
			m_bBegin = true;
		else
			m_nChrSkip = 0;
		break;
	};
};


static CIniContainer& IniContainer(void* hIni)
{	
	return *(CIniContainer*)hIni;
};


HOBJ IniNew(void)
{
	return new CIniContainer();
};


HOBJ IniOpen(const char* Buffer, int32 BufferSize)
{
	CIniContainer* pIni = new CIniContainer();
	if (pIni)
	{
		CIniReader IniReader(*pIni);
		if (!IniReader.Import(Buffer, BufferSize))
		{
			delete pIni;
			pIni = nullptr;
		};
	};

	return pIni;
};


bool IniSave(HOBJ hIni, char* Buffer, int32* BufferSize)
{
	CIniReader IniReader(*((CIniContainer*)hIni));
	return IniReader.Export(Buffer, BufferSize);
};


void IniClose(HOBJ hIni)
{	
	CIniContainer* pIni = (CIniContainer*)hIni;
	delete pIni;
};


bool IniSegmentNew(HOBJ hIni, const char* Segment)
{
	return IniContainer(hIni).SegmentNew(Segment);
};


bool IniSegmentSearch(HOBJ hIni, const char* Segment)
{
	return (IniContainer(hIni).SegmentSearch(Segment) != nullptr);
};


bool IniSegmentDelete(HOBJ hIni, const char* Segment)
{
	return IniContainer(hIni).SegmentDelete(Segment);
};


bool IniKeyNew(HOBJ hIni, const char* Segment, const char* Key)
{
	return IniContainer(hIni).KeyNew(Segment, Key);
};


bool IniKeySearch(HOBJ hIni, const char* Segment, const char* Key)
{
	return (IniContainer(hIni).KeySearch(Segment, Key) != nullptr);
};


bool IniKeyDelete(HOBJ hIni, const char* Segment, const char* Key)
{
	return IniContainer(hIni).KeyDelete(Segment, Key);
};


void IniKeyEnum(HOBJ hIni, IniKeyEnumCallback_t pfnEnumCB, void* Param)
{
	IniContainer(hIni).KeyEnum(pfnEnumCB, Param);
};


bool IniKeyWriteInt(HOBJ hIni, const char* Segment, const char* Key, int32 Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	IniContainer(hIni).KeyValue(Segment, Key) = ValueToString("%d", Value);
	return true;
};


bool IniKeyWriteUInt(HOBJ hIni, const char* Segment, const char* Key, uint32 Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	IniContainer(hIni).KeyValue(Segment, Key) = ValueToString("%u", Value);
	return true;
};


bool IniKeyWriteHex(HOBJ hIni, const char* Segment, const char* Key, uint32 Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	IniContainer(hIni).KeyValue(Segment, Key) = ValueToString("0x%08x", Value);
	return true;
};


bool IniKeyWriteFloat(HOBJ hIni, const char* Segment, const char* Key, float Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	IniContainer(hIni).KeyValue(Segment, Key) = ValueToString("%f", Value);
	return true;
};


bool IniKeyWriteDouble(HOBJ hIni, const char* Segment, const char* Key, double Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	IniContainer(hIni).KeyValue(Segment, Key) = ValueToString("%lf", Value);
	return true;
};


bool IniKeyWriteBool(HOBJ hIni, const char* Segment, const char* Key, bool Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	IniContainer(hIni).KeyValue(Segment, Key) = ValueToString("%b", Value);
	return true;
};


bool IniKeyWriteString(HOBJ hIni, const char* Segment, const char* Key, const char* Buffer, int32 BufferSize)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	IniContainer(hIni).KeyValue(Segment, Key) = std::string(Buffer, Buffer + BufferSize);
	return true;
};


bool IniKeyWriteString(HOBJ hIni, const char* Segment, const char* Key, const char* Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	IniContainer(hIni).KeyValue(Segment, Key) = std::string(Value);
	return true;
};


bool IniKeyReadInt(HOBJ hIni, const char* Segment, const char* Key, int32* Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	StringToValue(IniContainer(hIni).KeyValue(Segment, Key), "%d", Value);
	return true;
};


bool IniKeyReadUInt(HOBJ hIni, const char* Segment, const char* Key, uint32* Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	StringToValue(IniContainer(hIni).KeyValue(Segment, Key), "%u", Value);
	return true;
};


bool IniKeyReadHex(HOBJ hIni, const char* Segment, const char* Key, uint32* Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	StringToValue(IniContainer(hIni).KeyValue(Segment, Key), "0x%x", Value);
	return true;
};


bool IniKeyReadFloat(HOBJ hIni, const char* Segment, const char* Key, float* Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	StringToValue(IniContainer(hIni).KeyValue(Segment, Key), "%f", Value);
	return true;
};


bool IniKeyReadDouble(HOBJ hIni, const char* Segment, const char* Key, double* Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	StringToValue(IniContainer(hIni).KeyValue(Segment, Key), "%lf", Value);
	return true;
};


bool IniKeyReadBool(HOBJ hIni, const char* Segment, const char* Key, bool* Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	StringToValue(IniContainer(hIni).KeyValue(Segment, Key), "%b", Value);
	return true;
};


bool IniKeyReadString(HOBJ hIni, const char* Segment, const char* Key, char* Buffer, int32 BufferSize)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	std::string& KeyValue = IniContainer(hIni).KeyValue(Segment, Key);
	if (uint32(BufferSize) >= KeyValue.size())
		return false;
		
	std::strncpy(Buffer, &KeyValue[0], KeyValue.size());
	return true;
};


bool IniKeyReadString(HOBJ hIni, const char* Segment, const char* Key, const char** Value)
{
	if (!IniContainer(hIni).KeySearch(Segment, Key))
		return false;

	*Value = &IniContainer(hIni).KeyValue(Segment, Key)[0];
	return true;
};

