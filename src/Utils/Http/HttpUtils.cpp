#include "HttpUtils.hpp"


inline void extract_header_name_value(const std::string& header, std::string& name, std::string& value)
{
    std::size_t posDelim = header.find_first_of('=');
    name = header.substr(0u, posDelim);
    if (posDelim != std::string::npos)
        value = header.substr(posDelim + 1u, std::string::npos);
};


namespace httputils
{
    std::vector<std::pair<std::string, std::string>> split_headers(const std::string& value)
    {
        std::vector<std::pair<std::string, std::string>> results;
        if (value.empty())
            return results;
        
        std::size_t posBeg = 0u;
        std::size_t posEnd = value.find_first_of(';', posBeg);
        do
        {
            std::string hdr = value.substr(posBeg, posEnd - posBeg);
            if (!hdr.empty())
            {
                std::string hdrName;
                std::string hdrValue;
                extract_header_name_value(hdr, hdrName, hdrValue);

                results.push_back(std::make_pair(hdrName, hdrValue));
            };

            if (posEnd == std::string::npos)
                break;

            posBeg = value.find_first_not_of(' ', posEnd + 1u);
            posEnd = value.find_first_of(';', posBeg);
        } while (true);

        return results;
    };

    
    bool change_request_location(std::string& request, const std::string& location)
    {
        size_t posBegin = request.find("Host:");
        if (posBegin == std::string::npos)
            return false;

        size_t posEnd = request.find("\r\n", posBegin);
        if (posEnd == std::string::npos)
            return false;

        ASSERT(posEnd >= posBegin);
        request.erase(posBegin, posEnd - posBegin);
        request.insert(posBegin, "Host: " + location);

        return true;
    };
} /* namespace httputils */