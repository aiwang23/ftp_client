#include "FtpCore.h"
#include "CurlTools.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

// 队列里下载文件信息
struct download_file_info
{
	std::string name;
	bool is_transmitting = false;
};

typedef download_file_info up_download_file_info;

static bool is_exist(std::vector<up_download_file_info> &_list, const up_download_file_info &info)
{
	for (auto &item: _list)
	{
		if (item.name == info.name
		    && item.is_transmitting == info.is_transmitting)
			return true;
	}
	return false;
}

static bool is_exist(const std::vector<file_info> &_list, const file_info &info)
{
	for (auto &item: _list)
	{
		if (item.name == info.name)
			return true;
	}
	return false;
}


// 字符串切割
static std::vector<std::string> split(const std::string &str, const char c)
{
	std::stringstream ss(str);
	std::string item;
	std::vector<std::string> elems;
	while (std::getline(ss, item, c))
	{
		if (!item.empty())
		{
			elems.push_back(item);
		}
	}
	return elems;
}

std::string file_type2str(const file_type type)
{
	switch (type)
	{
		case file_type::dir:
			return "dir";
		case file_type::file:
			return "file";
		case file_type::link:
			return "link";
		default:
			return {};
	}
}

FtpCore::FtpCore(FtpCoreCallBack *ftp_core_call_back)
{
	curl_global_init(CURL_GLOBAL_ALL); // 初始化全局的CURL库
	download_list_.resize(20);
	upload_list_.resize(20);

	ftp_core_call_back_ = ftp_core_call_back;
}

FtpCore::~FtpCore()
{
	curl_global_cleanup(); // 清理全局的CURL库
}

bool FtpCore::Connect(const std::string &remote_url, int port,
                      const std::string &username, const std::string &password
                      , const bool is_only, const std::string &visit_path)
{
	// 参数校验
	if (remote_url.empty() || port <= 0 || username.empty() || password.empty())
	{
		fprintf(stderr, "[error] remote_url, port, username, password can't be empty!\n");
		return false;
	}
	// url校验
	if (nullptr == strstr(remote_url.c_str(), "ftp://")
	    && nullptr == strstr(remote_url.c_str(), "ftps://")
	    && nullptr == strstr(remote_url.c_str(), "sftp://"))
	{
		fprintf(stderr, "[error] remote_url must start with ftp://, ftps:// or sftp://\n");
		return false;
	}

	is_login_ = false;
	CurlTools curl_tools; // 设置url, port, username, password
	bool ret = curl_tools.SetUrl_Port_User_Passwd(remote_url + visit_path,
	                                              port, username, password);
	if (!ret)
	{
		fprintf(stderr, "[error] set url, port, username, password failed!\n");
		return false;
	}
	curl_tools.SetOpt(CURLOPT_TIMEOUT, 6); // 设置超时时间 6s
	CURLcode res = curl_tools.Perform();   // 执行登录操作
	if (CURLE_OK != res)
	{
		fprintf(stderr, "[error] curl perform failed! res = %d\n", res);
		return false;
	}
	// 登录成功
	if (false == is_only) // 单次连接 不记录
	{
		remote_url_ = remote_url;
		port_ = port;
		username_ = username;
		password_ = password;
	}
	fprintf(stdout, "[info] %s Connect success!\n", remote_url_.c_str());
	fflush(stdout);
	is_login_ = true;
	return true;
}

void FtpCore::GetFile(const std::string &remote_file, std::string local_file,
                      std::shared_ptr<ftp_file_transfer_status> &download_status)
{
	const uint32_t id = ftp_file_transfer_id_.fetch_add(1, std::memory_order_relaxed);
	// 为登录过
	if (!is_login_)
	{
		fprintf(stderr, "");
		std::string msg{"[error] not login!\n"};
		fprintf(stderr, msg.c_str());
		// ftp_result_queue_.enqueue();
		if (nullptr != ftp_core_call_back_)
			ftp_core_call_back_->ResultCallBack(ftp_result{
				id,
				remote_file,
				local_file,
				ftp_transfer_type::download,
				msg,
				CURLE_LOGIN_DENIED
			});
		return;
	}

	while (std::filesystem::exists(local_file)) // 本地有重复的文件
	{
		if (is_exist(download_list_, // 断点续传
		             {local_file, true}))
			break;
		else
			local_file += ".bak";
	}

	download_status = std::make_shared<ftp_file_transfer_status>();
	download_status->id = id;
	// 创建下载任务
	download_pool_.enqueue([=]()
	{
		fprintf(stdout, "[info] download file: %s\n", remote_file.c_str());
		GetFileCallback(remote_file, local_file, 0, download_status);
	});
}


