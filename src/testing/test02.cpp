//
// Created by 10484 on 24-9-22.
//
#include "FtpCore.h"


int main()
{
    FtpCore ftpCore;
    ftpCore.Connect("sftp://172.22.254.93", 9022, "wang", "123456");
    // auto ret = ftpCore.GetFile(
    //     "/home/wang/download/mediamtx_v1.8.4_linux_amd64.tar.gz",
    //     "mediamtx_v1.8.4_linux_amd64.tar.gz"
    // );
    // // std::this_thread::sleep_for(std::chrono::seconds(3));
    // if (ret.get() == CURLE_OK)
    // {
    //     fprintf(stdout,"download success\n");
    // }
    // else
    // {
    //     fprintf(stderr,"download failed\n");
    // }
    // auto ret = ftpCore.PutFile("/home/wang/download/ftp-list.txt", "ftp-list.txt");
    // if (ret.get() == CURLE_OK)
    // {
    //     fprintf(stdout, "upload success\n");
    // }
    // else
    // {
    //     fprintf(stderr, "upload failed\n");
    // }
    // auto res= ftpCore.Delete("/home/wang/download/ok/", dir);
    // if (res == CURLE_OK)
    // {
    //     fprintf(stdout, "delete success\n");
    // }
    // else
    // {
    //     fprintf(stderr, "delete failed\n");
    // }
    // auto res = ftpCore.Rename("/home/wang/download/nb.txt", "/home/wang/download/ok.txt");
    // if (res == CURLE_OK)
    // {
    //     fprintf(stdout, "rename success\n");
    // }
    // else
    // {
    //     fprintf(stderr, "rename failed\n");
    // }
    auto res = ftpCore.Mkdir("/home/wang/download/nb/");
    if (res == CURLE_OK)
    {
        fprintf(stdout, "mkdir success\n");
    }
    else
    {
        fprintf(stderr, "mkdir failed\n");
    }
    return 0;
}
