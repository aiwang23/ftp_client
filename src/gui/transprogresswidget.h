//
// Created by 10484 on 24-12-10.
//

#ifndef TRANSPROGRESSWIDGET_H
#define TRANSPROGRESSWIDGET_H

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui { class TransProgressWidget; }
QT_END_NAMESPACE

class TransProgressWidget : public QWidget {
Q_OBJECT

public:
    explicit TransProgressWidget(QWidget *parent = nullptr);
    ~TransProgressWidget() override;

private:
    Ui::TransProgressWidget *ui;
};


#endif //TRANSPROGRESSWIDGET_H