std::vector<file_info> FtpCore::GetDirList(const std::string &remote_file)
{
	if (remote_file.empty())
	{
		fprintf(stderr, "[error] remote_file can't be empty!\n");
		return {};
	}

	CurlTools curl_tools;
	curl_tools.SetUrl_Port_User_Passwd(remote_url_ + "/" + remote_file + "/", port_,
	                                   username_, password_);
	std::string ftpfile_name{"ftp-list.txt"};
	FILE *ftpfile = nullptr;
	ftpfile = fopen(ftpfile_name.c_str(), "wb");

	curl_tools.SetOpt(CURLOPT_WRITEDATA, ftpfile);
	curl_tools.SetOpt(CURLOPT_WRITEFUNCTION, WriteCallback);
	CURLcode res = curl_tools.Perform();
	if (CURLE_OK != res)
	{
		fprintf(stderr, "[error] curl perform failed! err: %s\n",
		        curl_easy_strerror(res));
		if (ftpfile)
			fclose(ftpfile);
		return {};
	}
	curl_tools.Cleanup();
	if (ftpfile)
		fclose(ftpfile);

	std::ifstream in(ftpfile_name);
	std::vector<std::string> lines;
	if (in)
	{
		std::string line;
		while (std::getline(in, line))
			lines.push_back(line);
	}
	in.close();

	std::vector<file_info> file_list;
	for (auto &line: lines)
	{
		// line 大概长这样
		//?类型权限    链接数 用户 属组     大小   日期         文件名|文件夹名
		// -r--r--r-- 1 ftp ftp         99982 Jul 27  2023 LitleCat.jpg
		// 用空格将每一行分割成多个参数
		std::vector<std::string> args = split(line, ' ');
		if (args.size() < 9)
			continue;
		// 日期
		std::string date = args[5] + " " + args[6] + " " + args[7];
		// 文件名
		std::string name = args[8];
		// 将文件名后面的参数（如果有的话）拼接到文件名字符串中
		for (int i = 9; i < args.size(); ++i)
			name += " " + args.at(i);
		file_list.push_back({
			args.at(0), // 权限
			args.at(1), // 链接数
			args.at(2), // 用户
			args.at(3), // 用户组
			args.at(4), // 文件大小
			date,       // 最新修改日期
			name        // 文件名
		});
	}

	fprintf(stdout, "[info] get file list success!\n");
	return file_list;
}

void FtpCore::PutFile(std::string remote_file,
                      const std::string &local_file,
                      std::shared_ptr<ftp_file_transfer_status> &upload_status)
{
	const uint32_t id = ftp_file_transfer_id_.fetch_add(1, std::memory_order_relaxed);
	if (!is_login_)
	{
		std::string msg = "[error] not login!\n";
		fprintf(stderr, msg.c_str());
		ftp_core_call_back_->ResultCallBack(ftp_result{
				id,
				remote_file,
				local_file,
				ftp_transfer_type::upload,
				msg,
				CURLE_LOGIN_DENIED
			});
	}

	auto myfunc = [&]()
	{
		std::filesystem::path path(remote_file);
		std::vector<file_info> list = this->GetDirList(path.parent_path().string());
		file_info info{};
		info.name = path.filename().string();
		return is_exist(list, info);
	};
	while (myfunc())
	{
		if (is_exist(upload_list_,
		             {remote_url_ + "/" + remote_file, true}))
			break;
		else
			remote_file += ".bak";
	}

	upload_status = std::make_shared<ftp_file_transfer_status>();
	upload_status->id = id;

	upload_pool_.enqueue([=]()
	{
		fprintf(stdout, "[info] upload file: %s\n", remote_file.c_str());
		PutFileCallback(remote_file, local_file, 0, upload_status);
	});
}

