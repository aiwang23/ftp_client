//
// Created by 10484 on 24-12-10.
//

// You may need to build the project (run Qt uic code generator) to get "ui_TransProgressWidget.h" resolved

#include "transprogresswidget.h"
#include "ui_TransProgressWidget.h"


TransProgressWidget::TransProgressWidget(QWidget *parent) :
    QWidget(parent), ui(new Ui::TransProgressWidget) {
    ui->setupUi(this);
}

TransProgressWidget::~TransProgressWidget() {
    delete ui;
}
