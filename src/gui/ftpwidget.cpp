//
// Created by 10484 on 24-9-24.
//

// You may need to build the project (run Qt uic code generator) to get "ui_FtpWidget.h" resolved

#include "ftpwidget.h"
#include "ui_FtpWidget.h"
#include <string>
#include <QMessageBox>
#include <thread>

#include <QtSvg/QSvgRenderer>
#include <QPainter>

#include <QMenu>
#include <QFileDialog>
#include <filesystem>

#include <QInputDialog>
#include <QIcon>


FtpWidget::FtpWidget(QWidget *parent) : QWidget(parent), ui(new Ui::FtpWidget)
{
	ui->setupUi(this);
	ftp_core_ = std::make_unique<FtpCore>();
	svg_renderer_ = new QSvgRenderer();

	this->setWindowTitle(QString{"FileMover"});

	init_resource();          // 初始化资源
	init_connect_sig_slots(); // 初始化信号与槽
	init_file_list();         // 初始化文件列表
	init_memu();              // 初始化右键菜单
	init_icon();              // 初始化图标
}

FtpWidget::~FtpWidget()
{
	if (p_interrupt_login_)
		delete p_interrupt_login_;
	if (ui)
		delete ui;
	if (svg_renderer_)
		delete svg_renderer_;
}

void FtpWidget::init_connect_sig_slots()
{
	connect(ui->pushButton_start, &QPushButton::clicked, this,
	        &FtpWidget::on_pushButton_start); // 点击登录按钮，尝试登录
	connect(ui->pushButton_cancel_login, &QPushButton::clicked, this,
	        &FtpWidget::on_pushButton_cancel_login); // 点击取消登录按钮

	connect(this, &FtpWidget::sig_connect_OK, this, [this]()
	{
		/* 登陆成功 跳转到工作界面 */
		ui->stackedWidget->setCurrentIndex(2);
		QMessageBox::information(nullptr, "登录成功", "登陆成功!!");
	});
	connect(this, &FtpWidget::sig_connect_failed, this, [this]()
	{
		/* 登陆失败 跳转到登录界面 */
		ui->stackedWidget->setCurrentIndex(0);
		if (!(*p_interrupt_login_))
			QMessageBox::warning(nullptr, "登录失败", "请重新检查");
	});

	connect(this, &FtpWidget::sig_connect_OK, this,
	        &FtpWidget::on_pushButton_refresh); // 登录成功 刷新文件列表
	connect(ui->pushButton_refresh, &QPushButton::clicked, this,
	        &FtpWidget::on_pushButton_refresh); // 点击刷新按钮 刷新文件列表
	connect(this, &FtpWidget::sig_connect_OK, this, [this]()
	{
		/* 登录成功 显示当前路径 显示远程主机 用户名 端口 */
		ui->lineEdit_url_working->setText(remote_path_.c_str());
		std::string url = ftp_core_->remote_url();
		std::string user = ftp_core_->username();
		int port = ftp_core_->port();
		QString text = QString("%0:%1").arg(url.c_str()).arg(port);
		QString remote_url = text.replace("://", QString("://%0@").arg(user.c_str()));
		ui->label_remote_url->setText(remote_url);
	});

	connect(ui->lineEdit_url_working, &QLineEdit::returnPressed, this,
	        &FtpWidget::on_lineEdit_url_working); // 输入路径后回车，跳转到指定路径

	connect(ui->pushButton_cdup, &QPushButton::clicked, this,
	        &FtpWidget::on_pushButton_cdup); // 点击返回上一级目录

	connect(ui->tableWidget, &QTableWidget::cellDoubleClicked, this,
	        &FtpWidget::on_tableWidget_cellDoubleClicked); // 双击后跳转

	connect(this, &FtpWidget::sig_put_result, this, [this](const QString &msg)
	{
		/* 上传完成后，会调用此函数来报告结果 */
		QMessageBox::information(this, "上传结果", msg);
	});
	connect(this, &FtpWidget::sig_get_result, this, [this](const QString &msg)
	{
		/* 下载完成后，会调用此函数来报告结果 */
		QMessageBox::information(this, "下载结果", msg);
	});

	connect(ui->tableWidget, &QTableWidget::cellClicked, this,
	        &FtpWidget::on_tableWidget_cellClicked); // 单击 更新右方详细消息窗口
}

