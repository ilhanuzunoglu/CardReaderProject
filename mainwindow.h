#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include <QByteArray> // QByteArray sınıfı için gerekli

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class DetailsDialog; // DetailsDialog sınıfının önden bildirimi (forward declaration)

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_refreshButton_clicked();
    void on_openButton_clicked();
    void on_detailsButton_clicked();
    void handlePoll();
    void on_cardDetected(bool success,
                         QByteArray raw,
                         QString type,
                         QString uid,
                         QString sak,
                         QString atq);

    // MIFARE Blok Okuma butonu için slot
    void on_readBlockButton_clicked();
    // Yeni eklenen kimlik doğrulama butonu için slot
    void on_authenticateButton_clicked();


private:
    void refreshPorts();
    void parseResponse(const QByteArray &resp,
                       bool &success,
                       QString &type,
                       QString &uid,
                       QString &sak,
                       QString &atq);

    // MIFARE yanıtlarını işlemek için yardımcı fonksiyon
    void processMifareResponse(const QByteArray &resp);

    // Longitudinal Redundancy Check (LRC) hesaplama fonksiyonu
    uchar calculateLRC(const QByteArray &data);

    Ui::MainWindow *ui;
    QSerialPort serial;
    QTimer pollTimer;
    DetailsDialog *detailsDialog; // Pointer olarak tanımlandığında sorun yok
};

#endif // MAINWINDOW_H
