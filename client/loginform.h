#ifndef LOGINFORM_H
#define LOGINFORM_H

#include <QWidget>
#include <QString>

namespace Ui {
class loginform;
}

class loginform : public QWidget
{
    Q_OBJECT

public:
    explicit loginform(QWidget *parent = nullptr);
    ~loginform();

signals:
    void loginDataEntered(const QString &username, const QString &password);

private slots:
    void on_loginButton_clicked();

private:
    Ui::loginform *ui;
};

#endif // LOGINFORM_H
