#include "matchwindow.h"
#include "qjsonarray.h"
#include "qjsondocument.h"
#include "qjsonobject.h"
#include <QApplication>
#include <QDebug>
#include <QScrollArea>

MatchWindow::MatchWindow(const QString &apiKey, QWidget *parent)
    : QMainWindow(parent)
    , apiKey(apiKey)
{
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MatchWindow::onMatchDataLoaded);

    setupUI();

    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(500, 600);
    move(1020, 440);
}

void MatchWindow::loadMatchById(const QString &matchId, const QString &playerId)
{
    currentMatchId = matchId;
    currentPlayerId = playerId;

    QString url = QString("https://open.faceit.com/data/v4/matches/%1").arg(matchId);
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    request.setRawHeader("Accept", "application/json");

    networkManager->get(request);
    infoLabel->setText("Загрузка информации о матче...");
}

void MatchWindow::onMatchDataLoaded(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Match data error:" << reply->errorString();
        infoLabel->setText("Ошибка загрузки матча: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    reply->deleteLater();

    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        infoLabel->setText("Ошибка: некорректные данные матча");
        return;
    }

    QJsonObject matchData = jsonDoc.object();
    displayMatchInfo(matchData);
    qDebug() << "=== DEBUG JSON STRUCTURE ===";
    debugJsonStructure(matchData, "");
    qDebug() << "=== END DEBUG ===";

    if (!currentMatchId.isEmpty())
    {
        QString statsUrl = QString("https://open.faceit.com/data/v4/matches/%1/stats").arg(currentMatchId);
        QNetworkRequest statsRequest;
        statsRequest.setUrl(QUrl(statsUrl));
        statsRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

        disconnect(networkManager, &QNetworkAccessManager::finished, this, &MatchWindow::onMatchDataLoaded);
        connect(networkManager, &QNetworkAccessManager::finished, this, &MatchWindow::onMatchStatsLoaded);

        networkManager->get(statsRequest);
    }
}

void MatchWindow::onMatchStatsLoaded(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Match stats error:" << reply->errorString();
        playerStatsLabel->setText(playerStatsLabel->text() + "\n\n❌ Ошибка загрузки статистики");
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    reply->deleteLater();

    if (!jsonDoc.isNull() && jsonDoc.isObject()) {
        displayPlayerStats(jsonDoc.object());
    }

    disconnect(networkManager, &QNetworkAccessManager::finished, this, &MatchWindow::onMatchStatsLoaded);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MatchWindow::onMatchDataLoaded);
}

void MatchWindow::displayMatchInfo(const QJsonObject &matchData)
{
    QString matchId = matchData["match_id"].toString();
    QString competitionName = matchData["competition_name"].toString("Неизвестный турнир");
    QString status = matchData["status"].toString("Неизвестно");

    QJsonObject teams = matchData["teams"].toObject();
    QString team1Name = teams["faction1"].toObject()["name"].toString("Команда 1");
    QString team2Name = teams["faction2"].toObject()["name"].toString("Команда 2");

    QJsonArray team1Players = teams["faction1"].toObject()["roster"].toArray();
    QJsonArray team2Players = teams["faction2"].toObject()["roster"].toArray();

    // ИСПРАВЛЕННОЕ получение названия карты
    QString mapName = "Неизвестно";

    if (matchData.contains("voting") && matchData["voting"].isObject())
    {
        QJsonObject voting = matchData["voting"].toObject();
        if (voting.contains("map") && voting["map"].isObject())
        {
            QJsonObject mapVoting = voting["map"].toObject();

            // Получаем выбранную карту (GUID) из pick
            if (mapVoting.contains("pick") && mapVoting["pick"].isArray())
            {
                QJsonArray picks = mapVoting["pick"].toArray();
                if (!picks.isEmpty()) {
                    QString selectedMapGuid = picks.first().toString();

                    // Теперь ищем название карты по GUID в entities
                    if (mapVoting.contains("entities") && mapVoting["entities"].isArray())
                    {
                        QJsonArray entities = mapVoting["entities"].toArray();
                        for (const QJsonValue &entityValue : entities) {
                            QJsonObject entity = entityValue.toObject();
                            QString guid = entity["guid"].toString();
                            if (guid == selectedMapGuid) {
                                mapName = entity["name"].toString("Неизвестно");
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Для отладки
    qDebug() << "Selected map:" << mapName;

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
        .arg(mapName)
        .arg(competitionName)
        .arg(status)
        .arg(team1Name)
        .arg(team1Players.size())
        .arg(team2Name)
        .arg(team2Players.size());

    infoLabel->setText(matchInfoText);

    QString playersText = "Список игроков:\n\n";

    playersText += "🔵 " + team1Name + ":\n";
    for (const QJsonValue &playerValue : team1Players) {
        QJsonObject player = playerValue.toObject();
        QString nickname = player["nickname"].toString("Unknown");
        QString playerId = player["player_id"].toString();
        bool isCurrent = (playerId == currentPlayerId);
        playersText += QString("   %1%2\n").arg(nickname).arg(isCurrent ? " (ТЫ)" : "");
    }

    playersText += "\n🔴 " + team2Name + ":\n";
    for (const QJsonValue &playerValue : team2Players) {
        QJsonObject player = playerValue.toObject();
        QString nickname = player["nickname"].toString("Unknown");
        QString playerId = player["player_id"].toString();
        bool isCurrent = (playerId == currentPlayerId);
        playersText += QString("   %1%2\n").arg(nickname).arg(isCurrent ? " (ТЫ)" : "");
    }

    playerStatsLabel->setText(playersText);
}

void MatchWindow::displayPlayerStats(const QJsonObject &statsData)
{
    playerStatsLabel->setText(playerStatsLabel->text() + "\n\n✅ Статистика загружена");
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
    mainLayout->addWidget(scrollArea);
}

void MatchWindow::debugJsonStructure(const QJsonObject &obj, const QString &prefix)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString key = it.key();
        QJsonValue value = it.value();

        qDebug() << prefix << key << ":";

        if (value.isObject()) {
            debugJsonStructure(value.toObject(), prefix + "  ");
        } else if (value.isArray()) {
            qDebug() << prefix << "  [Array]";
        } else {
            qDebug() << prefix << "  " << value.toString();
        }
    }
}



void MatchWindow::onBackButtonClicked()
{
    this->close();
}