void FtpWidget::init_icon()
{
	// 设置图标
	auto m_func = [this](const QString &svg_path, const int w, const int h)
	{
		svg_renderer_->load(svg_path);
		QPixmap pixmap(w, h);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		svg_renderer_->render(&painter);
		return std::move(QIcon{pixmap});
	};

	// 设置窗口图标
	this->setWindowIcon(m_func(":/img/next.svg", 100, 100));

	// 设置 刷新按钮图标
	ui->pushButton_refresh->setIcon(m_func(":/img/refresh.svg", 100, 100));

	// 设置 返回上一级目录按钮图标
	ui->pushButton_cdup->setIcon(m_func(":/img/up.svg", 100, 100));
}

void FtpWidget::init_resource()
{
	// 加载svg图像
	QString strPath = ":/img/loader.svg";
	svg_renderer_->load(strPath);

	int s = min(ui->label_loading->size().width(),
	            ui->label_loading->size().height());
	QPixmap pixmap(s, s);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	svg_renderer_->render(&painter);

	ui->label_loading->setPixmap(pixmap);
	ui->label_loading->setAlignment(Qt::AlignHCenter);
}

void FtpWidget::init_file_list()
{
	// 列表只能有对应的7项 权限 链接数 属组 属主...
	ui->tableWidget->setColumnCount(7);
	// 设置列表表头
	// ui->tableWidget->setHorizontalHeaderLabels({"权限", "链接数", "用户", "用户组", "大小", "日期", "文件名"});
	ui->tableWidget->setHorizontalHeaderLabels({"名称", "大小", "修改日期", "权限", "用户", "用户组", "链接数"});

	// 选中单个目标
	ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);

	// 名称列开始时 宽点
	ui->tableWidget->setColumnWidth(0, 150);
}

void FtpWidget::file_list_clear()
{
	while (ui->tableWidget->rowCount() > 0)
		ui->tableWidget->removeRow(0);
}

void FtpWidget::file_list_add(std::vector<file_info> &file_list)
{
	file_info current_directory; // .
	file_info parent_directory;  // ..

	for (auto &file_info: file_list)
	{
		// 过滤掉 "." ".."
		if (file_info.name == std::string{"."} &&
		    current_directory.name.empty())
		{
			current_directory = file_info;
			continue;
		}
		else if (file_info.name == std::string{".."} &&
		         parent_directory.name.empty())
		{
			parent_directory = file_info;
			continue;
		}

		int new_row = ui->tableWidget->rowCount();
		ui->tableWidget->insertRow(new_row);

		file_type file_t;
		if (char t = file_info.permissions[0];
			t == 'd')
			// 文件夹
			file_t = file_type::dir;
		else if (t == 'l')
			// 软链接
			file_t = file_type::link;
		else
			// 文件
			file_t = file_type::file;

		file_list_item_add(new_row, 0, file_info.name, file_t);
		file_list_item_add(new_row, 1, file_info.size);
		file_list_item_add(new_row, 2, file_info.modify_time);
		file_list_item_add(new_row, 3, file_info.permissions);
		file_list_item_add(new_row, 4, file_info.user);
		file_list_item_add(new_row, 5, file_info.group);
		file_list_item_add(new_row, 6, file_info.link_number);
	}

	// 添加 "." ".."
	if ((not current_directory.name.empty()) and
	    (not parent_directory.name.empty()))
	{
		ui->tableWidget->insertRow(0);
		file_list_item_add(0, 0, current_directory.name, file_type::dir);
		file_list_item_add(0, 1, current_directory.size);
		file_list_item_add(0, 2, current_directory.modify_time);
		file_list_item_add(0, 3, current_directory.permissions);
		file_list_item_add(0, 4, current_directory.user);
		file_list_item_add(0, 5, current_directory.group);
		file_list_item_add(0, 6, current_directory.link_number);

		ui->tableWidget->insertRow(1);
		file_list_item_add(1, 0, parent_directory.name, file_type::dir);
		file_list_item_add(1, 1, parent_directory.size);
		file_list_item_add(1, 2, parent_directory.modify_time);
		file_list_item_add(1, 3, parent_directory.permissions);
		file_list_item_add(1, 4, parent_directory.user);
		file_list_item_add(1, 5, parent_directory.group);
		file_list_item_add(1, 6, parent_directory.link_number);
	}
}

