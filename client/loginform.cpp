#include "loginform.h"
#include "ui_loginform.h"
#include <QLineEdit>
loginform::loginform(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::loginform)
{
    ui->setupUi(this);
    ui->passwordLineEdit->setEchoMode(QLineEdit::Password);
    // Подключаем сигнал от кнопки к слоту
    connect(ui->loginButton, &QPushButton::clicked, this, &loginform::on_loginButton_clicked);
}

loginform::~loginform()
{
    delete ui;
}

void loginform::on_loginButton_clicked()
{
    QString username = ui->usernameLineEdit->text(); // Получаем текст из поля для логина
    QString password = ui->passwordLineEdit->text(); // Получаем текст из поля для пароля

    // Генерируем сигнал с логином и паролем
    emit loginDataEntered(username, password);
}
