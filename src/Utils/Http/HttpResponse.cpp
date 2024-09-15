#include "HttpResponse.hpp"

#include "llhttp/llhttp.h"


class CHttpResponse::impl final
{
private:
	enum flag {
		flag_keepalive 		= (1 << 0),
		flag_complete 		= (1 << 1),
		flag_url_complete 	= (1 << 2),
	};

public:
	impl();
	~impl();
	void clear();
	bool process(const void* data, std::size_t size);
	std::size_t header_count(const std::string& hdr) const;
	std::string header_value(const std::string& hdr) const;
	std::vector<std::string> header_values(const std::string& hdr) const;
	int on_status_complete();
	int on_url_complete();
	int on_header_field_complete();
	int on_header_value_complete();
	int on_headers_complete();
	int on_message_complete();
	int on_url(const char* data, std::size_t size);
	int on_header_field(const char* data, std::size_t size);
	int on_header_value(const char* data, std::size_t size);
	int on_body(const char* data, std::size_t size);

	inline int status() const {
		return m_status;
	};

	inline const char* body() const {
		return m_bufferBody.data();
	};

	inline std::size_t body_size() const {
		return m_bufferBody.size();
	};

	inline bool is_complete() const {
		return flag_test(flag_complete);
	};
	
	inline bool is_keepalive() const {
		return flag_test(flag_keepalive);
	};

	inline void flag_set(uint32 f, bool state) {
		(state ? (m_flags |= f) : (m_flags &= ~f));
	};
	
	inline bool flag_test(uint32 f) const {
		return (m_flags & f) == f;
	};

private:
	unsigned long m_flags;
	int m_status;
	llhttp_t m_parser;
	llhttp_settings_t m_parserCfg;
	std::vector<char> m_bufferUrl;
	std::string m_bufferHdrField;
	std::string m_bufferHdrValue;
	std::vector<char> m_bufferBody;
	std::unordered_multimap<std::string, std::string> m_mapHeaders;
};


CHttpResponse::impl::impl()
: m_flags(0u)
, m_status(-1)
, m_parser()
, m_parserCfg()
, m_bufferUrl()
, m_bufferHdrField()
, m_bufferHdrValue()
, m_bufferBody()
, m_mapHeaders()
{
	llhttp_settings_init(&m_parserCfg);

	/* completers */
	m_parserCfg.on_status_complete = [](llhttp_t* obj) {
		return reinterpret_cast<impl*>(obj->data)->on_status_complete();
	};
	
	m_parserCfg.on_url_complete = [](llhttp_t* obj) {
		return reinterpret_cast<impl*>(obj->data)->on_url_complete();
	};
	
	m_parserCfg.on_header_field_complete = [](llhttp_t* obj) {
		return reinterpret_cast<impl*>(obj->data)->on_header_field_complete();
	};
	
	m_parserCfg.on_header_value_complete = [](llhttp_t* obj) {
		return reinterpret_cast<impl*>(obj->data)->on_header_value_complete();
	};
	
	m_parserCfg.on_headers_complete = [](llhttp_t* obj) {
		return reinterpret_cast<impl*>(obj->data)->on_headers_complete();
	};

	m_parserCfg.on_message_complete = [](llhttp_t* obj) {
		return reinterpret_cast<impl*>(obj->data)->on_message_complete();
	};

	/* readers */
	m_parserCfg.on_url = [](llhttp_t* obj, const char* data, std::size_t size) {
		return reinterpret_cast<impl*>(obj->data)->on_url(data, size);
	};
	
	m_parserCfg.on_header_field = [](llhttp_t* obj, const char* data, std::size_t size) {
		return reinterpret_cast<impl*>(obj->data)->on_header_field(data, size);
	};
	
	m_parserCfg.on_header_value = [](llhttp_t* obj, const char* data, std::size_t size) {
		return reinterpret_cast<impl*>(obj->data)->on_header_value(data, size);
	};

	m_parserCfg.on_body = [](llhttp_t* obj, const char* data, std::size_t size) {
		return reinterpret_cast<impl*>(obj->data)->on_body(data, size);
	};

	llhttp_init(&m_parser, llhttp_type::HTTP_RESPONSE, &m_parserCfg);
	m_parser.data = this;
};


CHttpResponse::impl::~impl()
{};


void CHttpResponse::impl::clear()
{
	m_flags = 0u;
	m_status = -1;
	
	llhttp_init(&m_parser, llhttp_type::HTTP_RESPONSE, &m_parserCfg);
	m_parser.data = this;
	
	m_bufferUrl.clear();
	m_bufferHdrField.clear();
	m_bufferHdrValue.clear();
	m_bufferBody.clear();
	m_mapHeaders.clear();
};