void FtpWidget::file_list_item_add(int row, int idx, const std::string &item, file_type type)
{
	ui->tableWidget->setItem(row, idx, new QTableWidgetItem(item.c_str()));
	// Qt::ItemIsSelectable：项目可以被选中。
	// Qt::ItemIsUserCheckable：项目可以被用户勾选或取消勾选。
	// Qt::ItemIsEnabled：项目是可交互的。
	ui->tableWidget->item(row, idx)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);

	// ==none 不用设置图标
	if (type == none)
		return;

	QString imgPath;
	switch (type)
	{
		case file_type::dir: // 文件夹
			imgPath = ":/img/folder.svg";
			break;
		case file_type::link: // 软链接
			imgPath = ":/img/link.svg";
			break;
		case file_type::file: // 文件
			imgPath = ":/img/file.svg";
			break;
		default:
			return;
	}

	// 设置图标
	QIcon item_icon;
	svg_renderer_->load(imgPath);
	int s = min(
		ui->tableWidget->size().width(),
		ui->tableWidget->size().height()
	);
	QPixmap pixmap(s, s);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	svg_renderer_->render(&painter);
	item_icon.addPixmap(pixmap);

	ui->tableWidget->item(row, idx)->setIcon(item_icon);
}

std::string FtpWidget::getParentDirectory(const std::string &path)
{
	size_t lastSlashPos = path.rfind('/');

	// 如果路径中没有'/'，返回"./"表示当前目录
	if (lastSlashPos == std::string::npos)
	{
		return "./";
	}

	// 截取最后一个'/'之前的部分
	std::string parentDir = path.substr(0, lastSlashPos);

	// 如果最后一个'/'是路径的最后一个字符，我们需要去掉它
	if (lastSlashPos == path.length() - 1)
	{
		// 再找前一个'/'，如果不存在，返回"./"，否则截取到那个'/'为止
		size_t secondLastSlashPos = path.rfind('/', lastSlashPos - 1);
		if (secondLastSlashPos == std::string::npos)
		{
			return "./";
		}
		else
		{
			parentDir = path.substr(0, secondLastSlashPos);
		}
	}

	// 添加斜杠以确保返回的上级目录以斜杠结尾
	parentDir += "/";

	return parentDir;
}

void FtpWidget::init_memu()
{
	QMenu *menu = new QMenu;
	QAction *cdupAction = menu->addAction("返回上级");
	QAction *refAction = menu->addAction("刷新");
	menu->addSeparator();
	QAction *putAction = menu->addAction("上传");
	QAction *getAction = menu->addAction("下载");
	menu->addSeparator();
	QAction *delAction = menu->addAction("删除");
	QAction *renameAction = menu->addAction("重命名");
	menu->addSeparator();
	QAction *mkdirAction = menu->addAction("新建文件夹");
	// 鼠标右键发出 customContextMenuRequested 信号
	ui->tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->tableWidget, &QTableWidget::customContextMenuRequested, [=](const QPoint &pos)
	{
		// 打印鼠标在列表的哪一行
		qDebug() << ui->tableWidget->currentRow();
		// 菜单在右键的地方显示
		// viewport 返回局部坐标 mapToGlobal 局部坐标转全局坐标
		menu->exec(ui->tableWidget->viewport()->mapToGlobal(pos));
	});

	connect(cdupAction, &QAction::triggered, this, &FtpWidget::on_pushButton_cdup);
	connect(refAction, &QAction::triggered, this, &FtpWidget::on_pushButton_refresh);
	connect(putAction, &QAction::triggered, this, &FtpWidget::on_putAction);
	connect(getAction, &QAction::triggered, this, &FtpWidget::on_getAction);
	connect(delAction, &QAction::triggered, this, &FtpWidget::on_delAction);
	connect(renameAction, &QAction::triggered, this, &FtpWidget::on_renameAction);
	connect(mkdirAction, &QAction::triggered, this, &FtpWidget::on_mkdirAction);
}

