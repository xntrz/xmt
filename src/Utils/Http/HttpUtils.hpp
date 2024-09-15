#pragma once

namespace httputils
{
    /* splits http header values that has format VALUE=NAME; VALUE=NAME; ... etc */
    std::vector<std::pair<std::string, std::string>> split_headers(const std::string& value);

    /* changes location header in HTTP request to a new specified, returns true if replace success otherwise false */
    bool change_request_location(std::string& request, const std::string& location);
}; /* namespace httputils */