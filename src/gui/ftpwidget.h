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
    void init_connect();

signals:
    void sig_connect_OK();
    void sig_connect_failed();

public:
    void on_pushButton_start();

    void on_pushButton_cancel_login();

private:
    Ui::FtpWidget *ui;
    std::unique_ptr<FtpCore> ftp_core_;
    bool *p_interrupt_login_ = new bool(false);
    QMovie *movie_ = nullptr;
};


#endif //FTPWIDGET_H
