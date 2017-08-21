#include "mainwindow.h"

#include <time.h>
#include <QApplication>

int main(int argc, char *argv[])
{
    srand(time(NULL));

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
