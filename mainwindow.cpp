#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "detailsdialog.h" // detailsdialog.h dosyasının buraya dahil edildiğinden emin olun

#include <QSerialPortInfo>
#include <QMessageBox>
#include <QDebug>
#include <QByteArray>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , detailsDialog(new DetailsDialog(this)) // DetailsDialog artık tanınıyor
{
    ui->setupUi(this); // UI elemanlarını ayarlar
    ui->statusLabel->setText("Port kapalı"); // Başlangıç durumu
    refreshPorts(); // Mevcut seri portları yükler

    // Her 2 saniyede bir kart sorgulamak için zamanlayıcıyı ayarlar ve bağlar
    pollTimer.setInterval(2000);
    connect(&pollTimer, &QTimer::timeout, this, &MainWindow::handlePoll);

    // Yenile ve Detay butonlarını slotlarına bağlar
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::on_refreshButton_clicked);
    connect(ui->detailsButton, &QPushButton::clicked, this, &MainWindow::on_detailsButton_clicked);

    // MIFARE Blok Oku butonunu slotuna bağlar
    connect(ui->readBlockButton, &QPushButton::clicked, this, &MainWindow::on_readBlockButton_clicked);
    // Yeni Kimlik Doğrulama butonunu slotuna bağla
    connect(ui->authenticateButton, &QPushButton::clicked, this, &MainWindow::on_authenticateButton_clicked);

    // Varsayılan anahtar tipi ve numarası
    ui->keyTypeCombo->addItem("Key A", 0x00); // 0x00 for KeyA
    ui->keyTypeCombo->addItem("Key B", 0x04); // 0x04 for KeyB
    ui->keyNumberInput->setText("0"); // Varsayılan key #0
    ui->sectorNumberInput->setText("0"); // Varsayılan sektör #0
    ui->authStatusLabel->setText("Kimlik doğrulama bekleniyor.");
}

MainWindow::~MainWindow()
{
    // Uygulama kapanırken zamanlayıcıyı durdurur ve seri portu kapatır
    pollTimer.stop();
    serial.close();
    delete ui;
}

void MainWindow::refreshPorts()
{
    // Mevcut seri portları temizler ve günceller
    ui->portCombo->clear();
    for (const auto &info : QSerialPortInfo::availablePorts())
        ui->portCombo->addItem(info.portName());
}

void MainWindow::on_refreshButton_clicked()
{
    // "Portları Yenile" butonuna tıklandığında port listesini günceller
    refreshPorts();
}

void MainWindow::on_openButton_clicked()
{
    qDebug() << "--- on_openButton_clicked() --- önce isOpen() =" << serial.isOpen();

    if (!serial.isOpen()) {
        // Port kapalıysa, seçilen portu açmaya çalışır
        QString portName = ui->portCombo->currentText();
        qDebug() << "Açılan port:" << portName;
        serial.setPortName(portName);
        serial.setBaudRate(QSerialPort::Baud115200); // Baud oranı
        serial.setDataBits(QSerialPort::Data8); // Veri bitleri
        serial.setParity(QSerialPort::NoParity); // Eşlik biti
        serial.setStopBits(QSerialPort::OneStop); // Durdurma bitleri
        serial.setFlowControl(QSerialPort::NoFlowControl); // Akış kontrolü

        bool ok = serial.open(QIODevice::ReadWrite); // Portu oku/yaz modunda aç
        qDebug() << "serial.open() sonucu:" << ok << ", hata:" << serial.errorString();
        if (!ok) {
            // Port açılamazsa hata mesajı göster
            QMessageBox::critical(this, "Hata", "Port açılamadı: " + serial.errorString());
            return;
        }

        // Port açıldığında UI'yı günceller
        ui->openButton->setText("Portu Kapat");
        ui->statusLabel->setText("Port açık, okuma başladı");
        pollTimer.start(); // Kart sorgulama zamanlayıcısını başlatır
        qDebug() << "Port açıldı ve polling başladı.";

    } else {
        // Port açıksa, portu kapatır
        qDebug() << "Port kapatılıyor";
        pollTimer.stop(); // Kart sorgulama zamanlayıcısını durdurur
        serial.close(); // Seri portu kapatır
        ui->openButton->setText("Portu Aç"); // UI'yı günceller
        ui->statusLabel->setText("Port kapalı");
        qDebug() << "Port kapatıldı ve polling durduruldu.";
    }
}

