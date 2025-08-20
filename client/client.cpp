#include "client.h"
#include "ui_client.h"
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include<QSqlRecord>
client::client(QWidget *parent) : QMainWindow(parent), ui(new Ui::client)
{
    ui->setupUi(this);

    clientSocket = new QTcpSocket(this);
    connect(clientSocket, &QTcpSocket::connected, this, &client::onSocketConnected);
    connect(clientSocket, &QTcpSocket::disconnected, this, &client::onSocketDisconnected);
    connect(clientSocket, &QTcpSocket::readyRead, this, &client::readServerResponse);
    connect(ui->seeButton, &QPushButton::clicked, this, &client::sendRequest);
    connect(ui->getButton, &QPushButton::clicked, this, &client::onGetButtonClicked);
    connect(ui->pushButton, &QPushButton::clicked, this, &client::onPushToServerButtonClicked);
    connect(ui->openButton, &QPushButton::clicked, this, &client::openButtonClicked);
    treeWidget = new QTreeWidget(this);
    tableWidget = new QTableWidget(this);

    treeWidget->setHeaderLabel("Databases and Tables");
    ui->verticalLayout->addWidget(treeWidget);
    ui->verticalLayout->addWidget(tableWidget);

    connect(treeWidget, &QTreeWidget::itemClicked, this, &client::onDatabaseOrTableClicked);
    connect(tableWidget, &QTableWidget::itemChanged, this, [this]() {
        isTableModified = true;
    });
    connect(tableWidget, &QTableWidget::itemSelectionChanged, this, &client::onSelectionChanged);

    loginForm = new loginform(this);  // Создаем форму авторизации
    loginForm->show();  // Показываем форму авторизации

    // Ждем, пока форма авторизации не будет закрыта
    connect(loginForm, &loginform::loginDataEntered, this, &client::onLoginDataEntered);
    clientSocket->connectToHost("127.0.0.1", 12345);
}


client::~client(){
    delete ui;
}

void client::onSocketConnected()
{
    qDebug() << "Connected to server";
}

void client::onSocketDisconnected()
{
    qDebug() << "Disconnected from server";
}

void client::sendRequest()
{
    if (clientSocket->state() == QAbstractSocket::ConnectedState) {
        isFullAccessRequest = false; // Это обычный запрос
        // Создаём запрос с логином и паролем
        QString request = QString("GET_DATABASES|%1|%2")
                              .arg(username)
                              .arg(password);

        // Отправляем данные
        clientSocket->write(request.toUtf8());
    } else {
        qDebug() << "Socket is not connected!";
    }
}
void client::onGetButtonClicked()
{
    if (clientSocket->state() == QAbstractSocket::ConnectedState) {
        isFullAccessRequest = true; // Это запрос на полные права
        QString request = QString("GET_FULL_ACCESS_DATABASES|%1|%2").arg(username).arg(password);
        clientSocket->write(request.toUtf8());
    } else {
        qDebug() << "Socket is not connected!";
    }
}
void client::openButtonClicked()
{
    QString rootObject = getToLocalServer();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(rootObject.toUtf8());
    QJsonObject jsonObject = jsonDoc.object();
    // В любом случае обновляем дерево
    treeWidget->clear();
    tableWidget->clear();
    for (const QString &dbName : jsonObject.keys()) {
        QTreeWidgetItem *dbItem = new QTreeWidgetItem(treeWidget);
        dbItem->setText(0, dbName);

        QJsonObject dbObject = jsonObject.value(dbName).toObject();
        for (const QString &tableName : dbObject.keys()) {
            QTreeWidgetItem *tableItem = new QTreeWidgetItem(dbItem);
            tableItem->setText(0, tableName);
            tableItem->setData(0, Qt::UserRole, dbObject.value(tableName).toArray());
        }
    }
        treeWidget->expandAll();
}

