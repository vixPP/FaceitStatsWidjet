#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "qlabel.h"
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void applyRoundedCorners();

private slots:
    void onFetchStatsClicked();
    void onRequestFinished(QNetworkReply *reply);
    void onMatchHistoryFinished(QNetworkReply *reply);
    void onAvatarDownloaded(QNetworkReply *reply);

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *networkManager;
    QNetworkAccessManager *avatarNetworkManager;
    QNetworkAccessManager *statsNetworkManager;
    QString currentPlayerId;
    QString apiKey = "7334b675-bf37-41a3-9f37-df39acb05fba"; // Перенесено в поле класса
    QLabel *avatarLabel;
    QLabel *text_KR;
};
#endif // MAINWINDOW_H

