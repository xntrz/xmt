#pragma once

#include "HttpStatus.hpp"


class CHttpResponse final
{
private:
	class Impl;

public:
	CHttpResponse(void);
	~CHttpResponse(void);
	void clear(void);
	bool process(const char* data, int dataSize);
	httpstatus::value status(void) const;
	int header_count(const char* hdr) const;
	std::string header_value(const char* hdr) const;
	std::vector<std::string> header_values(const char* hdr) const;
	std::string cookie_value(const char* cookie) const;
	char* body(void) const;
	int body_size(void) const;
	bool is_complete(void) const;
	bool is_keepalive(void) const;

private:
	Impl& impl(void);
	const Impl& impl(void) const;

private:
	Impl* m_pImpl;
};