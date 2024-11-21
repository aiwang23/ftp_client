#include <QApplication>
#include "ftpwidget.h"
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFile qss_file(":/qss/Ubuntu.qss");
    // QFile qss_file(":/qss/ElegantDark.qss");
    if (qss_file.open(QFile::ReadOnly))
    {
        a.setStyleSheet(qss_file.readAll());
    }
    qss_file.close();

    FtpWidget widget;
    widget.show();
    return QApplication::exec();
}