CURLcode FtpCore::Delete(std::string remote_file, file_type type)
{
	if (!is_login_)
	{
		fprintf(stderr, "[error] not login!\n");
		return CURLE_OPERATION_TIMEOUTED;
	}

	std::string cmd;
	if (nullptr == strstr(remote_url_.c_str(), "sftp://")
	    && nullptr != strstr(remote_url_.c_str(), "ftp://")
	    || nullptr != strstr(remote_url_.c_str(), "ftps://"))
	{
		if (file_type::file == type)
			cmd = "DELE ";
		else if (file_type::dir == type)
			cmd = "RMD  ";
	}
	else if (nullptr != strstr(remote_url_.c_str(), "sftp://"))
	{
		if (file_type::file == type)
			cmd = "rm ";
		else if (file_type::dir == type)
			cmd = "rmdir ";
	}
	cmd += remote_file;
	curl_slist *slist = nullptr;
	slist = curl_slist_append(slist, cmd.c_str());

	CurlTools curl_tools;
	curl_tools.SetUrl_Port_User_Passwd(remote_url_, port_, username_, password_);
	curl_tools.SetOpt(CURLOPT_NOBODY, 1);
	curl_tools.SetOpt(CURLOPT_POSTQUOTE, slist);
	CURLcode res = curl_tools.Perform();
	if (CURLE_OK == res)
	{
		fprintf(stdout, "[info] delete %s success!\n", remote_file.c_str());
	}
	else
	{
		fprintf(stderr, "[error] delete %s failed!\n", remote_file.c_str());
	}

	return res;
}

CURLcode FtpCore::Rename(std::string remote_file_old, std::string remote_file_new)
{
	if (!is_login_)
	{
		fprintf(stderr, "[error] not login!\n");
		return CURLE_OPERATION_TIMEOUTED;
	}

	// 新文件名已经存在 就退出
	std::filesystem::path path(remote_file_new);
	std::vector<file_info> infos = GetDirList(path.parent_path().string());
	file_info info{};
	info.name = path.filename().string();
	if (is_exist(infos, info))
	{
		fprintf(stderr, "[error] %s is exist!\n", remote_file_new.c_str());
		return CURLE_REMOTE_FILE_EXISTS;
	}

	curl_slist *slist = nullptr;
	std::string cmd;
	if (nullptr == strstr(remote_url_.c_str(), "sftp://")
	    && nullptr != strstr(remote_url_.c_str(), "ftp://")
	    || nullptr != strstr(remote_url_.c_str(), "ftps://"))
	{
		cmd = "RNFR " + remote_file_old;
		slist = curl_slist_append(slist, cmd.c_str());
		cmd = "RNTO " + remote_file_new;
		slist = curl_slist_append(slist, cmd.c_str());
	}
	else if (nullptr != strstr(remote_url_.c_str(), "sftp://"))
	{
		cmd = "rename " + remote_file_old + " " + remote_file_new;
		slist = curl_slist_append(slist, cmd.c_str());
	}

	CurlTools curl_tools;
	curl_tools.SetUrl_Port_User_Passwd(remote_url_, port_, username_, password_);
	curl_tools.SetOpt(CURLOPT_NOBODY, 1);
	curl_tools.SetOpt(CURLOPT_POSTQUOTE, slist);
	CURLcode res = curl_tools.Perform();
	if (CURLE_OK == res)
	{
		fprintf(stdout, "[info] rename %s to %s success!\n",
		        remote_file_old.c_str(), remote_file_new.c_str());
	}
	else
	{
		fprintf(stderr, "[error] rename %s to %s failed!\n",
		        remote_file_old.c_str(), remote_file_new.c_str());
	}

	return res;
}

