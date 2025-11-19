#include "mainwindow.h"
#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMessageBox>
#include <QDebug>
#include <QLockFile>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString tempPath = QDir::temp().absoluteFilePath("myapp.lock");
       QLockFile lockFile(tempPath);

       // Пытаемся получить эксклюзивный доступ
       if (!lockFile.tryLock(100))
       {
           QMessageBox::warning(nullptr, "faceitUrStats",
                              "Приложение уже запущено!");
           return 1;
       }


    // Проверяем доступность системного трея
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        QMessageBox::critical(nullptr, "Ошибка", "Системный трей недоступен!");
        return 1;
    }

    // Загружаем иконку один раз и используем везде
    QIcon appIcon;

    // Пробуем разные пути и форматы
    appIcon = QIcon(":/MainIcon.png");  // Из ресурсов
    if (appIcon.isNull())
    {
        appIcon = QIcon(":/icons/MainIcon.png");
    }
    if (appIcon.isNull())
    {
        appIcon = QIcon(":/MainIcon.ico");
    }
    if (appIcon.isNull())
    {
        appIcon = QIcon(":/icons/MainIcon.ico");
    }

    // Если все еще не загрузилась, создаем простую иконку
    if (appIcon.isNull())
    {
        qDebug() << "Иконка не загружена! Создаем временную";
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::blue);
        appIcon = QIcon(pixmap);
    } else {
        qDebug() << "Иконка успешно загружена";
    }

    // Устанавливаем иконку для приложения
    a.setWindowIcon(appIcon);

    MainWindow w;
    w.setWindowFlags(Qt::FramelessWindowHint);
    w.setAttribute(Qt::WA_TranslucentBackground);
    w.setFixedSize(375, 500);
    w.move(1535, 540);

    // Устанавливаем ту же иконку для окна
    w.setWindowIcon(appIcon);

    // Создаем иконку для трея
    QSystemTrayIcon trayIcon;

    trayIcon.setIcon(QIcon(":/icons/MainIcon.ico")); // Используем ту же иконку
    trayIcon.setToolTip("FaceitUrStats");

    // Создаем меню для трея
    QMenu trayMenu;
    //trayMenu.addAction("Восстановить", &w, &MainWindow::showNormal);
    w.setWindowFlags(w.windowFlags() | Qt::WindowStaysOnTopHint);
    trayMenu.addAction("Выход", &a, &QApplication::quit);
    trayIcon.setContextMenu(&trayMenu);

    // Показываем иконку в трее
    trayIcon.show();

    //сворачивание окон при нажатии по трею
    MatchWindow* matchWindow = nullptr;

    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason reason)
        {
            if (reason == QSystemTrayIcon::Trigger)
            {
                if (w.isVisible() && !w.isMinimized())
                {
                    // Сворачиваем основное окно
                    w.showMinimized();
                    w.hide();
                    // Закрываем окно матча, если оно открыто
                    if (matchWindow && matchWindow->isVisible())
                    {
                        matchWindow->close();
                    }
                } else
                {
                    // Показываем основное окно
                    w.showNormal();
                    w.activateWindow();
                }
            }
        });


    w.show();

    return a.exec();
}
