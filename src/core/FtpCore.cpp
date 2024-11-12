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

static bool is_exist(std::vector<up_download_file_info> &_list, up_download_file_info info)
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

FtpCore::FtpCore()
{
	curl_global_init(CURL_GLOBAL_ALL); // 初始化全局的CURL库
	download_list_.resize(20);
	upload_list_.resize(20);
}

FtpCore::~FtpCore()
{
	curl_global_cleanup(); // 清理全局的CURL库
}

bool FtpCore::Connect(const std::string &remote_url, int port,
                      const std::string &username, const std::string &password
                      , bool is_only)
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
	bool ret = curl_tools.SetUrl_Port_User_Passwd(remote_url,
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

std::future<CURLcode> FtpCore::GetFile(const std::string &remote_file, std::string local_file)
{
	// 为登录过
	if (!is_login_)
	{
		fprintf(stderr, "[error] not login!\n");
		return {};
	}

	while (std::filesystem::exists(local_file)) // 本地有重复的文件
	{
		if (is_exist(download_list_, // 断点续传
		             {local_file, true}))
			break;
		else
			local_file += ".bak";
	}

	// 创建下载任务
	return download_pool_.enqueue([=]()
	{
		fprintf(stdout, "[info] download file: %s\n", remote_file.c_str());
		return GetFileCallback(remote_file, local_file, 0);
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

std::future<CURLcode> FtpCore::PutFile(std::string remote_file,
                                       const std::string &local_file)
{
	if (!is_login_)
	{
		fprintf(stderr, "[error] not login!\n");
		return {};
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

	return upload_pool_.enqueue([=]()
	{
		fprintf(stdout, "[info] upload file: %s\n", remote_file.c_str());
		return PutFileCallback(remote_file, local_file, 0);
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

CURLcode FtpCore::GetFileCallback(std::string remote_file, std::string local_file, long timeout)
{
	FILE *f = nullptr;
	curl_off_t local_file_len = -1;
	CURLcode res = CURLE_GOT_NOTHING;
	struct stat file_info{};
	int use_resume = 0;
	// 如果本地有同名文件，则使用断点续传
	if (stat(local_file.c_str(), &file_info) == 0)
	{
		local_file_len = file_info.st_size;
		use_resume = 1;
	}
	f = fopen(local_file.c_str(), "ab+"); // TODO
	if (nullptr == f)
	{
		fprintf(stderr, "[error] open local file failed! local_file = %s\n",
		        local_file.c_str());

		return CURLE_WRITE_ERROR;
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
	bool ret = curl_tools.SetUrl_Port_User_Passwd(remote_url_ + "/" + remote_file,
	                                              port_, username_, password_);
	if (!ret)
	{
		fprintf(stderr, "[error] set url, port, username, password failed!\n");
		return CURLE_URL_MALFORMAT;
	}

	curl_tools.SetOpt(CURLOPT_CONNECTTIMEOUT, timeout);
	curl_tools.SetOpt(CURLOPT_RESUME_FROM_LARGE, use_resume ? local_file_len : 0);
	curl_tools.SetOpt(CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_tools.SetOpt(CURLOPT_WRITEDATA, f);

	curl_tools.SetOpt(CURLOPT_NOPROGRESS, 1L);
	curl_tools.SetOpt(CURLOPT_VERBOSE, 1L);
	res = curl_tools.Perform();
	if (CURLE_OK != res)
	{
		fprintf(stderr, "[error] curl perform failed! err:%s\n",
		        curl_easy_strerror(res));
	}
	else
	{
		fprintf(stdout, "[info] download %s OK\n", remote_file.c_str());
		download_list_.at(idx).name = "";               // 下载完成，清空
		download_list_.at(idx).is_transmitting = false; // 下载完成
	}
	if (f)
		fclose(f);
	return res;
}

size_t FtpCore::ReadCallback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	return fread(ptr, size, nmemb, static_cast<FILE *>(userdata));
}


CURLcode FtpCore::PutFileCallback(std::string remote_file, std::string local_file, long timeout)
{
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
	curl_tools.SetOpt(CURLOPT_VERBOSE, 1L);
	res = curl_tools.Perform();
	if (CURLE_OK != res)
	{
		fprintf(stderr, "[error] curl perform failed! err:%s\n",
		        curl_easy_strerror(res));
	}
	else
	{
		fprintf(stdout, "[info] upload %s OK\n", remote_file.c_str());
		upload_list_.at(idx).name = "";               // 下载完成，清空
		upload_list_.at(idx).is_transmitting = false; // 下载完成
	}

	return res;
}