void client::readServerResponse()
{
    static QByteArray buffer; // Буфер для накопления данных
    buffer.append(clientSocket->readAll());

    // Проверяем, является ли буфер корректным JSON
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(buffer, &parseError);

    if (parseError.error == QJsonParseError::NoError) {
        // Успешный разбор JSON
        buffer.clear(); // Очищаем буфер
        qDebug() << "Корректный JSON получен.";
        if (!jsonDoc.isObject()) {
            qDebug() << "Некорректный формат JSON.";
            return;
        }

        QJsonObject rootObject = jsonDoc.object();

        if (rootObject.contains("error")) {
            // Если есть ошибка, показываем сообщение
            QString errorMessage = rootObject["error"].toString();
            QMessageBox::warning(this, "Ошибка", errorMessage);
            return;
        }
        // Если это запрос с полными правами, сохраняем в локальную базу
        if (isFullAccessRequest) {
            saveDataToLocalDatabase(rootObject);
            QMessageBox::information(this, "Успех", "Данные успешно сохранены на локальном сервере.");
            return; // Завершаем метод, не обновляя дерево
        }

        // В любом случае обновляем дерево
        treeWidget->clear();
        tableWidget->clear();
        for (const QString &dbName : rootObject.keys()) {
            QTreeWidgetItem *dbItem = new QTreeWidgetItem(treeWidget);
            dbItem->setText(0, dbName);

            QJsonObject dbObject = rootObject.value(dbName).toObject();
            for (const QString &tableName : dbObject.keys()) {
                QTreeWidgetItem *tableItem = new QTreeWidgetItem(dbItem);
                tableItem->setText(0, tableName);
                tableItem->setData(0, Qt::UserRole, dbObject.value(tableName).toArray());
            }
        }

        treeWidget->expandAll();
    } else {
        qDebug() << "Ожидание дополнительных данных...";
    }
}
void client::saveDataToLocalDatabase(const QJsonObject &data)
{
    // Подключаемся к локальному PostgreSQL
    QSqlDatabase localDb = QSqlDatabase::addDatabase("QPSQL", "local_connection");
    localDb.setHostName("localhost");
    localDb.setPort(5433);
    localDb.setUserName("postgres"); // Укажите локального пользователя
    localDb.setPassword("8698"); // Укажите пароль для локального пользователя

    if (!localDb.open()) {
        qDebug() << "Не удалось подключиться к локальному серверу PostgreSQL:" << localDb.lastError().text();
        return;
    }

    QSqlQuery query(localDb);

    for (const QString &dbName : data.keys()) {
        QJsonObject dbObject = data.value(dbName).toObject();

        // Проверяем, существует ли база данных
        QString checkDbQuery = QString(
                                   "SELECT 1 FROM pg_database WHERE datname = '%1'").arg(dbName);
        if (!query.exec(checkDbQuery)) {
            qDebug() << "Ошибка при проверке существования базы данных:" << query.lastError().text();
            continue;
        }

        bool dbExists = query.next();

        if (!dbExists) {
            // Создаем новую базу данных, если она не существует
            if (!query.exec(QString("CREATE DATABASE \"%1\"").arg(dbName))) {
                qDebug() << "Ошибка при создании базы данных:" << query.lastError().text();
                continue;
            }
        }
        // Подключаемся к созданной базе данных
        QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", dbName);
        db.setHostName("localhost");
        db.setPort(5433);
        db.setUserName("postgres");
        db.setPassword("8698");
        db.setDatabaseName(dbName);

        if (!db.open()) {
            qDebug() << "Не удалось подключиться к базе данных:" << dbName << ":" << db.lastError().text();
            continue;
        }

        QSqlQuery tableQuery(db);

        for (const QString &tableName : dbObject.keys()) {
            QJsonArray tableData = dbObject.value(tableName).toArray();

            // Удаляем старую таблицу, если она существует
            tableQuery.exec(QString("DROP TABLE IF EXISTS \"%1\"").arg(tableName));

            if (tableData.isEmpty()) {
                continue; // Пропускаем пустые таблицы
            }

            // Создаем таблицу
            QJsonObject firstRow = tableData[0].toObject();
            QString createTableQuery = QString("CREATE TABLE \"%1\" (").arg(tableName);
            QStringList columns;

            for (const QString &columnName : firstRow.keys()) {
                columns.append(QString("\"%1\" TEXT").arg(columnName));
            }

            createTableQuery += columns.join(", ") + ")";
            if (!tableQuery.exec(createTableQuery)) {
                qDebug() << "Ошибка при создании таблицы:" << tableQuery.lastError().text();
                continue;
            }

            // Вставляем данные в таблицу
            for (const QJsonValue &rowValue : tableData) {
                QJsonObject row = rowValue.toObject();
                QString insertQuery = QString("INSERT INTO \"%1\" (").arg(tableName);

                QStringList columnNames = row.keys();
                QStringList columnValues;

                for (const QString &columnName : columnNames) {
                    columnValues.append(QString("'%1'").arg(row.value(columnName).toString().replace("'", "''")));
                }

                insertQuery += columnNames.join(", ") + ") VALUES (" + columnValues.join(", ") + ")";
                if (!tableQuery.exec(insertQuery)) {
                    qDebug() << "Ошибка при вставке данных в таблицу:" << tableQuery.lastError().text();
                }
            }
        }

        db.close();
    }

    localDb.close();
    qDebug() << "Данные успешно скопированы в локальный сервер PostgreSQL.";
}
void client::onPushToServerButtonClicked()
{
    if (clientSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Socket is not connected!";
        return;
    }
     QString jsonString = getToLocalServer();

    QString request = QString("PUSH_DATA|%1|%2|%3").arg(username).arg(password).arg(jsonString);
    clientSocket->write(request.toUtf8());
}
QString client::getToLocalServer(){
    // Подключение к локальному PostgreSQL
    QSqlDatabase localDb = QSqlDatabase::addDatabase("QPSQL", "local_connection");
    localDb.setHostName("localhost");
    localDb.setPort(5433);
    localDb.setUserName("postgres"); // Укажите локального пользователя
    localDb.setPassword("8698");     // Укажите пароль для локального пользователя

    if (!localDb.open()) {
        qDebug() << "Не удалось подключиться к локальному серверу PostgreSQL:" << localDb.lastError().text();
    }

    QSqlQuery query(localDb);
    QStringList databases;

    // Получение списка баз данных
    if (query.exec("SELECT datname FROM pg_database WHERE datistemplate = false")) {
        while (query.next()) {
            databases << query.value(0).toString();
        }
    } else {
        qDebug() << "Ошибка при получении списка баз данных:" << query.lastError().text();
    }

    QJsonObject dataToSend;

    for (const QString &dbName : databases) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", dbName);
        db.setHostName("localhost");
        db.setPort(5433);
        db.setUserName("postgres");
        db.setPassword("8698");
        db.setDatabaseName(dbName);

        if (!db.open()) {
            qDebug() << "Не удалось подключиться к базе данных:" << dbName << ":" << db.lastError().text();
            continue;
        }

        QSqlQuery tableQuery(db);
        if (!tableQuery.exec(
                "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'")) {
            qDebug() << "Ошибка при получении списка таблиц базы данных " << dbName << ":"
                     << tableQuery.lastError().text();
            continue;
        }

        QJsonObject dbObject;

        while (tableQuery.next()) {
            QString tableName = tableQuery.value(0).toString();
            QSqlQuery dataQuery(QString("SELECT * FROM \"%1\"").arg(tableName), db);

            QJsonArray tableDataArray;

            while (dataQuery.next()) {
                QJsonObject rowObject;
                for (int i = 0; i < dataQuery.record().count(); ++i) {
                    rowObject[dataQuery.record().fieldName(i)] = dataQuery.value(i).toString();
                }
                tableDataArray.append(rowObject);
            }

            dbObject[tableName] = tableDataArray;
        }

        dataToSend[dbName] = dbObject;
        db.close();
    }

    localDb.close();

    // Отправляем данные на сервер
    QJsonDocument jsonDoc(dataToSend);
    QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);
    return jsonString;
}
void client::onAddRowClicked()
{
    // Добавляем новую строку в таблицу
    int currentRowCount = tableWidget->rowCount();
    tableWidget->insertRow(currentRowCount);

    // Если нужно, можно инициализировать пустыми значениями ячейки новой строки
    for (int i = 0; i < tableWidget->columnCount(); ++i) {
        QTableWidgetItem *item = new QTableWidgetItem("");
        tableWidget->setItem(currentRowCount, i, item);
    }

    // Устанавливаем флаг изменений
    isTableModified = true;
}

