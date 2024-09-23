//
// Created by 10484 on 24-9-22.
//

#ifndef CURLTOOLS_H
#define CURLTOOLS_H

#include <string>
#include <curl/curl.h>

/**
 * curl 包装类
 */
class CurlTools
{
public:
    explicit CurlTools();
    ~CurlTools();
    CURLcode SetUrl_Port(const std::string &remote_url, int port);
    CURLcode SetUser_Passwd(const std::string &user, const std::string &passwd);
    bool SetUrl_Port_User_Passwd(const std::string &remote_url, int port, const std::string &user, const std::string &passwd);

    CURLcode SetOpt(CURLoption option, ...) const;
    CURLcode Perform() const;

    CURLcode GetInfo(CURLINFO info, ...) const;

    void Cleanup();

private:
    CURL * curl_ = nullptr;
};


#endif //CURLTOOLS_H
