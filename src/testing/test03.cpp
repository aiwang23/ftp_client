//
// Created by 10484 on 24-9-23.
//
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

#undef DISABLE_SSH_AGENT

size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream) //回调函数
{
    curl_off_t nread;
    size_t retcode = fread(ptr, size, nmemb, static_cast<FILE *>(stream));
    nread = (curl_off_t) retcode;
    return retcode;
}

int main(void)
{
    CURL *curl;
    CURLcode res;
    const char *urlkey = "wang:123456"; //服务器用户名密码
    FILE *pSendFile = fopen("ftp-list.txt", "rb");
    fseek(pSendFile, 0L, SEEK_END);
    size_t iFileSize = ftell(pSendFile);
    fseek(pSendFile, 0L, SEEK_SET);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    /*
        const char* remote = "sftp://wang:123456@172.22.254.93:9022/home/wang/download/list.txt";
    const char* filename = "D:/Project/Cpp_/My_Ftp_Client/bin/ftp-list.txt";
    */
    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL,
                         "sftp://wang:123456@172.22.254.93:9022/home/wang/download/list.txt");
        curl_easy_setopt(curl, CURLOPT_USERPWD, urlkey);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(curl, CURLOPT_READDATA, pSendFile);
        curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 0);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, iFileSize);

#ifndef DISABLE_SSH_AGENT
        curl_easy_setopt(curl, CURLOPT_SSH_AUTH_TYPES, CURLSSH_AUTH_PASSWORD);
#endif
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);

        if (CURLE_OK != res)
        {
            fprintf(stderr, "curl told us %d\n", res);
        }
    }
    fclose(pSendFile);
    curl_global_cleanup();
    return 0;
}