void MainWindow::handlePoll()
{
    // Port açık değilse işlem yapmaz
    if (!serial.isOpen())
        return;

    // POLL A PICC komutunu gönderir (protokol belgesine göre 020A003EDF7E01009603)
    static const QByteArray cmd = QByteArray::fromHex("020A003EDF7E01009603");
    serial.write(cmd);
    if (!serial.waitForBytesWritten(100)) // Komutun yazılmasını 100ms bekler
        return;

    // Okuyucudan gelen yanıtı okur
    QByteArray resp;
    if (serial.waitForReadyRead(500)) { // İlk verinin gelmesini 500ms bekler
        resp = serial.readAll();
        while (serial.waitForReadyRead(50)) // Ek verilerin gelmesini 50ms aralıklarla bekler
            resp += serial.readAll();
    }

    // Yanıtı ayrıştırır ve on_cardDetected slotunu çağırır
    bool success;
    QString type, uid, sak, atq;
    parseResponse(resp, success, type, uid, sak, atq);
    on_cardDetected(success, resp, type, uid, sak, atq);
}

void MainWindow::parseResponse(const QByteArray &resp,
                               bool &success,
                               QString &type,
                               QString &uid,
                               QString &sak,
                               QString &atq)
{
    // Yanıtta başarılı şablon (FF 01) olup olmadığını kontrol eder
    success = resp.contains(QByteArray::fromHex("FF01"));

    // TLV (Tag-Length-Value) yapısından belirli tag'leri (etiketleri) çıkarır
    auto extract = [&](uchar t1, uchar t2) -> QString {
        for (int i = 0; i + 3 < resp.size(); ++i) {
            if ((uchar)resp[i] == t1 && (uchar)resp[i+1] == t2) {
                int len = (uchar)resp[i+2]; // Uzunluk baytı
                // Çıkarılacak verinin paket sınırları içinde olduğundan emin olur
                if (i + 3 + len <= resp.size()) {
                    return resp.mid(i+3, len).toHex(' ').toUpper(); // Veriyi hex formatında döndürür
                }
            }
        }
        return "-"; // Eğer tag bulunamazsa "-" döndürür
    };

    if (success) {
        // Başarılıysa kart bilgilerini çıkarır
        type = extract(0xDF, 0x16); // PICC Tipi
        uid  = extract(0xDF, 0x0D); // PICC UID
        sak  = extract(0xDF, 0x6B); // PICC SAK
        atq  = extract(0xDF, 0x15); // PICC ATQ
    } else {
        // Başarısızsa tüm alanları "-" olarak ayarlar
        type = uid = sak = atq = "-";
    }
}

void MainWindow::on_cardDetected(bool success,
                                 QByteArray raw,
                                 QString type,
                                 QString uid,
                                 QString sak,
                                 QString atq)
{
    // Kart algılama durumuna göre UI'daki durum ve kart bilgilerini günceller
    ui->statusLabel->setText(success ? "Kart okuma başarılı" : "Kart okunamadı");
    ui->typeLabel->setText(type);
    ui->uidLabel->setText(uid);
    ui->sakLabel->setText(sak);
    ui->atqLabel->setText(atq);

    // Detay penceresini ve ana penceredeki ham veri metin alanını günceller
    detailsDialog->setDetails(raw, type, uid, sak, atq);
    ui->detailsText->appendPlainText(raw.toHex(' ').toUpper() + "\n"); // Ham veriyi ekler ve yeni satıra geçer
}

void MainWindow::on_detailsButton_clicked()
{
    // Detay penceresini gösterir
    detailsDialog->show();
}

// LRC (Longitudinal Redundancy Check) hesaplama fonksiyonu
// Protokol belgesine göre, LRC paketin STX dışındaki tüm baytlarının XOR'udur.
uchar MainWindow::calculateLRC(const QByteArray &data)
{
    uchar lrc = 0x00;
    // STX'ten sonraki baytları XOR ile toplar
    for (int i = 0; i < data.size(); ++i) {
        lrc ^= (uchar)data.at(i);
    }
    return lrc;
}


