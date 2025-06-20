#include "detailsdialog.h"
#include "ui_detailsdialog.h" // Bu satırı ekledik

DetailsDialog::DetailsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DetailsDialog)
{
    ui->setupUi(this);
}

DetailsDialog::~DetailsDialog()
{
    delete ui;
}

void DetailsDialog::setDetails(const QByteArray &raw, const QString &type, const QString &uid, const QString &sak, const QString &atq)
{
    ui->rawText->setPlainText(raw.toHex(' ').toUpper());
    ui->typeLabel->setText(type);
    ui->uidLabel->setText(uid);
    ui->sakLabel->setText(sak);
    ui->atqLabel->setText(atq);
}