CURLcode FtpCore::Mkdir(std::string remote_path)
{
	if (!is_login_)
	{
		fprintf(stderr, "[error] not login!\n");
		return CURLE_OPERATION_TIMEOUTED;
	}

	// 新文件夹 已经存在 就退出
	std::filesystem::path path(remote_path);
	std::vector<file_info> infos = GetDirList(path.parent_path().string());
	file_info info{};
	info.name = path.filename().string();
	if (is_exist(infos, info))
	{
		fprintf(stderr, "[error] %s is exist!\n", remote_path.c_str());
		return CURLE_REMOTE_FILE_EXISTS;
	}

	std::string cmd;
	if (nullptr == strstr(remote_url_.c_str(), "sftp://")
	    && nullptr != strstr(remote_url_.c_str(), "ftp://")
	    || nullptr != strstr(remote_url_.c_str(), "ftps://"))
	{
		cmd = "MKD " + remote_path;
	}
	else if (nullptr != strstr(remote_url_.c_str(), "sftp://"))
	{
		cmd = "mkdir " + remote_path;
	}

	curl_slist *slist = nullptr;
	slist = curl_slist_append(slist, cmd.c_str());

	CurlTools curl_tools;
	curl_tools.SetUrl_Port_User_Passwd(remote_url_, port_, username_, password_);
	curl_tools.SetOpt(CURLOPT_NOBODY, 1);
	curl_tools.SetOpt(CURLOPT_POSTQUOTE, slist);
	CURLcode res = curl_tools.Perform();
	if (CURLE_OK == res)
	{
		fprintf(stdout, "[info] mkdir %s success!\n", remote_path.c_str());
	}
	else
	{
		fprintf(stderr, "[error] mkdir %s failed!\n", remote_path.c_str());
	}

	return res;
}

size_t FtpCore::WriteCallback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	return fwrite(ptr, size, nmemb, static_cast<FILE *>(userdata));
}

void FtpCore::GetFileCallback(std::string remote_file, std::string local_file, long timeout,
                              std::shared_ptr<ftp_file_transfer_status> download_status)
{
	FILE *f = nullptr;
	curl_off_t local_file_len = -1;
	CURLcode res = CURLE_GOT_NOTHING;
	struct stat file_info{};
	int use_resume = 0;
	const uint32_t id = download_status->id;

	// 如果本地有同名文件，则使用断点续传
	if (stat(local_file.c_str(), &file_info) == 0)
	{
		local_file_len = file_info.st_size;
		use_resume = 1;
	}
	f = fopen(local_file.c_str(), "ab+"); // TODO
	if (nullptr == f)
	{
		std::string msg{
			"[error] open local file failed! "
		};
		fprintf(stderr, "%d %s, file: %s\n", __LINE__, msg.c_str(), local_file.c_str());
		res = CURLE_WRITE_ERROR;
		// ftp_result_queue_.enqueue();
		ftp_core_call_back_->ResultCallBack(ftp_result{
			id,
			remote_file,
			local_file,
			ftp_transfer_type::download,
			msg,
			res
		});
		return;
	}

	int idx = -1;
	for (int i = 0; i < download_list_.size(); ++i) // 登记到空位置里
	{
		if (download_list_[i].name == "" && download_list_[i].is_transmitting == false)
		{
			idx = i;
			download_list_[i].name = local_file;
			download_list_[i].is_transmitting = true;
			break;
		}
	}
	if (-1 == idx)
	{
		download_list_.push_back({local_file, true});;
	}

	CurlTools curl_tools;
	// 设置url，端口，用户名，密码
	bool ret = curl_tools.SetUrl_Port_User_Passwd(remote_url_ + "/" + remote_file,
	                                              port_, username_, password_);
	if (!ret)
	{
		// 设置失败
		std::string msg = {"[error] set url, port, username, password failed!\n"};
		fprintf(stderr, "%d %s\n",__LINE__, msg.c_str());
		res = CURLE_URL_MALFORMAT;
		// 将结果放入队列
		// ftp_result_queue_.enqueue();
		ftp_core_call_back_->ResultCallBack(ftp_result{
			id,
			remote_file,
			local_file,
			ftp_transfer_type::download,
			msg,
			res
		});
		return;
	}

	curl_tools.SetOpt(CURLOPT_CONNECTTIMEOUT, timeout);
	curl_tools.SetOpt(CURLOPT_RESUME_FROM_LARGE, use_resume ? local_file_len : 0);
	curl_tools.SetOpt(CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_tools.SetOpt(CURLOPT_WRITEDATA, f);

	// 设置进度条
	curl_tools.SetOpt(CURLOPT_NOPROGRESS, 0L);
	curl_tools.SetOpt(CURLOPT_XFERINFOFUNCTION, ProgressCallback);
	curl_tools.SetOpt(CURLOPT_XFERINFODATA, download_status.get());
	curl_tools.SetOpt(CURLOPT_VERBOSE, 1L);

	std::string curl_msg;
	res = curl_tools.Perform();
	if (CURLE_OK != res)
	{
		curl_msg = std::string{"[error] curl perform failed! err: "} +
		           curl_easy_strerror(res);
		fprintf(stderr, "%d %s\n", __LINE__, curl_msg.c_str());
	}
	else
	{
		curl_msg = std::string{"[info] download OK"};
		fprintf(stderr, "%d %s\n", __LINE__, curl_msg.c_str());
		download_list_.at(idx).name = "";               // 下载完成，清空
		download_list_.at(idx).is_transmitting = false; // 下载完成
	}
	if (f)
		fclose(f);

	// 将结果放入队列
	// ftp_result_queue_.enqueue();

	// 结果给qt
	ftp_core_call_back_->ResultCallBack(ftp_result{
		id,
		remote_file,
		local_file,
		ftp_transfer_type::download,
		curl_msg,
		res
	});
}

size_t FtpCore::ReadCallback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	return fread(ptr, size, nmemb, static_cast<FILE *>(userdata));
}