void FtpWidget::update_groupBox_info(const QString &name, const file_type file_t, const int byte, const QString &time)
{
	// 设置文件名称
	ui->groupBox_info->setTitle(name);

	QString imgPath;
	switch (file_t)
	{
		case file_type::dir: // 文件夹
			imgPath = ":/img/folder.svg";
			break;
		case file_type::link: // 软链接
			imgPath = ":/img/link.svg";
			break;
		case file_type::file: // 文件
			imgPath = ":/img/file.svg";
			break;
		default:
			return;
	}

	// 加载svg
	svg_renderer_->load(imgPath);
	int s = min(ui->label_file_image->size().width(),
	            ui->label_file_image->size().height());
	QPixmap pixmap(s, s);
	pixmap.fill(Qt::transparent);
	// 绘制svg
	QPainter painter(&pixmap);
	svg_renderer_->render(&painter);
	// 设置图片
	ui->label_file_image->setPixmap(pixmap);
	ui->label_file_image->setAlignment(Qt::AlignHCenter);

	// 文件类型
	const QString type_name = file_type2str(file_t).c_str();
	ui->label_type->setText(QString("file_type: %0").arg(type_name));

	// 文件大小
	ui->label_size->setText(QString{"file_size: %0 byte"}.arg(byte));

	// 文件最新修改时间
	ui->label_time->setText(QString{"file_time: %0"}.arg(time));
}

void FtpWidget::on_pushButton_start()
{
	std::string ip = ui->lineEdit_ip->text().toStdString();
	int port = ui->lineEdit_port->text().toInt();
	std::string user = ui->lineEdit_user->text().toStdString();
	std::string passwd = ui->lineEdit_passwd->text().toStdString();
	std::string connect_type = ui->comboBox_connect->currentText().toStdString();

	// 换到等待界面
	ui->stackedWidget->setCurrentIndex(1);

	// ftp://ip
	std::string url = connect_type + "://" + ip;
	*p_interrupt_login_ = false;
	auto f = [=]()
	{
		bool ret = ftp_core_->Connect(url, port, user, passwd);
		if (ret && !(*p_interrupt_login_))
			// 跳转到工作界面
			emit sig_connect_OK();
		else
			// 跳转回登录界面
			emit sig_connect_failed();
	};
	std::thread th(f);
	th.detach();
}

void FtpWidget::on_pushButton_cancel_login()
{
	*p_interrupt_login_ = true;
	// 跳转回登录界面
	ui->stackedWidget->setCurrentIndex(0);
}

void FtpWidget::on_pushButton_refresh()
{
	std::vector<file_info> file_infos = ftp_core_->GetDirList(this->remote_path_);
	file_list_clear();
	file_list_add(file_infos);
}

void FtpWidget::on_lineEdit_url_working()
{
	QString path = ui->lineEdit_url_working->text();
	this->remote_path_ = path.toStdString();
	on_pushButton_refresh();
}

void FtpWidget::on_pushButton_cdup()
{
	const std::string par_path = getParentDirectory(remote_path_);
	remote_path_ = par_path;
	ui->lineEdit_url_working->setText(remote_path_.c_str());
	on_pushButton_refresh();
}

