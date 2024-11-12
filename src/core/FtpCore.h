#ifndef FTP_CORE_H
#define FTP_CORE_H

#include <string>
#include <curl/curl.h>

#include "ThreadPool.h"

struct download_file_info;
/**
 * 文件信息
 * @note 文件信息包括权限、链接数、用户、用户组、大小、修改日期、名称
 * drwxr-xr-x    3 root     root         4096 May 29 01:01 home
 */
struct file_info
{
	std::string permissions; // 权限
	std::string link_number; // 链接数
	std::string user;        // 用户
	std::string group;       // 用户组
	std::string size;        // 文件大小
	std::string modify_time; // 最新修改日期
	std::string name;        // 文件名
};

enum file_type { none = -1, file, dir, link};

std::string file_type2str(file_type type);

class FtpCore
{
public:
	FtpCore();

	~FtpCore();

	/**
	 * 登录 查看是否能连接成功
	 * @param remote_url 远程url
	 * @note 远程url格式为 ftp://ip 或 ftps://ip sftp://ip
	 * @param port 端口
	 * @param username 用户名
	 * @param password 密码
	 * @param is_only 如果是单次连接，不记录url port username password
	 * @return ture表示登录成功 false表示登录失败
	 */
	bool Connect(const std::string &remote_url, int port,
	             const std::string &username, const std::string &password, bool is_only = false);

	/**
	 * 无阻塞多线程 下载文件
	 * 可断点续传
	 * @param remote_file 远程文件路径
	 * @note  远程文件路径格式为 /path/test.txt
	 * @param local_file 本地文件路径
	 * @note 本地文件路径格式为 /path/test.txt
	 * @return 状态码
	 */
	std::future<CURLcode> GetFile(const std::string &remote_file, std::string local_file);

	/**
	 * ! TODO
	 * 查看文件信息
	 * @param remote_file 远程文件路径
	 * @return
	 */
	file_info GetFileInfo(const std::string &remote_file) = delete;

	/**
	 * 获取文件列表
	 * @param remote_file 远程文件路径
	 * @return
	 */
	std::vector<file_info> GetDirList(const std::string &remote_file);

	/**
	 * 无阻塞多线程 上传文件
	 * 无断点续传
	 * @param remote_file 远程文件路径
	 * @note  远程文件路径格式为 /path/test.txt
	 * @param local_file 本地文件路径
	 * @note 本地文件路径格式为 /path/test.txt
	 * @return
	 */
	std::future<CURLcode> PutFile(std::string remote_file,
	                              const std::string &local_file);

	/**
	 * 删除文件 文件夹
	 * @note remote_file 要绝对路径
	 *
	 * @param remote_file 要删除的远程文件
	 * @param type @file : 文件, @dir : 文件夹, @file_type
	 * @return
	 */
	CURLcode Delete(std::string remote_file, file_type type);

	/**
	 * 重命名文件 文件夹
	 * @note 要绝对路径
	 * @param remote_file_old 旧名称
	 * @param remote_file_new 新名称
	 * @return
	 */
	CURLcode Rename(std::string remote_file_old, std::string remote_file_new);

	/**
	 *
	 * @param remote_path 远程路径的新文件夹名
	 * @return
	 */
	CURLcode Mkdir(std::string remote_path);

	void set_remote_url(const std::string &url) { remote_url_ = url; }
	void set_port(const int port) { port_ = port; }
	void set_username(const std::string &user) { username_ = user; }
	void set_password(const std::string &passwd) { password_ = passwd; }

	/**
	 *
	 * @return ftp://ip 或 ftps://ip sftp://ip
	 */
	std::string remote_url() const { return remote_url_; }

	std::string username() const { return username_; }
	int port() const { return port_; }

protected:
	// @GetFileCallback 的回调函数
	static size_t WriteCallback(void *ptr, size_t size, size_t nmemb, void *userdata);

	// @GetFile 的回调函数
	CURLcode GetFileCallback(std::string remote_file, std::string local_file,
	                         long timeout);

	// @PutFileCallback 的回调函数
	static size_t ReadCallback(void *ptr, size_t size, size_t nmemb, void *userdata);

	// @PutFile 的回调函数
	CURLcode PutFileCallback(std::string remote_file, std::string local_file,
	                         long timeout);

private:
	std::string remote_url_; // 远程url  格式为 ftp://ip 或 ftps://ip sftp://ip
	std::string username_;   // 用户名
	std::string password_;   // 密码
	bool is_login_ = false;  // 是否登录成功
	int port_ = 21;          // 端口

	ThreadPool download_pool_{3}; // 默认三个下载线程
	ThreadPool upload_pool_{3};   // 默认三个上传线程

	std::vector<download_file_info> download_list_; // 登记一下正在下载的文件是什么 防止下载后重名
	std::vector<download_file_info> upload_list_;   // 登记一下正在上传的文件是什么 防止上传后重名
};


#endif // FTP_CORE_H
