#include "matchwindow.h"
#include "qjsonarray.h"
#include "qjsondocument.h"
#include "qjsonobject.h"
#include <QApplication>
#include <QDebug>
#include <QScrollArea>
#include <QRegularExpression>

MatchWindow::MatchWindow(const QString &apiKey, QWidget *parent)
    : QMainWindow(parent)
    , apiKey(apiKey)
    , team1Name("Команда 1")
    , team2Name("Команда 2")
{
    networkManager = new QNetworkAccessManager(this);
    setupConnections();

    setupUI();

    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(500, 600);
    move(1020, 440);
}

void MatchWindow::setupConnections()
{
    disconnect(networkManager, &QNetworkAccessManager::finished, this, nullptr);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MatchWindow::onMatchDataLoaded);
}

void MatchWindow::loadMatchById(const QString &matchId, const QString &playerId)
{
    currentMatchId = matchId;
    currentPlayerId = playerId;
    playerStatsMap.clear();
    playerTeamMap.clear();
    loadedPlayersCount = 0;
    totalPlayersToLoad = 0;

    setupConnections();

    QString url = QString("https://open.faceit.com/data/v4/matches/%1").arg(matchId);
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Accept", "application/json");

    networkManager->get(request);
    infoLabel->setText("Загрузка информации о матче...");
    progressBar->setValue(0);
    progressBar->setVisible(false);
}