void FtpWidget::on_putAction()
{
	QFileDialog file_dialog(this, "上传文件");
	// QFileDialog::FileMode::ExistingFile 只允许用户选择一个已经存在的文件，而不是创建一个新文件
	file_dialog.setFileMode(QFileDialog::FileMode::ExistingFile);
	// 没有点击确认按钮 关闭文件窗口
	if (file_dialog.exec() != QDialog::Accepted)
		return;
	QStringList files = file_dialog.selectedFiles();

	// 最多只能选择一个文件
	if (files.size() > 1) // TODO
	{
		QMessageBox::warning(this, "选择文件过多！", "最多选择一个文件");
		return;
	}
	qDebug() << files[0];

	const std::filesystem::path path(files[0].toStdString());
	std::string remote_file = remote_path_ + "/" + path.filename().string();
	std::string loc_file = files[0].toStdString();
	// 上传
	std::future<CURLcode> ret = ftp_core_->PutFile(remote_file, loc_file);
	put_rets_.emplace_back(remote_file, loc_file, std::move(ret));

	// 假如没有启动 上传返回值检测线程 则启动线程
	if (is_putting_atomic_.load() == false)
	{
		is_putting_atomic_.store(true);
		check_put_result_thread_ = std::thread(&FtpWidget::check_put_result_thread_func_, this);
		check_put_result_thread_.detach();
	}
}

void FtpWidget::on_tableWidget_cellDoubleClicked(int row, int idx)
{
	// 位置不合理 退出
	if (row == -1)
		return;

	/*
	 * "名称", "大小", "修改日期", "权限", "用户", "用户组", "链接数"
	 *  [0]    [1]    [2]        [3]    [4]    [5]      [6]
	 */
	// 名称
	QString name = ui->tableWidget->item(row, 0)->text();
	// 假如这是个软链接
	if (ui->tableWidget->item(row, 3)->text()[0] == 'l')
	{
		/*
		 * bin -> usr/bin
		 */
		name = name.split("->")[0];
		name = name.trimmed(); // 去除首尾空格
	}
	// 如果是当前目录，那就不用跳
	if (name.trimmed() == ".")
		return;

	// 如果是跳转到上级
	if (name.trimmed() == "..")
		remote_path_ = getParentDirectory(remote_path_);
	else
		remote_path_ = remote_path_ +
		               (remote_path_[remote_path_.size() - 1] == '/' ? "" : "/") +
		               name.toStdString();


	on_pushButton_refresh();
	ui->lineEdit_url_working->setText(remote_path_.c_str());
}

void FtpWidget::on_getAction()
{
	/*
	 * "名称", "大小", "修改日期", "权限", "用户", "用户组", "链接数"
     *  [0]    [1]    [2]        [3]    [4]    [5]      [6]
     */

	// 鼠标点到了哪一行
	int row = ui->tableWidget->currentRow();
	if (row == -1)
	{
		QMessageBox::warning(this, "没有选择下载的文件", "没有选择下载文件");
		return;
	}

	QFileDialog file_dialog(this, "下载文件到");
	// QFileDialog::FileMode::AnyFile 允许用户选择任何类型的文件，包括新文件和现有文件
	file_dialog.setFileMode(QFileDialog::FileMode::AnyFile);

	// 文件名称
	QString name = ui->tableWidget->item(row, 0)->text();
	// 假如这是个软链接
	if (ui->tableWidget->item(row, 3)->text()[0] == 'l')
	{
		/*
		 * bin -> usr/bin
		 */
		name = name.split("->")[0];
		name = name.trimmed(); // 去除首尾空格
	}

	// 这是个文件夹
	if (ui->tableWidget->item(row, 3)->text()[0] == 'd') // TODO
	{
		QMessageBox::warning(this, "请重新选择", "这是个文件夹，无法下载");
		return;
	}

	// 记录列表的文件夹名
	file_dialog.selectFile(name);

	// 弹出文件框框 挑选下载路径
	if (file_dialog.exec() != QDialog::Accepted)
		return;

	QStringList files = file_dialog.selectedFiles();
	// 最多只能选择一个文件来下载
	if (files.size() > 1) // TODO
	{
		QMessageBox::warning(this, "选择文件过多！", "最多选择一个文件");
		return;
	}
	qDebug() << files[0];
	// 下载
	std::string remote_file = remote_path_ + "/" + name.toStdString();
	std::string loc_file = files[0].toStdString();
	std::future<CURLcode> ret = ftp_core_->GetFile(remote_file, loc_file);

	get_rets_.emplace_back(remote_file, loc_file, std::move(ret));

	// 假如没有启动 上传返回值检测线程 则启动线程
	if (is_getting_atomic_.load() == false)
	{
		is_getting_atomic_.store(true);
		check_get_result_thread_ = std::thread(&FtpWidget::check_get_result_thread_func_, this);
		check_get_result_thread_.detach();
	}
}