bool CHttpResponse::impl::process(const void* data, std::size_t size)
{
	llhttp_errno_t err = llhttp_execute(&m_parser, reinterpret_cast<const char*>(data), size);
	switch (err)
	{
	case HPE_OK:
		return true;

	case HPE_PAUSED_UPGRADE:
		llhttp_resume_after_upgrade(&m_parser);
		return true;

	default:
		OUTPUTLN("%s failed with %" PRIi32, __FUNCTION__, int32(err));
		return false;
	};
};


std::size_t CHttpResponse::impl::header_count(const std::string& hdr) const
{
	auto itPair = m_mapHeaders.equal_range(hdr);
	return std::distance(itPair.first, itPair.second);
};


std::string CHttpResponse::impl::header_value(const std::string& hdr) const
{
	/* convert header to lower case format */
	std::string hdrLC(hdr);
	std::transform(hdrLC.begin(), hdrLC.end(), hdrLC.begin(), std::tolower);
	
	auto it = m_mapHeaders.find(hdr);
	return (it != m_mapHeaders.end() ? it->second : std::string());
};


std::vector<std::string> CHttpResponse::impl::header_values(const std::string& hdr) const
{
	auto itPair = m_mapHeaders.equal_range(hdr);
	if (std::distance(itPair.first, itPair.second)) {
		std::vector<std::string> values;
		std::for_each(itPair.first, itPair.second, [&values](std::pair<std::string, std::string> mapPair) {
			values.push_back(mapPair.second);
		});
		return values;
	};
	return{};
};


int CHttpResponse::impl::on_status_complete()
{
	m_status = m_parser.status_code;

	/* pre alloc space for url & header read buffer */
	m_bufferUrl.reserve(256u);
	m_bufferHdrField.reserve(64u);
	m_bufferHdrValue.reserve(128u);
	m_mapHeaders.reserve(12);

	return HPE_OK;
};


int CHttpResponse::impl::on_url_complete()
{
	ASSERT(flag_test(flag_url_complete) == false);
	flag_set(flag_url_complete, true);
	
	return HPE_OK;
};


int CHttpResponse::impl::on_header_field_complete()
{
	return HPE_OK;
};


int CHttpResponse::impl::on_header_value_complete()
{
	std::transform(m_bufferHdrField.begin(), m_bufferHdrField.end(), m_bufferHdrField.begin(), std::tolower);

	m_mapHeaders.insert({ std::move(m_bufferHdrField), std::move(m_bufferHdrValue) });
	m_bufferHdrField.clear();
	m_bufferHdrValue.clear();
	
	return HPE_OK;
};


int CHttpResponse::impl::on_headers_complete()
{
	if (llhttp_should_keep_alive(&m_parser) == 1)
		flag_set(flag_keepalive, true);

	/* pre alloc body buffer */
	std::string result = header_value("content-length");
	if (!result.empty())
		m_bufferBody.reserve(std::stoi(result));

	return HPE_OK;
};


int CHttpResponse::impl::on_message_complete()
{
	ASSERT(flag_test(flag_complete) == false);
	flag_set(flag_complete, true);

	return HPE_OK;
};


int CHttpResponse::impl::on_url(const char* data, std::size_t size)
{
	m_bufferUrl.insert(std::end(m_bufferUrl), data, data + size);
	return HPE_OK;
};


int CHttpResponse::impl::on_header_field(const char* data, std::size_t size)
{
	m_bufferHdrField.insert(std::end(m_bufferHdrField), data, data + size);
	return HPE_OK;
};


int CHttpResponse::impl::on_header_value(const char* data, std::size_t size)
{
	m_bufferHdrValue.insert(std::end(m_bufferHdrValue), data, data + size);
	return HPE_OK;
};


int CHttpResponse::impl::on_body(const char* data, std::size_t size)
{
	m_bufferBody.insert(std::end(m_bufferBody), data, data + size);
	return HPE_OK;
};


CHttpResponse::CHttpResponse() = default;
CHttpResponse::~CHttpResponse() = default;


void CHttpResponse::clear()
{
	m_pimpl->clear();
};


bool CHttpResponse::process(const void* data, std::size_t size)
{
	return m_pimpl->process(data, size);
};


httpstatus::value CHttpResponse::status() const
{
	return httpstatus::value(m_pimpl->status());
};


int CHttpResponse::header_count(const std::string& hdr) const
{
	return m_pimpl->header_count(hdr);
};


std::string CHttpResponse::header_value(const std::string& hdr) const
{
	return m_pimpl->header_value(hdr);
};


std::vector<std::string> CHttpResponse::header_values(const std::string& hdr) const
{
	return m_pimpl->header_values(hdr);
};


const char* CHttpResponse::body() const
{
	return m_pimpl->body();
};


std::size_t CHttpResponse::body_size() const
{
	return m_pimpl->body_size();
};


bool CHttpResponse::is_complete() const
{
	return m_pimpl->is_complete();
};


bool CHttpResponse::is_keepalive() const
{
	return m_pimpl->is_keepalive();
};