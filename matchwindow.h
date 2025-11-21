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
    void debugJsonStructure(const QJsonObject &obj, const QString &prefix);

private:
    void setupUI();
    void displayMatchInfo(const QJsonObject &matchData);
    void displayPlayerStats(const QJsonObject &statsData);

    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QPushButton *backButton;
    QPushButton *refreshButton;
    QLabel *infoLabel;
    QLabel *playerStatsLabel;

    QNetworkAccessManager *networkManager;
    QString apiKey;
    QString currentPlayerId;
    QString currentMatchId;
};

#endif // MATCHWINDOW_H
