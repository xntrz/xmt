#include "Base64.hpp"

#include "cppbase64/base64.h"


/*static*/ std::string CBase64::Encode(const std::string& data)
{
	return base64_encode(data);
};


/*static*/ std::string CBase64::Decode(const std::string& data)
{
	return base64_decode(data);
};