void MainWindow::on_readBlockButton_clicked()
{
    // Port açık değilse uyarı mesajı gösterir
    if (!serial.isOpen()) {
        QMessageBox::warning(this, "Uyarı", "Port açık değil. Lütfen önce portu açın.");
        return;
    }

    // Blok numarasını giriş alanından alır ve geçerliliğini kontrol eder
    int blockNumber = ui->blockNumberInput->text().toInt();
    if (blockNumber < 0 || blockNumber > 255) {
        QMessageBox::warning(this, "Hata", "Geçersiz blok numarası. Lütfen 0-255 arası bir değer girin.");
        return;
    }

    // MIFARE READ BLOCK Komutu Oluşturma (Protokol Belgesi Bölüm 4.3.1)
    QByteArray dataField;
    dataField.append((char)0xDF); // TAG1: Tag Byte 1
    dataField.append((char)0x78); // TAG2: Tag Byte 2 (MIFARE Komut TAG'ı)
    dataField.append((char)0x02); // Length: MIFARE komut verisinin uzunluğu (0xA5 + Blok Numarası = 2 byte)
    dataField.append((char)0xA5); // Value: MIFARE READ BLOCK Komutu (CMD)
    dataField.append((char)blockNumber); // Value: Okunacak Blok Numarası

    QByteArray packetBody;
    packetBody.append((char)0x00); // PCB: Protokol Kontrol Baytı (genellikle 0x00)
    packetBody.append((char)0x3E); // INS: Instruction Code (DO komutu için 0x3E)
    packetBody.append(dataField); // DATA: Oluşturulan TLV yapısındaki veri alanı

    QByteArray command;
    command.append((char)0x02); // STX: Paketin başlangıç işareti
    // Düzeltilen Kısım: LEN değeri packetBody'nin boyutu olmalı
    command.append((char)packetBody.size()); // LEN
    command.append(packetBody); // PCB, INS, DATA alanlarını komuta ekler

    uchar lrc = calculateLRC(command.mid(1));
    command.append((char)lrc); // Hesaplanan LRC'yi komuta ekler
    command.append((char)0x03); // ETX

    qDebug() << "MIFARE READ BLOCK Komutu Gönderiliyor:" << command.toHex(' ').toUpper();
    serial.write(command);

    if (!serial.waitForBytesWritten(200)) {
        QMessageBox::critical(this, "Hata", "MIFARE Blok okuma komutu gönderilemedi veya zaman aşımına uğradı.");
        return;
    }

    QByteArray resp;
    if (serial.waitForReadyRead(3000)) { // 3 saniye yanıt bekle
        resp = serial.readAll();
        while (serial.waitForReadyRead(50))
            resp += serial.readAll();
    } else {
        QMessageBox::warning(this, "Uyarı", "MIFARE Blok okuma yanıtı alınamadı (Zaman Aşımı).");
        ui->blockDataDisplay->setPlainText("MIFARE Blok okuma yanıtı alınamadı (Zaman Aşımı).");
        return;
    }

    qDebug() << "MIFARE READ BLOCK Yanıtı Alındı:" << resp.toHex(' ').toUpper();
    processMifareResponse(resp); // Yanıtı işleme fonksiyonunu çağırır
}


// ... (diğer kodlar) ...