void FtpCore::PutFileCallback(std::string remote_file, std::string local_file, long timeout,
	std::shared_ptr<ftp_file_transfer_status> upload_status)
{
	const uint32_t id = upload_status->id;
	int idx = -1;
	for (int i = 0; i < upload_list_.size(); ++i) // 登记到空位置里
	{
		if (upload_list_[i].name == "" && upload_list_[i].is_transmitting == false)
		{
			idx = i;
			upload_list_[i].name = remote_url_ + "/" + remote_file;
			upload_list_[i].is_transmitting = true;
			break;
		}
	}
	if (-1 == idx)
	{
		upload_list_.push_back({remote_file, true});;
	}

	CurlTools curl_tools;
	CURLcode res;
	FILE *sendFile = nullptr;
#if _WIN32 // win
	fopen_s(&sendFile, local_file.c_str(), "rb");
#else // linux
    fopen( local_file.c_str(), "rb");
#endif
	fseek(sendFile, 0L, SEEK_END);
	size_t sendFileSize = ftell(sendFile);
	fseek(sendFile, 0L, SEEK_SET);

	curl_tools.SetUrl_Port_User_Passwd(
		(remote_url_ + "/" + remote_file), port_,
		username_, password_
	);
	curl_tools.SetOpt(CURLOPT_READFUNCTION, ReadCallback);
	curl_tools.SetOpt(CURLOPT_READDATA, sendFile);
	curl_tools.SetOpt(CURLOPT_FTP_CREATE_MISSING_DIRS, 0);
	curl_tools.SetOpt(CURLOPT_UPLOAD, 1);
	curl_tools.SetOpt(CURLOPT_INFILESIZE, sendFileSize);
	curl_tools.SetOpt(CURLOPT_SSH_AUTH_TYPES, CURLSSH_AUTH_PASSWORD);

	curl_tools.SetOpt(CURLOPT_NOPROGRESS, 0L);
	curl_tools.SetOpt(CURLOPT_XFERINFOFUNCTION, ProgressCallback);
	curl_tools.SetOpt(CURLOPT_XFERINFODATA, upload_status.get());
	curl_tools.SetOpt(CURLOPT_VERBOSE, 1L);

	std::string curl_msg;
	res = curl_tools.Perform();
	if (CURLE_OK != res)
	{
		curl_msg = std::string{"[error] curl perform failed! err: "} +
				curl_easy_strerror(res);
		fprintf(stderr, curl_msg.c_str());
	}
	else
	{
		curl_msg = std::string{"[info] upload OK\n"};
		fprintf(stdout, curl_msg.c_str());
		upload_list_.at(idx).name = "";               // 下载完成，清空
		upload_list_.at(idx).is_transmitting = false; // 下载完成
	}

	if (sendFile)
		fclose(sendFile);

	ftp_core_call_back_->ResultCallBack(ftp_result{
		id,
		remote_file,
		local_file,
		ftp_transfer_type::upload,
		curl_msg,
		res
	});
}

int FtpCore::ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	double progress;
	ftp_file_transfer_status *p_;
	if (nullptr == clientp)
		return 1;

	p_ = static_cast<ftp_file_transfer_status *>(clientp);

	// 如果停止了，则返回 1 表示停止下载
	if (p_->is_stop)
		return 1;

	if (dltotal != 0)
		p_->progress = static_cast<double>(dlnow) / static_cast<double>(dltotal) * 100.0;

	return 0; // 返回 0 表示继续下载
}
