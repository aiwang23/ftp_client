#include <QApplication>
#include "ftpwidget.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    FtpWidget widget;
    widget.show();
    return QApplication::exec();


}
