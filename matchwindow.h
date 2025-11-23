#ifndef MATCHWINDOW_H
#define MATCHWINDOW_H

#include "qnetworkreply.h"
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QNetworkReply>
#include <QScrollArea>
#include <QProgressBar>

class MatchWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MatchWindow(const QString &apiKey, QWidget *parent = nullptr);
    ~MatchWindow() = default;

    void loadMatchById(const QString &matchId, const QString &playerId);

private slots:
    void onBackButtonClicked();
    void onMatchDataLoaded(QNetworkReply *reply);
    void onMatchStatsLoaded(QNetworkReply *reply);
    void onPlayerStatsLoaded(QNetworkReply *reply);

private:
    void setupUI();
    void displayMatchInfo(const QJsonObject &matchData);

    void fetchPlayerStats(const QString &playerId, const QString &nickname, const QString &mapName);
    void processAllPlayerStats();
    void setupConnections();

    struct PlayerStats
    {
        QString nickname;
        QString playerId;
        int matches = 0;
        double kdRatio = 0.0;
        double avgKills = 0.0;
        double winRate = 0.0;
        bool loaded = false;
    };

    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QPushButton *backButton;
    QPushButton *refreshButton;
    QLabel *infoLabel;
    QLabel *playerStatsLabel;
    QProgressBar *progressBar;

    QNetworkAccessManager *networkManager;
    QString apiKey;
    QString currentPlayerId;
    QString currentMatchId;
    QString currentMapName;

    QMap<QString, PlayerStats> playerStatsMap;
    int totalPlayersToLoad = 0;
    int loadedPlayersCount = 0;

    QMap<QString, QString> playerTeamMap; // playerId -> teamName
        QString team1Name;
        QString team2Name;
};

#endif // MATCHWINDOW_H
