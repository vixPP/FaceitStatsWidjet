#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "matchwindow.h"
#include "qlabel.h"
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct MapStats
{
    QString mapName;
    int kills = 0;
    int deaths = 0;
    double kdRatio = 0.0;
};

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
    void onMatchHistoryFetched(QNetworkReply *reply);
    void onMatchStatsFetched(QNetworkReply *reply);
    void onBestMapImageDownloaded(QNetworkReply *reply);
    void onFetch10MatchesClicked();
    void onFetch20MatchesClicked();
    void onFetch30MatchesClicked();
    void fetchInternalMatchStats(const QString &matchId);
    void onInternalMatchStatsFetched(QNetworkReply *reply);
    void on_Button_Save_clicked();
    void autoFetchStats();
    void onInTheMatchhButtonClicked();

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *networkManager;
    QNetworkAccessManager *avatarNetworkManager;
    QNetworkAccessManager *statsNetworkManager;
    QNetworkAccessManager *historyNetworkManager;
    QNetworkAccessManager *matchStatsNetworkManager;
    QNetworkAccessManager* bestMapImageNetworkManager;
    QNetworkAccessManager *internalStatsNetworkManager;

    QString currentPlayerId;
    QString apiKey = "7334b675-bf37-41a3-9f37-df39acb05fba"; // Перенесено в поле класса
    QLabel *avatarLabel;
    QLabel *text_KR;
    QLabel *bestMapImageLabel;

    int activeRequests = 0;
    bool isRequestInProgress = false;
    QNetworkReply* currentReply = nullptr;

    QTimer *buttonSearcheTimer;
    QTimer *button10Timer;
    QTimer *button20Timer;
    QTimer *button30Timer;
    QTimer *messageTimer;
    int currentPlayerElo; // Текущее ELO игрока
    int totalKills = 0;
    int matchesCount = 0;
    int processedMatches = 0;
    int totalDeaths = 0;
    int matchesToFetch;
    int matchesToRequest;
    double totalADR;
    QMap<QString, MapStats> mapStats;
    QList<double> matchKDRatios;

    // Окно матча
    MatchWindow *matchWindow;
    QPushButton *InTheMatchhButton;



};
#endif // MAINWINDOW_H
