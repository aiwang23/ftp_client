//
// Created by 10484 on 24-9-24.
//

// You may need to build the project (run Qt uic code generator) to get "ui_FtpWidget.h" resolved

#include "ftpwidget.h"
#include "ui_FtpWidget.h"
#include <string>
#include <QMessageBox>
#include <thread>
#include <QMovie>

FtpWidget::FtpWidget(QWidget *parent) : QWidget(parent), ui(new Ui::FtpWidget)
{
    ui->setupUi(this);
    ftp_core_ = std::make_unique<FtpCore>();

    // movie_ = new QMovie("loading.gif"); // 加载 登录等待动画
    movie_ = new QMovie(":/loading.gif"); // 加载 登录等待动画
    ui->label_loading->setMovie(movie_);
    movie_->setCacheMode(QMovie::CacheAll);

    init_connect(); // 初始化信号与槽
}

FtpWidget::~FtpWidget()
{
    if (p_interrupt_login_)
        delete p_interrupt_login_;
    if (movie_)
        delete movie_;
    if (ui)
        delete ui;
}

void FtpWidget::init_connect()
{
    connect(ui->pushButton_start, &QPushButton::clicked, this, &FtpWidget::on_pushButton_start); // 点击登录按钮，尝试登录
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
    movie_->start(); // 加载 等待动画

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
    movie_->stop();
}