void client::onDatabaseOrTableClicked(QTreeWidgetItem *item, int column)
{
    // Если таблица была изменена, предупреждаем пользователя
    if (isTableModified) {
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this, "Несохраненные данные",
            "Вы внесли изменения в текущую таблицу. Сохранить изменения?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Yes) {
            if (previousItem && previousItem != item) {
                // Сохраняем изменения перед переключением
                onSaveButtonClicked(true);
            }


        } else if (reply == QMessageBox::Cancel) {
            return; // Отменяем переход
        }
    }
    // Обновляем previousItem
    previousItem = item;
    // Сброс флага изменений
    isTableModified = false;

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid() || !data.canConvert<QJsonArray>()) {
        return; // Не таблица или данных нет
    }

    QJsonArray tableData = data.toJsonArray();

    // Отключаем сигналы на время изменения таблицы
    tableWidget->blockSignals(true);

    tableWidget->clear();
    if (tableData.isEmpty()) {
        tableWidget->blockSignals(false); // Включаем сигналы обратно
        return; // Таблица пуста
    }

    // Настраиваем заголовки таблицы
    QJsonObject firstRow = tableData[0].toObject();
    QStringList headers = firstRow.keys();
    tableWidget->setColumnCount(headers.size());
    tableWidget->setHorizontalHeaderLabels(headers);

    // Заполняем таблицу данными
    tableWidget->setRowCount(tableData.size());
    for (int i = 0; i < tableData.size(); ++i) {
        QJsonObject row = tableData[i].toObject();
        for (int j = 0; j < headers.size(); ++j) {
            QTableWidgetItem *item = new QTableWidgetItem(row.value(headers[j]).toString());
            tableWidget->setItem(i, j, item);
        }
    }

    // Включаем сигналы обратно
    tableWidget->blockSignals(false);

    tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);

    if (!addRowButton) {
        addRowButton = new QPushButton(this);
        addRowButton->setText("+");
        ui->verticalLayout->addWidget(addRowButton);
        connect(addRowButton, &QPushButton::clicked, this, &client::onAddRowClicked);
    }

    if (!saveButton) {
        saveButton = new QPushButton(this);
        saveButton->setText("Сохранить");
        ui->verticalLayout->addWidget(saveButton);
        connect(saveButton, &QPushButton::clicked, this, &client::onSaveButtonClicked);
    }
}


