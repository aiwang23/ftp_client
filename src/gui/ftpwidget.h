//
// Created by 10484 on 24-9-24.
//

#ifndef FTPWIDGET_H
#define FTPWIDGET_H

#include <QWidget>
#include "FtpCore.h"
#include <memory>

QT_BEGIN_NAMESPACE

namespace Ui
{
    class FtpWidget;
}

QT_END_NAMESPACE

class FtpWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FtpWidget(QWidget *parent = nullptr);

    ~FtpWidget() override;

private:
    // 初始信号与槽
    void init_connect_sig_slots();

    // 初始化资源文件
    void init_resource();

    // 初始化文件列表 ui->tableWidget
    void init_file_list();

    // 清空文件列表
    void file_list_clear();

    // 添加文件列表
    void file_list_add(std::vector<file_info> &file_list);

    // 添加文件列表项
    void file_list_item_add(int row, int idx, const std::string &item);

    /**
     * 获取path的上级目录
     * @param path 目录
     * @return path的上级目录
     */
    static std::string getParentDirectory(const std::string& path);

    // 初始化右键菜单
    void init_memu();   // TODO

signals:
    // 连接成功时发送
    void sig_connect_OK();

    // 连接失败时发送
    void sig_connect_failed();

public:
    // 点击开始登录按钮
    void on_pushButton_start();

    // 点击取消登录按钮
    void on_pushButton_cancel_login();

    // 点击刷新按钮
    void on_pushButton_refresh();

    // 回车 更新路径
    void on_lineEdit_url_working();

    // 回退上级目录
    void on_pushButton_cdup();

private:
    Ui::FtpWidget *ui;
    std::unique_ptr<FtpCore> ftp_core_;         // ftp ftps sftp 核心功能
    bool *p_interrupt_login_ = new bool(false); // 在尝试登录时中断
    QPixmap *pixmap_;                           // 加载图标

    std::string remote_path_ = "/";             // 远程路径 默认在'/'
};


#endif //FTPWIDGET_H