void FtpWidget::on_delAction()
{
	int row = ui->tableWidget->currentRow();
	if (row == -1)
	{
		QMessageBox::warning(this, "删除失败", "没有选择删除的文件");
		return;
	}

	// 文件名称
	QString name = ui->tableWidget->item(row, 0)->text();
	// 假如这是个软链接
	if (ui->tableWidget->item(row, 3)->text()[0] == 'l')
	{
		/*
		 * bin -> usr/bin
		 */
		name = name.split("->")[0];
		name = name.trimmed(); // 去除首尾空格
	}

	file_type file_t = file;
	// 这是个文件夹
	if (ui->tableWidget->item(row, 3)->text()[0] == 'd')
		file_t = file_type::dir;

	// 给一个反悔的机会
	if (auto box = QMessageBox::warning(this,
	                                    "提醒", QString{"您确认要删除 %0 ?"}.arg(name),
	                                    QMessageBox::Yes | QMessageBox::No);
		box == QMessageBox::No)
		return;

	// delete
	std::string del_file = remote_path_ + "/" + name.toStdString();
	auto curl_ret = ftp_core_->Delete(del_file, file_t);
	if (curl_ret == CURLE_OK)
		QMessageBox::information(this, "消息", QString{"删除 %0 成功"}.arg(name));
	else
		QMessageBox::warning(this, "提醒", QString{"删除 %0 失败"}.arg(name));

	on_pushButton_refresh();
}

void FtpWidget::on_renameAction()
{
	int row = ui->tableWidget->currentRow();
	if (row == -1)
	{
		QMessageBox::warning(this, "提醒", "没有选择删除文件");
		return;
	}

	// 文件名称
	QString name = ui->tableWidget->item(row, 0)->text();
	// 假如这是个软链接
	if (ui->tableWidget->item(row, 3)->text()[0] == 'l')
	{
		/*
		 * bin -> usr/bin
		 */
		name = name.split("->")[0];
		name = name.trimmed(); // 去除首尾空格
	}

	// 输入新的名称 弹窗
	bool is_input_ok;
	QString new_name = QInputDialog::getText(this,
	                                         "请输入新名称", "新名称: ", QLineEdit::Normal, name, &is_input_ok);
	if (is_input_ok == false)
		return;

	// 重命名
	std::string old_file = remote_path_ + "/" + name.toStdString();
	std::string new_file = remote_path_ + "/" + new_name.toStdString();
	CURLcode curl_ret = ftp_core_->Rename(old_file, new_file);
	if (curl_ret == CURLE_OK)
		QMessageBox::information(this, "消息", QString{"重命名 %0 为 %1 成功"}.arg(name).arg(new_name));
	else if (curl_ret == CURLE_REMOTE_FILE_EXISTS)
		QMessageBox::warning(this, "提醒", QString{"重命名 %0 为 %1 失败，远程文件已存在"}.arg(name).arg(new_name));
	else
		QMessageBox::warning(this, "提醒", QString{"重命名 %0 为 %1 失败"}.arg(name).arg(new_name));

	on_pushButton_refresh();
}

void FtpWidget::on_mkdirAction()
{
	bool is_input_ok;
	QString folder_name = QInputDialog::getText(this, "创建文件夹", "文件夹名称: ", QLineEdit::Normal, "", &is_input_ok);
	if (is_input_ok == false)
		return;

	std::string new_folder_path = remote_path_ + "/" + folder_name.toStdString();
	CURLcode curl_ret = ftp_core_->Mkdir(new_folder_path);
	if (curl_ret == CURLE_OK)
		QMessageBox::information(this,
		                         "消息",
		                         QString{"创建文件夹 %0 成功"}.arg(folder_name));
	else if (curl_ret == CURLE_REMOTE_FILE_EXISTS)
		QMessageBox::warning(this,
		                     "提醒",
		                     QString{"创建文件夹 %0 失败，远程文件已存在"}.arg(folder_name));
	else
		QMessageBox::warning(this,
		                     "提醒",
		                     QString{"创建文件夹 %0 失败"}.arg(folder_name));

	on_pushButton_refresh();
}