void MainWindow::on_authenticateButton_clicked()
{
    if (!serial.isOpen()) {
        QMessageBox::warning(this, "Uyarı", "Port açık değil. Lütfen önce portu açın.");
        ui->authStatusLabel->setText("Kimlik doğrulama başarısız: Port kapalı.");
        return;
    }

    uchar keyType = static_cast<uchar>(ui->keyTypeCombo->currentData().toUInt());
    int keyNumber = ui->keyNumberInput->text().toInt();
    int sectorNumber = ui->sectorNumberInput->text().toInt();

    if (keyNumber < 0 || keyNumber > 1) {
        QMessageBox::warning(this, "Hata", "Geçersiz anahtar numarası. Lütfen 0 veya 1 girin.");
        ui->authStatusLabel->setText("Kimlik doğrulama başarısız: Geçersiz anahtar numarası.");
        return;
    }
    if (sectorNumber < 0 || sectorNumber > 39) {
        QMessageBox::warning(this, "Hata", "Geçersiz sektör numarası. Lütfen 0-39 arası bir değer girin.");
        ui->authStatusLabel->setText("Kimlik doğrulama başarısız: Geçersiz sektör numarası.");
        return;
    }

    // ADIM 1: MIFARE STD LOAD NEW KEY komutunu gönder (0xA9)
    QByteArray defaultKey = QByteArray::fromHex("FFFFFFFFFFFF"); // 6 baytlık varsayılan anahtar

    QByteArray loadKeyDataField;
    loadKeyDataField.append((char)0xDF);
    loadKeyDataField.append((char)0x78);
    loadKeyDataField.append((char)0x0F); // Length: MIFARE komut verisinin uzunluğu (0xA9 + MODE + KEY# + RFU[6] + KEY[6] = 15 byte)
    loadKeyDataField.append((char)0xA9); // Value: MIFARE STD LOAD NEW KEY Komutu (CMD)
    loadKeyDataField.append((char)keyType);
    loadKeyDataField.append((char)keyNumber);
    loadKeyDataField.append(QByteArray::fromHex("FFFFFFFFFFFF")); // RFU[6]
    loadKeyDataField.append(defaultKey); // KEY[6]

    QByteArray loadKeyPacketBody;
    loadKeyPacketBody.append((char)0x00); // PCB
    loadKeyPacketBody.append((char)0x3E); // INS: DO
    loadKeyPacketBody.append(loadKeyDataField);

    QByteArray loadKeyCommand;
    loadKeyCommand.append((char)0x02); // STX
    // Düzeltilen Kısım: LEN değeri loadKeyPacketBody'nin boyutu olmalı
    loadKeyCommand.append((char)loadKeyPacketBody.size()); // LEN
    loadKeyCommand.append(loadKeyPacketBody);

    uchar loadKeyLrc = calculateLRC(loadKeyCommand.mid(1));
    loadKeyCommand.append((char)loadKeyLrc);
    loadKeyCommand.append((char)0x03); // ETX

    qDebug() << "MIFARE LOAD NEW KEY Komutu Gönderiliyor:" << loadKeyCommand.toHex(' ').toUpper();
    serial.write(loadKeyCommand);

    if (!serial.waitForBytesWritten(200)) {
        QMessageBox::critical(this, "Hata", "Anahtar yükleme komutu gönderilemedi veya zaman aşımına uğradı.");
        ui->authStatusLabel->setText("Anahtar yükleme başarısız: Komut gönderilemedi.");
        return;
    }

    QByteArray loadKeyResp;
    if (serial.waitForReadyRead(2000)) { // 2 saniye yanıt bekle
        loadKeyResp = serial.readAll();
        while (serial.waitForReadyRead(50))
            loadKeyResp += serial.readAll();
    } else {
        QMessageBox::warning(this, "Uyarı", "Anahtar yükleme yanıtı alınamadı (Zaman Aşımı). Kimlik doğrulama yapılamadı.");
        ui->authStatusLabel->setText("Anahtar yükleme başarısız: Yanıt alınamadı (Zaman Aşımı).");
        return;
    }

    qDebug() << "MIFARE LOAD NEW KEY Yanıtı Alındı:" << loadKeyResp.toHex(' ').toUpper();

    // Anahtar yükleme yanıtını işleme
    bool loadKeySuccess = false;
    if (loadKeyResp.size() >= 7 && (uchar)loadKeyResp.at(0) == 0x02 && (uchar)loadKeyResp.at(loadKeyResp.size() - 1) == 0x03) {
        uchar respTemplateTag1 = (uchar)loadKeyResp.at(5);
        uchar respTemplateTag2 = (uchar)loadKeyResp.at(6);
        if (respTemplateTag1 == 0xFF && respTemplateTag2 == 0x01) {
            loadKeySuccess = true;
            qDebug() << "Anahtar yükleme başarılı.";
        } else if (respTemplateTag1 == 0xFF && respTemplateTag2 == 0x03) {
            // Hata yanıtını ayrıştır
            QByteArray errorDataPart = loadKeyResp.mid(8);
            if (errorDataPart.size() >= 4 && (uchar)errorDataPart.at(0) == 0xDF && (uchar)errorDataPart.at(1) == 0x78) {
                uchar errorCode = (uchar)errorDataPart.at(3); // DF 78 LEN ERROR_CODE
                ui->authStatusLabel->setText(QString("Anahtar yükleme başarısız: Hata Kodu 0x%1. Yanıt: %2")
                                                 .arg(QString::number(errorCode, 16).toUpper())
                                                 .arg(QString(loadKeyResp.toHex(' ').toUpper())));
            } else {
                 ui->authStatusLabel->setText("Anahtar yükleme başarısız: Hata yanıtı ayrıştırılamadı.");
            }
            return;
        }
    }

    if (!loadKeySuccess) {
        ui->authStatusLabel->setText("Anahtar yükleme başarısız: Beklenmeyen yanıt formatı. Kimlik doğrulama yapılamadı.");
        return;
    }

    // ADIM 2: MIFARE STD AUTHENTICATE SECTOR Komutu Oluşturma (0xB0)
    QByteArray authDataField;
    authDataField.append((char)0xDF);
    authDataField.append((char)0x78);
    authDataField.append((char)0x04); // Length: MIFARE komut verisinin uzunluğu (0xB0 + MODE + KEY# + SECTOR# = 4 byte)
    authDataField.append((char)0xB0); // Value: MIFARE AUTHENTICATE SECTOR Komutu (CMD)
    authDataField.append((char)keyType);
    authDataField.append((char)keyNumber);
    authDataField.append((char)sectorNumber);

    QByteArray authPacketBody;
    authPacketBody.append((char)0x00); // PCB
    authPacketBody.append((char)0x3E); // INS: DO
    authPacketBody.append(authDataField);

    QByteArray authCommand;
    authCommand.append((char)0x02); // STX
    // Düzeltilen Kısım: LEN değeri authPacketBody'nin boyutu olmalı
    authCommand.append((char)authPacketBody.size()); // LEN
    authCommand.append(authPacketBody);

    uchar authLrc = calculateLRC(authCommand.mid(1));
    authCommand.append((char)authLrc);
    authCommand.append((char)0x03); // ETX

    qDebug() << "MIFARE AUTHENTICATE Komutu Gönderiliyor:" << authCommand.toHex(' ').toUpper();
    serial.write(authCommand);

    if (!serial.waitForBytesWritten(200)) {
        QMessageBox::critical(this, "Hata", "Kimlik doğrulama komutu gönderilemedi veya zaman aşımına uğradı.");
        ui->authStatusLabel->setText("Kimlik doğrulama başarısız: Komut gönderilemedi.");
        return;
    }

    QByteArray authResp;
    if (serial.waitForReadyRead(3000)) { // 3 saniye yanıt bekle
        authResp = serial.readAll();
        while (serial.waitForReadyRead(50))
            authResp += serial.readAll();
    } else {
        QMessageBox::warning(this, "Uyarı", "Kimlik doğrulama yanıtı alınamadı (Zaman Aşımı).");
        ui->authStatusLabel->setText("Kimlik doğrulama başarısız: Yanıt alınamadı (Zaman Aşımı).");
        return;
    }

    qDebug() << "MIFARE AUTHENTICATE Yanıtı Alındı:" << authResp.toHex(' ').toUpper();

    // Kimlik doğrulama yanıtını işleme
    if (authResp.isEmpty() || (uchar)authResp.at(0) != 0x02 || (uchar)authResp.at(authResp.size() - 1) != 0x03) {
        ui->authStatusLabel->setText("Kimlik doğrulama başarısız: Geçersiz yanıt formatı.");
        return;
    }

    uchar receivedLRC_auth = (uchar)authResp.at(authResp.size() - 2);
    QByteArray dataForLRC_auth = authResp.mid(1, authResp.size() - 3);
    uchar calculatedLRC_auth = calculateLRC(dataForLRC_auth);

    if (receivedLRC_auth != calculatedLRC_auth) {
        ui->authStatusLabel->setText(QString("Kimlik doğrulama başarısız: LRC hatası. Alınan: 0x%1, Hesaplanan: 0x%2. Yanıt: %3")
                                         .arg(QString::number(receivedLRC_auth, 16).toUpper())
                                         .arg(QString::number(calculatedLRC_auth, 16).toUpper())
                                         .arg(QString(authResp.toHex(' ').toUpper())));
        return;
    }

    if (authResp.size() >= 7 && (uchar)authResp.at(5) == 0xFF) {
        uchar templateTag2 = (uchar)authResp.at(6);

        if (templateTag2 == 0x01) { // Başarılı Kimlik Doğrulama
            ui->authStatusLabel->setText("Kimlik doğrulama başarılı!");
            qDebug() << "Kimlik doğrulama başarılı.";
        } else if (templateTag2 == 0x03) { // Hata Yanıtı
            QByteArray errorDataPart = authResp.mid(8);
            if (errorDataPart.size() >= 4 && (uchar)errorDataPart.at(0) == 0xDF && (uchar)errorDataPart.at(1) == 0x78) {
                uchar errorCode = (uchar)errorDataPart.at(3); // DF 78 LEN ERROR_CODE
                QString errorMsg;
                if (errorCode == 0x08) {
                    errorMsg = "Kimlik Doğrulama Hatası (Yanlış Anahtar/Sektör)";
                } else if (errorCode == 0x05) { // Protokolde belirtilen genel hata kodu
                    errorMsg = "Genel İşlem Hatası";
                }
                else {
                    errorMsg = "Bilinmeyen Hata Kodu";
                }
                ui->authStatusLabel->setText(QString("Kimlik doğrulama başarısız: %1 (Hata Kodu 0x%2). Tam Yanıt: %3")
                                                 .arg(errorMsg)
                                                 .arg(QString::number(errorCode, 16).toUpper())
                                                 .arg(QString(authResp.toHex(' ').toUpper())));
                qDebug() << "Kimlik doğrulama hatası. Kod:" << QString::number(errorCode, 16).toUpper();
            } else {
                ui->authStatusLabel->setText("Kimlik doğrulama başarısız: Hata yanıtı ayrıştırılamadı.");
            }
        } else {
            ui->authStatusLabel->setText("Kimlik doğrulama başarısız: Bilinmeyen yanıt şablonu (FF xx).");
        }
    } else {
        ui->authStatusLabel->setText("Kimlik doğrulama başarısız: Yanıt şablonu kontrolü için yetersiz veri veya FF ile başlamıyor.");
    }
}

