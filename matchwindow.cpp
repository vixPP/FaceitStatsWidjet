#include "matchwindow.h"
#include <QApplication>


MatchWindow::MatchWindow(QWidget *parent)
    : QMainWindow(parent)
{

    setupUI();


    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground );
    setFixedSize(500, 480);
    move(1020, 540);
}



void MatchWindow::setupUI()
{
    // Основной виджет и layout
    centralWidget = new QWidget(this);
    centralWidget->setStyleSheet(
        "QWidget "
        "{"
        "    background-color: #2e2e2d;"
        "    border: 2px solid #4CAF50;"
        "    border-radius: 10px;"
        "}"
    );
    setCentralWidget(centralWidget);

    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Кнопка возврата
    backButton = new QPushButton("CLOSE", this);
    backButton->setFixedSize(50, 40);
    backButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    border: none;"
        "    border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #45a049;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #3d8b40;"
        "}"
    );
    connect(backButton, &QPushButton::clicked, this, &MatchWindow::onBackButtonClicked);

    // Информационная надпись
    infoLabel = new QLabel("Здесь будет отображаться статистика текущего матча\n\nФункция в разработке...", this);
    infoLabel->setStyleSheet("font-size: 16px; color: #666; text-align: center;");
    infoLabel->setAlignment(Qt::AlignCenter);


    Player1Label = new QLabel("Здесь будет\n статистика\n игрока 1", this);
    infoLabel->setStyleSheet("font-size: 16px; color: #666; text-align: center;");
    infoLabel->setAlignment(Qt::AlignCenter);

    // Добавляем элементы в layout с выравниванием
    //mainLayout->addWidget(backButton, 0, Qt::AlignLeft); // ← Выравнивание по левому краю
    // или
     mainLayout->addWidget(backButton, 0, Qt::AlignRight); // ← Выравнивание по правому краю
    // или
    // mainLayout->addWidget(backButton, 0, Qt::AlignHCenter); // ← Выравнивание по центру


    mainLayout->addWidget(infoLabel);
    mainLayout->addWidget(Player1Label);
    mainLayout->addStretch();
}



void MatchWindow::onBackButtonClicked()
{
   this->close();
}
