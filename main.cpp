#include "mainwindow.h"
#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Проверяем доступность системного трея
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "Ошибка", "Системный трей недоступен!");
        return 1;
    }

    MainWindow w;
    w.setWindowFlags(Qt::FramelessWindowHint);
    w.setAttribute(Qt::WA_TranslucentBackground);
    w.setFixedSize(375, 500);
    w.move(1545, 540);
    w.setWindowIcon(QIcon(":/icons/MainIcon.ico"));

    // Создаем иконку для трея
    QSystemTrayIcon trayIcon;
    trayIcon.setIcon(QIcon(":/icons/MainIcon.ico")); // Используем ту же иконку
    trayIcon.setToolTip("FaceitUrStats");

    // Создаем меню для трея
    QMenu trayMenu;
    trayMenu.addAction("Восстановить", &w, &MainWindow::showNormal);
    w.setWindowFlags(w.windowFlags() | Qt::WindowStaysOnTopHint);
    trayMenu.addAction("Выход", &a, &QApplication::quit);
    trayIcon.setContextMenu(&trayMenu);

    // Показываем иконку в трее
    trayIcon.show();

    // Обработчик клика по иконке трея
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Trigger) {
            if (w.isMinimized() || !w.isVisible()) {
                w.showNormal();
            } else {
                w.hide();
            }
        }
    });

    // Обработчик закрытия окна (чтобы приложение не закрывалось полностью)
    QObject::connect(&a, &QApplication::aboutToQuit, []() {
        // Здесь можно сохранить состояние или выполнить другие действия
    });

    w.show();
    return a.exec();
}
