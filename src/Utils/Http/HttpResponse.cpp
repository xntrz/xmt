#include "HttpResponse.hpp"
#include "llhttp/llhttp.h"


class CHttpResponse::Impl final
{
private:
	struct llhttp_callback_router
	{
		static Impl* to_impl(llhttp_t* parserobj)
		{
			assert(parserobj->data);
			return (Impl*)parserobj->data;
		};


		static int on_status_complete(llhttp_t* parserobj)
		{
			to_impl(parserobj)->onStatusComplete();
			return HPE_OK;
		};


		static int on_url_complete(llhttp_t* parserobj)
		{
			to_impl(parserobj)->onUrlComplete();
			return HPE_OK;
		};


		static int on_header_field_complete(llhttp_t* parserobj)
		{
			to_impl(parserobj)->onHeaderFieldComplete();
			return HPE_OK;
		};


		static int on_header_value_complete(llhttp_t* parserobj)
		{
			to_impl(parserobj)->onHeaderValueComplete();
			return HPE_OK;
		};


		static int on_headers_complete(llhttp_t* parserobj)
		{
			to_impl(parserobj)->onHeadersComplete();
			return HPE_OK;
		};


		static int on_message_complete(llhttp_t* parserobj)
		{
			to_impl(parserobj)->onMessageComplete();
			return HPE_OK;
		};


		static int on_url(llhttp_t* parserobj, const char* data, std::size_t dataSize)
		{
			to_impl(parserobj)->onAccumulateDataUrl(data, dataSize);
			return HPE_OK;
		};


		static int on_header_field(llhttp_t* parserobj, const char* data, std::size_t dataSize)
		{
			to_impl(parserobj)->onAccumulateDataHeaderField(data, dataSize);
			return HPE_OK;
		};


		static int on_header_value(llhttp_t* parserobj, const char* data, std::size_t dataSize)
		{
			to_impl(parserobj)->onAccumulateDataHeaderValue(data, dataSize);
			return HPE_OK;
		};


		static int on_body(llhttp_t* parserobj, const char* data, std::size_t dataSize)
		{
			to_impl(parserobj)->onAccumulateDataBody(data, dataSize);
			return HPE_OK;
		};
	};


	enum FLAG
	{
		FLAG_NONE			= 0x0,
		FLAG_KEEPALIVE		= 0x1,
		FLAG_COMPLETE		= 0x2,
		FLAG_URL_COMPLETE	= 0x4,
	};


public:
	Impl(void)
	{
		m_status = -1;
		m_bufferUrl.clear();
		m_bufferHdrField.clear();
		m_bufferHdrValue.clear();
		m_bufferBody.clear();
		m_mapHeader.clear();
		m_flags = FLAG_NONE;
		parserInit();
	};


	~Impl(void)
	{
		;
	};


	void parserInit(void)
	{
		llhttp_settings_init(&m_parserSettei);

		m_parserSettei.on_status_complete		= llhttp_callback_router::on_status_complete;
		m_parserSettei.on_url_complete			= llhttp_callback_router::on_url_complete;
		m_parserSettei.on_header_field_complete = llhttp_callback_router::on_header_field_complete;
		m_parserSettei.on_header_value_complete = llhttp_callback_router::on_header_value_complete;
		m_parserSettei.on_headers_complete		= llhttp_callback_router::on_headers_complete;
		m_parserSettei.on_message_complete		= llhttp_callback_router::on_message_complete;

		m_parserSettei.on_url			= llhttp_callback_router::on_url;
		m_parserSettei.on_header_field	= llhttp_callback_router::on_header_field;
		m_parserSettei.on_header_value	= llhttp_callback_router::on_header_value;
		m_parserSettei.on_body			= llhttp_callback_router::on_body;

		llhttp_init(&m_parser, llhttp_type::HTTP_BOTH, &m_parserSettei);
		m_parser.data = this;
	};


	void parserTerm(void)
	{
		llhttp_finish(&m_parser);
	};


	void clear(void)
	{
		m_flags = FLAG_NONE;
		m_status = -1;
		parserTerm();
		parserInit();
		m_bufferUrl.clear();
		m_bufferHdrField.clear();
		m_bufferHdrValue.clear();
		m_bufferBody.clear();
		m_mapHeader.clear();
		m_mapCookies.clear();
	};


	bool process(const char* data, int dataSize)
	{
		llhttp_errno_t ErrorNo = llhttp_execute(&m_parser, data, dataSize);
		switch (ErrorNo)
		{
		case HPE_OK:
			return true;

		case HPE_PAUSED_UPGRADE:
			llhttp_resume_after_upgrade(&m_parser);
			return true;

		default:
			assert(false);
			return false;
		};
	};


	int status(void) const
	{
		return m_status;
	};


	int header_count(const char* hdr) const
	{
		auto itPair = m_mapHeader.equal_range(hdr);
		return std::distance(itPair.first, itPair.second);
	};


	std::string header_value(const char* hdr) const
	{
		auto it = m_mapHeader.find(hdr);
		if (it != m_mapHeader.end())
			return it->second;
		else
			return{};
	};


	std::vector<std::string> header_values(const char* hdr) const
	{
		auto itPair = m_mapHeader.equal_range(hdr);
		if (std::distance(itPair.first, itPair.second))
		{
			std::vector<std::string> values;
			std::for_each(itPair.first, itPair.second, [&values](std::pair<std::string, std::string> mapPair)
			{
				values.push_back(mapPair.second);
			});
			return values;
		};

		return{};
	};


