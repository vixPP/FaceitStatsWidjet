#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    w.setWindowFlags(Qt::FramelessWindowHint);
    w.setAttribute(Qt::WA_TranslucentBackground);
    //w.setStyleSheet("background-color: rgba(40, 40, 40, 250);");
    w.setFixedSize(350, 500);
    w.move(1550, 520);

    w.show();
    return a.exec();
}