// ... (diğer kodlar) ...

// MIFARE komut yanıtlarını işleme fonksiyonu (Blok Okuma için)
void MainWindow::processMifareResponse(const QByteArray &resp)
{
    if (resp.isEmpty()) {
        ui->blockDataDisplay->setPlainText("Boş yanıt alındı.");
        return;
    }
    if ((uchar)resp.at(0) != 0x02 || (uchar)resp.at(resp.size() - 1) != 0x03) {
        ui->blockDataDisplay->setPlainText("Geçersiz yanıt formatı (STX/ETX hatalı): " + resp.toHex(' ').toUpper());
        return;
    }
    if (resp.size() < 8) {
        ui->blockDataDisplay->setPlainText("Yanıt çok kısa: " + resp.toHex(' ').toUpper());
        return;
    }

    uchar receivedLRC = (uchar)resp.at(resp.size() - 2);
    QByteArray dataForLRC = resp.mid(1, resp.size() - 3);
    uchar calculatedLRC = calculateLRC(dataForLRC);

    if (receivedLRC != calculatedLRC) {
        ui->blockDataDisplay->setPlainText(QString("LRC hatası. Alınan: 0x%1, Hesaplanan: 0x%2. Yanıt: %3")
                                               .arg(QString::number(receivedLRC, 16).toUpper())
                                               .arg(QString::number(calculatedLRC, 16).toUpper())
                                               .arg(QString(resp.toHex(' ').toUpper())));
        return;
    }

    uchar ins = (uchar)resp.at(4);
    if (ins != 0x3E) {
         ui->blockDataDisplay->setPlainText("Beklenmedik INS kodu: " + QString::number(ins, 16).toUpper() + ". Yanıt: " + resp.toHex(' ').toUpper());
         return;
    }

    if (resp.size() >= 6) {
        uchar templateTag1 = (uchar)resp.at(5);
        uchar templateTag2 = (uchar)resp.at(6);

        if (templateTag1 == 0xFF) {
            if (templateTag2 == 0x01) { // Başarılı Yanıt (SUCCESS TEMPLATE)
                QByteArray dataPart = resp.mid(8);
                int df78Index = -1;
                if (dataPart.size() >= 2 && (uchar)dataPart.at(0) == 0xDF && (uchar)dataPart.at(1) == 0x78) {
                    df78Index = 0;
                }

                if (df78Index != -1 && (df78Index + 2 < dataPart.size())) {
                    int tagLen = (uchar)dataPart.at(df78Index + 2);
                    if (df78Index + 3 + tagLen <= dataPart.size()) {
                        QByteArray mifareData = dataPart.mid(df78Index + 3, tagLen);

                        if (!mifareData.isEmpty() && (uchar)mifareData.at(0) == 0xA5) { // MIFARE READ BLOCK yanıtı
                            if (mifareData.size() >= 17) { // A5 (1 byte) + Blok Verisi (16 byte)
                                QByteArray blockData = mifareData.mid(1);
                                ui->blockDataDisplay->setPlainText("Okunan Blok Verisi:\n" + blockData.toHex(' ').toUpper());
                                ui->statusLabel->setText("MIFARE Blok okuma başarılı.");
                            } else {
                                ui->blockDataDisplay->setPlainText("Yanıt eksik MIFARE blok verisi. Mifare Data: " + mifareData.toHex(' ').toUpper());
                            }
                        } else {
                            ui->blockDataDisplay->setPlainText("Beklenmeyen MIFARE komut yanıtı (0xA5 bekleniyordu). Mifare Data: " + mifareData.toHex(' ').toUpper());
                        }
                    } else {
                        ui->blockDataDisplay->setPlainText("DF 78 tag uzunluk hatası veya veri paketi dışında. Data Part: " + dataPart.toHex(' ').toUpper());
                    }
                } else {
                    ui->blockDataDisplay->setPlainText("Yanıtta DF 78 MIFARE TAG bulunamadı veya hatalı konumda. Yanıt Data Part: " + dataPart.toHex(' ').toUpper());
                }

            } else if (templateTag2 == 0x03) { // Hata Yanıtı (ERROR TEMPLATE)
                QByteArray errorDataPart = resp.mid(8);
                if (errorDataPart.size() >= 4 && (uchar)errorDataPart.at(0) == 0xDF && (uchar)errorDataPart.at(1) == 0x78) {
                    uchar errorCode = (uchar)errorDataPart.at(3); // DF 78 LEN ERROR_CODE
                    QString errorMsg;
                    if (errorCode == 0x08) {
                        errorMsg = "Kimlik Doğrulama Hatası (Blok okuma için yanlış anahtar/sektör)";
                    } else {
                        errorMsg = "Bilinmeyen Hata Kodu";
                    }
                    ui->blockDataDisplay->setPlainText(QString("MIFARE Blok okuma hatası: %1 (Hata Kodu 0x%2). Tam Yanıt: %3")
                                                           .arg(errorMsg)
                                                           .arg(QString::number(errorCode, 16).toUpper())
                                                           .arg(QString(resp.toHex(' ').toUpper())));
                    ui->statusLabel->setText("MIFARE Blok okunamadı (Hata).");
                } else {
                    ui->blockDataDisplay->setPlainText("MIFARE Blok okuma hatası (Hata yanıtı ayrıştırılamadı). Yanıt: " + resp.toHex(' ').toUpper());
                    ui->statusLabel->setText("MIFARE Blok okunamadı (Hata).");
                }
            } else {
                ui->blockDataDisplay->setPlainText("Bilinmeyen yanıt şablonu (FF xx): " + resp.toHex(' ').toUpper());
            }
        } else {
            ui->blockDataDisplay->setPlainText("Yanıt şablonu FF ile başlamıyor: " + resp.toHex(' ').toUpper());
        }
    } else {
        ui->blockDataDisplay->setPlainText("Yanıt şablonu kontrolü için yetersiz veri: " + resp.toHex(' ').toUpper());
    }
}
