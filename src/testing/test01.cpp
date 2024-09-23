#include "FtpCore.h"
#include <iostream>


int main(int argc, char* argv[])
{
    FtpCore ftp_core;
    bool res = ftp_core.Connect("sftp://172.22.254.93", 9022, "wang", "123456");
    if (res)
    {
        std::cout << "login success" << std::endl;
    }
    else
    {
        std::cout << "login failed" << std::endl;
    }

    auto list = ftp_core.GetDirList("/home/");

    return 0;
}
