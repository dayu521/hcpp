#ifndef parser_h
#define parser_h

#include <string>
#include <unordered_map>

using std::string_view,std::string;

using headers = std::unordered_map<string, string>;

struct RequestSvcInfo
{
    std::string method_;
    std::string host_;
    std::string port_;
};

bool check_req_line_1(std::string_view svl, RequestSvcInfo &ts, std::string &req_line_new);

bool parser_header(string_view svl, headers &h);

string to_lower(string_view s);

#endif