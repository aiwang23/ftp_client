//
// Created by 10484 on 24-12-9.
//

#ifndef FTPCORECALLBACK_H
#define FTPCORECALLBACK_H

enum class ftp_transfer_type { none = -1, upload, download };

// 一个ftp_file_transfer_status 和一个ftp_result 对应
struct ftp_result
{
	uint32_t id = -1;              // 唯一标识
	std::string remote_file;  // 远程文件名
	std::string local_file;   // 本地文件名
	ftp_transfer_type transfer_t; // 上传or下载
	std::string msg;          // 错误信息
	CURLcode code;            // curl返回码
};

// 一个ftp_file_transfer_status 和一个ftp_result 对应
struct ftp_file_transfer_status
{
	uint32_t id = -1;              // 唯一标识
	std::string remote_file;  // 远程文件名
	std::string local_file;   // 本地文件名
	ftp_transfer_type transfer_t; // 上传or下载
	double progress{};        // 上传or下载 进度
	bool is_stop = false;
};

class FtpCoreCallBack
{
public:
	virtual void DownloadProgressCallBack(ftp_file_transfer_status status) = 0;
	virtual void UploadProgressCallBack(ftp_file_transfer_status status) = 0;
	virtual void ResultCallBack(ftp_result rs) = 0;
};


#endif //FTPCORECALLBACK_H