void client::onSaveButtonClicked(bool isTransition = false) {
    if (!tableWidget) {
        qDebug() << "Таблица не загружена.";
        return;
    }

   QTreeWidgetItem *currentItem = isTransition ? previousItem : treeWidget->currentItem();
    if (!currentItem || !currentItem->parent()) {
        QMessageBox::warning(this, "Ошибка", "Выберите таблицу для сохранения.");
        return;
    }

    QString tableName = currentItem->text(0);
    QString dbName = currentItem->parent()->text(0);

    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", "save_connection");
    db.setHostName("localhost");
    db.setPort(5433);
    db.setUserName("postgres");
    db.setPassword("8698");
    db.setDatabaseName(dbName);

    if (!db.open()) {
        qDebug() << "Не удалось подключиться к базе данных:" << dbName << ":" << db.lastError().text();
        return;
    }

    QSqlQuery query(db);

    if (!query.exec(QString("DELETE FROM \"%1\"").arg(tableName))) {
        qDebug() << "Ошибка при очистке таблицы:" << query.lastError().text();
        db.close();
        return;
    }

    QStringList headers;
    for (int col = 0; col < tableWidget->columnCount(); ++col) {
        headers << tableWidget->horizontalHeaderItem(col)->text();
    }

    QJsonArray updatedTableData;

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QStringList values;
        QJsonObject rowObject;

        for (int col = 0; col < tableWidget->columnCount(); ++col) {
            QString cellValue = tableWidget->item(row, col) ? tableWidget->item(row, col)->text() : "";
            values << QString("'%1'").arg(cellValue.replace("'", "''"));
            rowObject[headers[col]] = cellValue; // Обновляем JSON
        }

        QString insertQuery = QString("INSERT INTO \"%1\" (%2) VALUES (%3)")
                                  .arg(tableName)
                                  .arg(headers.join(", "))
                                  .arg(values.join(", "));

        if (!query.exec(insertQuery)) {
            qDebug() << "Ошибка при вставке данных в таблицу:" << query.lastError().text();
        }

        updatedTableData.append(rowObject); // Добавляем строку в JSON
    }

    db.close();

    // Обновляем данные в QTreeWidgetItem
    currentItem->setData(0, Qt::UserRole, updatedTableData);

    QMessageBox::information(this, "Успех", "Данные успешно сохранены в таблице.");
    isTableModified = false;
}

void client::onSelectionChanged() {
    // Проверяем, выделена ли хотя бы одна строка
    bool hasSelection = !tableWidget->selectedItems().isEmpty();

    if (hasSelection) {
        if (!deleteButton) {
            deleteButton = new QPushButton(this);
            deleteButton->setText("Удалить");
            ui->verticalLayout->addWidget(deleteButton);

            connect(deleteButton, &QPushButton::clicked, this, &client::onDeleteRowClicked);
        }
        deleteButton->show();
    } else if (deleteButton) {
        deleteButton->hide();
    }
}

void client::onDeleteRowClicked() {
    // Удаляем выделенные строки
    QList<QTableWidgetSelectionRange> selectedRanges = tableWidget->selectedRanges();

    for (const QTableWidgetSelectionRange &range : selectedRanges) {
        for (int row = range.bottomRow(); row >= range.topRow(); --row) {
            tableWidget->removeRow(row);
        }
    }

    // Убираем кнопку, если больше нет выделенных строк
    if (tableWidget->selectedItems().isEmpty() && deleteButton) {
        deleteButton->hide();
    }

    // Устанавливаем флаг изменения таблицы
    isTableModified = true;
}
void client::onLoginDataEntered(const QString &user, const QString &pass)
{
    // Сохраняем логин и пароль для дальнейшего использования
    username = user;
    password = pass;
    if (loginForm) {
        loginForm->close();  // Закрываем форму
    }
}