	std::string cookie_value(const char* cookie) const
	{
		auto it = m_mapCookies.find(cookie);
		if (it != m_mapCookies.end())
			return it->second;
		else
			return{};
	};


	char* body(void) const
	{
		return (char*)m_bufferBody.data();
	};


	int body_size(void) const
	{
		return m_bufferBody.size();
	};


	bool is_complete(void) const
	{
		return isFlagSet(FLAG_COMPLETE);
	};


	bool is_keepalive(void) const
	{
		return isFlagSet(FLAG_KEEPALIVE);
	};


	void setFlag(FLAG flag, bool bSet)
	{
		if (bSet)
			m_flags |= flag;
		else
			m_flags &= ~flag;
	};


	bool isFlagSet(FLAG flag) const
	{
		return (m_flags & flag) == flag;
	};


	void onStatusComplete(void)
	{
		m_status = m_parser.status_code;
	};


	void onUrlComplete(void)
	{
		assert(!isFlagSet(FLAG_URL_COMPLETE));
		setFlag(FLAG_URL_COMPLETE, true);
	};


	void onHeadersComplete(void)
	{
		if (llhttp_should_keep_alive(&m_parser) == 1)
			setFlag(FLAG_KEEPALIVE, true);

		std::string result = header_value("content-length");
		if (!result.empty())
			m_bufferBody.reserve(std::atoi(result.c_str()));
	};


	void onHeaderFieldComplete(void)
	{
		;
	};


	void onHeaderValueComplete(void)
	{
		std::transform(
			m_bufferHdrField.begin(), 
			m_bufferHdrField.end(), 
			m_bufferHdrField.begin(),
			[](unsigned char ch)
			{
				return std::tolower(ch);
			}
		);

		m_mapHeader.insert({ m_bufferHdrField, m_bufferHdrValue });

		m_bufferHdrField.clear();
		m_bufferHdrValue.clear();
	};


	void onMessageComplete(void)
	{
		assert(!isFlagSet(FLAG_COMPLETE));
		setFlag(FLAG_COMPLETE, true);

		std::vector<std::string> setcookies = header_values("set-cookie");
		for (auto& str : setcookies)
		{
			std::string cookie = str.substr(0, str.find_first_of(';'));
			if (!cookie.empty())
			{
				std::string cookieName = cookie.substr(0, cookie.find_first_of('='));
				if (!cookieName.empty())
				{
					std::string cookieValue = cookie.substr(cookie.find_first_of('=') + 1, std::string::npos);
					m_mapCookies.insert({ cookieName, cookieValue });
				};
			};
		};
	};


	void onAccumulateDataUrl(const char* data, std::size_t dataSize)
	{
		m_bufferUrl.insert(std::end(m_bufferUrl), data, data + dataSize);
	};


	void onAccumulateDataBody(const char* data, std::size_t dataSize)
	{
		m_bufferBody.insert(std::end(m_bufferBody), data, data + dataSize);
	};


	void onAccumulateDataHeaderField(const char* data, std::size_t dataSize)
	{
		m_bufferHdrField.insert(std::end(m_bufferHdrField), data, data + dataSize);
	};


	void onAccumulateDataHeaderValue(const char* data, std::size_t dataSize)
	{
		m_bufferHdrValue.insert(std::end(m_bufferHdrValue), data, data + dataSize);
	};


private:
	unsigned long m_flags;
	int m_status;
	llhttp_t m_parser;
	llhttp_settings_t m_parserSettei;
	std::vector<unsigned char> m_bufferUrl;
	std::string m_bufferHdrField;
	std::string m_bufferHdrValue;
	std::vector<unsigned char> m_bufferBody;
	std::unordered_multimap<std::string, std::string> m_mapHeader;
	std::unordered_multimap<std::string, std::string> m_mapCookies;
};


CHttpResponse::CHttpResponse(void)
: m_pImpl(nullptr)
{
	m_pImpl = new Impl;
	assert(m_pImpl);
};


CHttpResponse::~CHttpResponse(void)
{
	if (m_pImpl)
	{
		delete m_pImpl;
		m_pImpl = nullptr;
	};
};


void CHttpResponse::clear(void)
{
	impl().clear();
};


bool CHttpResponse::process(const char* data, int dataSize)
{
	return impl().process(data, dataSize);
};


httpstatus::value CHttpResponse::status(void) const
{
	return httpstatus::value(impl().status());
};


int CHttpResponse::header_count(const char* hdr) const
{
	return impl().header_count(hdr);
};


std::string CHttpResponse::header_value(const char* hdr) const
{
	return impl().header_value(hdr);
};


std::vector<std::string> CHttpResponse::header_values(const char* hdr) const
{
	return impl().header_values(hdr);
};


std::string CHttpResponse::cookie_value(const char* cookie) const
{
	return impl().cookie_value(cookie);	
};


char* CHttpResponse::body(void) const
{
	return impl().body();
};


int CHttpResponse::body_size(void) const
{
	return impl().body_size();
};


bool CHttpResponse::is_complete(void) const
{
	return impl().is_complete();
};


bool CHttpResponse::is_keepalive(void) const
{
	return impl().is_keepalive();
};


CHttpResponse::Impl& CHttpResponse::impl(void)
{
	assert(m_pImpl);
	return *m_pImpl;
};


const CHttpResponse::Impl& CHttpResponse::impl(void) const
{
	assert(m_pImpl);
	return *m_pImpl;
};