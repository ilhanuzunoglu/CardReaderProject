#ifndef DETAILSDIALOG_H
#define DETAILSDIALOG_H

#include <QDialog>
#include <QByteArray>

QT_BEGIN_NAMESPACE
namespace Ui { class DetailsDialog; }
QT_END_NAMESPACE

class DetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DetailsDialog(QWidget *parent = nullptr);
    ~DetailsDialog();

    void setDetails(const QByteArray &raw, const QString &type, const QString &uid, const QString &sak, const QString &atq);

private:
    Ui::DetailsDialog *ui;
};

#endif // DETAILSDIALOG_H
