//
// Created by 10484 on 24-9-22.
//

#include "CurlTools.h"

CurlTools::CurlTools()
{
    curl_ = curl_easy_init();
}

CurlTools::~CurlTools()
{
    if (curl_)
        curl_easy_cleanup(curl_);
}

CURLcode CurlTools::SetUrl_Port(const std::string &remote_url, int port)
{
    CURLcode res = curl_easy_setopt(curl_, CURLOPT_URL, remote_url.c_str());
    if (res != CURLE_OK)
    {
        fprintf(stderr, "[error] curl_easy_setopt failed: %s\n",
                curl_easy_strerror(res));
        return res;
    }
    res = curl_easy_setopt(curl_, CURLOPT_PORT, port);
    if (res != CURLE_OK)
    {
        fprintf(stderr, "[error] curl_easy_setopt failed: %s\n",
                curl_easy_strerror(res));
        return res;
    }
    return res;
}

CURLcode CurlTools::SetUser_Passwd(const std::string &user, const std::string &passwd)
{
    CURLcode res = curl_easy_setopt(curl_,
                                    CURLOPT_USERPWD, (user + ":" + passwd).c_str());
    if (res != CURLE_OK)
    {
        fprintf(stderr, "[error] curl_easy_setopt failed: %s\n",
                curl_easy_strerror(res));
        return res;
    }
    return res;
}

bool CurlTools::SetUrl_Port_User_Passwd(const std::string &remote_url, int port, const std::string &user,
                                        const std::string &passwd)
{
    return (SetUrl_Port(remote_url, port) == CURLE_OK)
           && (SetUser_Passwd(user, passwd) == CURLE_OK);
}

CURLcode CurlTools::SetOpt(const CURLoption option, ...) const
{
    return curl_easy_setopt(curl_, option);
}

CURLcode CurlTools::Perform() const
{
    CURLcode res = curl_easy_perform(curl_);
    if (res != CURLE_OK)
    {
        fprintf(stderr, "[error] curl_easy_perform failed: %s\n",
                curl_easy_strerror(res));
        return res;
    }
    return res;
}

CURLcode CurlTools::GetInfo(CURLINFO info, ...) const
{
    return curl_easy_getinfo(curl_, info);
}

void CurlTools::Cleanup()
{
    if (curl_)
        curl_easy_cleanup(curl_);
}