void FtpWidget::on_tableWidget_cellClicked(int row, int col)
{
	/*
	 * "名称", "大小", "修改日期", "权限", "用户", "用户组", "链接数"
	 *  [0]    [1]    [2]        [3]    [4]    [5]      [6]
	 */

	// 判断是什么类型
	file_type file_t = file_type::none;
	if (QChar t = ui->tableWidget->item(row, 3)->text()[0];
		t == 'd')
		// 文件夹
		file_t = file_type::dir;
	else if (t == 'l')
		// 软链接
		file_t = file_type::link;
	else
		// 文件
		file_t = file_type::file;

	// 文件名
	QString name = ui->tableWidget->item(row, 0)->text();
	// 文件大小
	int bytes = ui->tableWidget->item(row, 1)->text().toInt();
	// 最新的修改时间
	QString modfy_time = ui->tableWidget->item(row, 2)->text();

	// 更新详细信息窗口
	update_groupBox_info(name, file_t, bytes, modfy_time);
}

void FtpWidget::check_put_result_thread_func_()
{
	/**
	 * put_rets_
	 * 第1个 std::string 远程文件
	 * 第2个 std::string 本地文件
	 * 第3个 std::future<CURLcode> 上传完文件后的结果
	 */

	int i = 0;
	while (!put_rets_.empty())
	{
		if (put_rets_.size() > i)
		{
			// 假如已经上传完毕 即可查看结果
			// std::future<CURLcode>
			if (std::get<2>(put_rets_[i]).wait_for(std::chrono::milliseconds(100)) ==
			    std::future_status::ready)
			{
				// 远程文件
				QString remote_file = std::get<0>(put_rets_[i]).c_str();
				// 本地文件
				QString loc_file = std::get<1>(put_rets_[i]).c_str();
				// 上传的结果

				std::string msg{
					std::get<2>(put_rets_[i]).get() == CURLE_OK
						? "上传成功"
						: "上传失败"
				};
				QString curl_msg;
				curl_msg += "远程文件: " + remote_file + "\n本地文件: " + loc_file + "\n" + msg.c_str();

				emit sig_put_result(curl_msg);

				put_rets_.erase(put_rets_.begin() + i);
				continue;
			}
			else
				++i;
		}
		else
			i = 0;
	}

	is_putting_atomic_.store(false);
}

void FtpWidget::check_get_result_thread_func_()
{
	/**
	 * get_rets_
	 * 第1个 std::string 远程文件
	 * 第2个 std::string 本地文件
	 * 第3个 std::future<CURLcode> 下载完文件后的结果
	 */

	int i = 0;
	while (!get_rets_.empty())
	{
		if (get_rets_.size() > i)
		{
			// 假如已经下载完毕 即可查看结果
			// std::future<CURLcode>
			if (std::get<2>(get_rets_[i]).wait_for(std::chrono::milliseconds(100)) ==
			    std::future_status::ready)
			{
				// 远程文件
				QString remote_file = std::get<0>(get_rets_[i]).c_str();
				// 本地文件
				QString loc_file = std::get<1>(get_rets_[i]).c_str();
				// 上传的结果
				std::string msg{
					std::get<2>(get_rets_[i]).get() == CURLE_OK
						? "下载成功"
						: "下载失败"
				};
				QString curl_msg;
				curl_msg += "远程文件: " + remote_file + "\n本地文件: " + loc_file + "\n" + msg.c_str();

				emit sig_get_result(curl_msg);
				get_rets_.erase(get_rets_.begin() + i);
				continue;
			}
			else
				++i;
		}
		else
			i = 0;
	}

	is_getting_atomic_.store(false);
}
