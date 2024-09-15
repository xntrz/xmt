#pragma once

#include "HttpStatus.hpp"
#include "HttpUtils.hpp"

#include "Utils/Misc/Pimpl.hpp"


class CHttpResponse final
{
public:
	CHttpResponse();
	~CHttpResponse();
	
	void clear();
	bool process(const void* data, std::size_t size);
	
	httpstatus::value status() const;
	int header_count(const std::string& hdr) const;
	std::string header_value(const std::string& hdr) const;
	std::vector<std::string> header_values(const std::string& hdr) const;
	const char* body() const;
	std::size_t body_size() const;
	bool is_complete() const;
	bool is_keepalive() const;

private:
	class impl;
#ifdef _DEBUG
	static const std::size_t impl_size = 248;
	static const std::size_t impl_align = 8;
#else
	static const std::size_t impl_size = 224;
	static const std::size_t impl_align = 8;
#endif
	pimpl<impl, impl_size, impl_align> m_pimpl;
};