void MatchWindow::onMatchDataLoaded(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Match data error:" << reply->errorString();
        infoLabel->setText("Ошибка загрузки матча: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    reply->deleteLater();

    if (jsonDoc.isNull() || !jsonDoc.isObject())
    {
        infoLabel->setText("Ошибка: некорректные данные матча");
        return;
    }

    QJsonObject matchData = jsonDoc.object();
    displayMatchInfo(matchData);

    // После отображения основной информации начинаем загрузку статистики игроков
    if (!currentMapName.isEmpty() && !playerStatsMap.isEmpty() && currentMapName != "Неизвестно")
    {
        // Меняем соединение для загрузки статистики игроков
        disconnect(networkManager, &QNetworkAccessManager::finished, this, &MatchWindow::onMatchDataLoaded);
        connect(networkManager, &QNetworkAccessManager::finished, this, &MatchWindow::onPlayerStatsLoaded);

        // Загружаем статистику для каждого игрока
        loadedPlayersCount = 0;
        totalPlayersToLoad = playerStatsMap.size();

        for (auto it = playerStatsMap.begin(); it != playerStatsMap.end(); ++it)
        {
            fetchPlayerStats(it.key(), it.value().nickname, currentMapName);
        }

        progressBar->setVisible(true);
        progressBar->setMaximum(totalPlayersToLoad);
        progressBar->setValue(0);

        infoLabel->setText(infoLabel->text() + "\n\n📊 Загрузка статистики игроков на карте " + currentMapName + "...");
    }
    else
    {
        if (currentMapName == "Неизвестно") {
            infoLabel->setText(infoLabel->text() + "\n\n❌ Не удалось определить карту для загрузки статистики");
        }
        setupConnections();
    }
}

void MatchWindow::onPlayerStatsLoaded(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Player stats error:" << reply->errorString();
        loadedPlayersCount++;
        progressBar->setValue(loadedPlayersCount);
        reply->deleteLater();

        // Проверяем, все ли загрузки завершены
        if (loadedPlayersCount >= totalPlayersToLoad)
        {
            processAllPlayerStats();
            setupConnections();
        }
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    reply->deleteLater();

    // Получаем playerId из URL
    QString url = reply->url().toString();
    QRegularExpression regex("players/([a-f0-9-]+)/stats");
    QRegularExpressionMatch match = regex.match(url);

    if (match.hasMatch())
    {
        QString playerId = match.captured(1);

        if (playerStatsMap.contains(playerId) && !jsonDoc.isNull() && jsonDoc.isObject())
        {
            QJsonObject root = jsonDoc.object();

            if (root.contains("segments") && root["segments"].isArray())
            {
                QJsonArray segments = root["segments"].toArray();
                bool statsFound = false;

                for (const QJsonValue &segmentValue : segments)
                {
                    if (!segmentValue.isObject()) continue;
                    QJsonObject segment = segmentValue.toObject();

                    if (segment["type"].toString() == "Map" &&
                        segment["label"].toString().compare(currentMapName, Qt::CaseInsensitive) == 0) {

                        QJsonObject stats = segment["stats"].toObject();
                        PlayerStats &playerStats = playerStatsMap[playerId];

                        playerStats.matches = stats.contains("Total Matches") ?
                                                  stats["Total Matches"].toString().toInt() : 0;
                        playerStats.kdRatio = stats.contains("Average K/D Ratio") ?
                                                  stats["Average K/D Ratio"].toString().toDouble() : 0.0;
                        playerStats.avgKills = stats.contains("Average Kills") ?
                                                   stats["Average Kills"].toString().toDouble() : 0.0;
                        playerStats.winRate = stats.contains("Win Rate %") ?
                                                  stats["Win Rate %"].toString().toDouble() : 0.0;
                        playerStats.loaded = true;
                        statsFound = true;

                        qDebug() << "Stats loaded for" << playerStats.nickname << "on" << currentMapName
                                 << "Matches:" << playerStats.matches << "KD:" << playerStats.kdRatio;
                        break;
                    }
                }

                if (!statsFound) {
                    qDebug() << "No stats found for" << playerStatsMap[playerId].nickname << "on map" << currentMapName;
                    playerStatsMap[playerId].loaded = true;
                }
            }
        }
    }

    loadedPlayersCount++;
    progressBar->setValue(loadedPlayersCount);

    // Когда все игроки загружены, обрабатываем и отображаем статистику
    if (loadedPlayersCount >= totalPlayersToLoad)
    {
        processAllPlayerStats();
        setupConnections();
    }
}

void MatchWindow::displayMatchInfo(const QJsonObject &matchData)
{
    QString matchId = matchData["match_id"].toString();
    QString competitionName = matchData["competition_name"].toString("Неизвестный турнир");
    QString status = matchData["status"].toString("Неизвестно");

    QJsonObject teams = matchData["teams"].toObject();
    team1Name = teams["faction1"].toObject()["name"].toString("Команда 1");
    team2Name = teams["faction2"].toObject()["name"].toString("Команда 2");

    QJsonArray team1Players = teams["faction1"].toObject()["roster"].toArray();
    QJsonArray team2Players = teams["faction2"].toObject()["roster"].toArray();

    // Получение названия карты
    currentMapName = "Неизвестно";

    if (matchData.contains("voting") && matchData["voting"].isObject())
    {
        QJsonObject voting = matchData["voting"].toObject();
        if (voting.contains("map") && voting["map"].isObject())
        {
            QJsonObject mapVoting = voting["map"].toObject();

            if (mapVoting.contains("pick") && mapVoting["pick"].isArray())
            {
                QJsonArray picks = mapVoting["pick"].toArray();
                if (!picks.isEmpty()) {
                    QString selectedMapGuid = picks.first().toString();

                    if (mapVoting.contains("entities") && mapVoting["entities"].isArray())
                    {
                        QJsonArray entities = mapVoting["entities"].toArray();
                        for (const QJsonValue &entityValue : entities)
                        {
                            QJsonObject entity = entityValue.toObject();
                            QString guid = entity["guid"].toString();
                            if (guid == selectedMapGuid)
                            {
                                currentMapName = entity["name"].toString("Неизвестно");
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Сохраняем информацию об игроках и их командах
    playerStatsMap.clear();
    playerTeamMap.clear();

    // Команда 1
    for (const QJsonValue &playerValue : team1Players)
    {
        QJsonObject player = playerValue.toObject();
        QString nickname = player["nickname"].toString("Unknown");
        QString playerId = player["player_id"].toString();

        PlayerStats stats;
        stats.nickname = nickname;
        stats.playerId = playerId;
        playerStatsMap[playerId] = stats;
        playerTeamMap[playerId] = team1Name;
    }

    // Команда 2
    for (const QJsonValue &playerValue : team2Players)
    {
        QJsonObject player = playerValue.toObject();
        QString nickname = player["nickname"].toString("Unknown");
        QString playerId = player["player_id"].toString();

        PlayerStats stats;
        stats.nickname = nickname;
        stats.playerId = playerId;
        playerStatsMap[playerId] = stats;
        playerTeamMap[playerId] = team2Name;
    }

    totalPlayersToLoad = playerStatsMap.size();

    QString matchInfoText = QString(
                                "🎮 Информация о матче\n"
                                "🆔 %1\n"
                                "🗺️ Карта: %2\n"
                                "🏆 Турнир: %3\n"
                                "📊 Статус: %4\n\n"
                                "👥 Команды:\n"
                                "🔵 %5 (%6 игроков)\n"
                                "🔴 %7 (%8 игроков)")
                                .arg(matchId)
                                .arg(currentMapName)
                                .arg(competitionName)
                                .arg(status)
                                .arg(team1Name)
                                .arg(team1Players.size())
                                .arg(team2Name)
                                .arg(team2Players.size());

    infoLabel->setText(matchInfoText);

    // Показываем начальную информацию об игроках
    QString playersText = "Список игроков:\n\n";

    playersText += "🔵 " + team1Name + ":\n";
    for (const QJsonValue &playerValue : team1Players)
    {
        QJsonObject player = playerValue.toObject();
        QString nickname = player["nickname"].toString("Unknown");
        QString playerId = player["player_id"].toString();
        bool isCurrent = (playerId == currentPlayerId);
        playersText += QString("   %1%2\n").arg(nickname).arg(isCurrent ? " (ТЫ)" : "");
    }

    playersText += "\n🔴 " + team2Name + ":\n";
    for (const QJsonValue &playerValue : team2Players)
    {
        QJsonObject player = playerValue.toObject();
        QString nickname = player["nickname"].toString("Unknown");
        QString playerId = player["player_id"].toString();
        bool isCurrent = (playerId == currentPlayerId);
        playersText += QString("   %1%2\n").arg(nickname).arg(isCurrent ? " (ТЫ)" : "");
    }

    playersText += "\n\n📊 Загрузка статистики игроков на карте " + currentMapName + "...";
    playerStatsLabel->setText(playersText);
}

void MatchWindow::fetchPlayerStats(const QString &playerId, const QString &nickname, const QString &mapName)
{
    QString statsUrl = QString("https://open.faceit.com/data/v4/players/%1/stats/cs2").arg(playerId);
    QNetworkRequest request;
    request.setUrl(QUrl(statsUrl));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Accept", "application/json");

    networkManager->get(request);
}

void MatchWindow::processAllPlayerStats()
{
    QString statsText = "📊 Статистика игроков на карте " + currentMapName + ":\n\n";

    QString team1Stats = "🔵 " + team1Name + ":\n";
    QString team2Stats = "🔴 " + team2Name + ":\n";

    bool hasStats = false;

    // Сортируем игроков по командам
    for (auto it = playerStatsMap.begin(); it != playerStatsMap.end(); ++it) {
        const QString &playerId = it.key();
        const PlayerStats &stats = it.value();

        if (stats.matches > 0) {
            hasStats = true;
        }

        QString playerLine = QString("   %1 - M: %2 | KD: %3 | AVG.K: %4 | WR: %5%\n")
                                 .arg(stats.nickname)
                                 .arg(stats.matches)
                                 .arg(stats.kdRatio, 0, 'f', 2)
                                 .arg(stats.avgKills, 0, 'f', 1)
                                 .arg(stats.winRate, 0, 'f', 1);

        if (playerTeamMap.contains(playerId)) {
            if (playerTeamMap[playerId] == team1Name) {
                team1Stats += playerLine;
            } else {
                team2Stats += playerLine;
            }
        }
    }

    if (!hasStats) {
        statsText += "❌ Статистика на этой карте не найдена для игроков\n";
        statsText += "Возможно, у игроков нет сыгранных матчей на карте " + currentMapName;
    } else {
        statsText += team1Stats + "\n" + team2Stats;
    }

    playerStatsLabel->setText(statsText);
    progressBar->setVisible(false);
}

void MatchWindow::setupUI()
{
    centralWidget = new QWidget(this);
    centralWidget->setStyleSheet(
        "QWidget {"
        "    background-color: #2e2e2d;"
        "    border: 2px solid #4CAF50;"
        "    border-radius: 10px;"
        "}"
        "QProgressBar {"
        "    border: 1px solid #4CAF50;"
        "    border-radius: 5px;"
        "    text-align: center;"
        "    color: white;"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #4CAF50;"
        "    border-radius: 4px;"
        "}"
        );
    setCentralWidget(centralWidget);

    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    QHBoxLayout *topLayout = new QHBoxLayout();

    refreshButton = new QPushButton("🔄 Обновить", this);
    refreshButton->setFixedSize(100, 30);
    refreshButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    font-size: 12px;"
        "    border: none;"
        "    border-radius: 5px;"
        "}"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:pressed { background-color: #3d8b40; }"
        );
    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        if (!currentMatchId.isEmpty()) {
            infoLabel->setText("Обновление...");
            loadMatchById(currentMatchId, currentPlayerId);
        }
    });

    backButton = new QPushButton("✕ Закрыть", this);
    backButton->setFixedSize(80, 30);
    backButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #f44336;"
        "    color: white;"
        "    font-size: 12px;"
        "    border: none;"
        "    border-radius: 5px;"
        "}"
        "QPushButton:hover { background-color: #da190b; }"
        "QPushButton:pressed { background-color: #a1150b; }"
        );
    connect(backButton, &QPushButton::clicked, this, &MatchWindow::onBackButtonClicked);

    topLayout->addWidget(refreshButton);
    topLayout->addStretch();
    topLayout->addWidget(backButton);

    infoLabel = new QLabel("Вставьте ссылку на комнату матча", this);
    infoLabel->setStyleSheet("font-size: 12px; color: #ffffff;");
    infoLabel->setAlignment(Qt::AlignLeft);
    infoLabel->setWordWrap(true);

    progressBar = new QProgressBar(this);
    progressBar->setVisible(false);
    progressBar->setFixedHeight(20);
    progressBar->setTextVisible(true);
    progressBar->setFormat("Загрузка статистики: %p%");

    playerStatsLabel = new QLabel("Информация о игроках появится здесь", this);
    playerStatsLabel->setStyleSheet("font-size: 11px; color: #cccccc;");
    playerStatsLabel->setAlignment(Qt::AlignLeft);
    playerStatsLabel->setWordWrap(true);

    QScrollArea *scrollArea = new QScrollArea(this);
    QWidget *scrollContent = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->addWidget(playerStatsLabel);
    scrollArea->setWidget(scrollContent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background-color: transparent; }");
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(infoLabel);
    mainLayout->addWidget(progressBar);
    mainLayout->addWidget(scrollArea);
}

void MatchWindow::onBackButtonClicked()
{
    this->close();
}
