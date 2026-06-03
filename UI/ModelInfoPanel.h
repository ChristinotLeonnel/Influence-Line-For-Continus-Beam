#pragma once
#include <QWidget>
#include <QLabel>
#include <QTableWidget>
#include "JsonUtils.h"

// Displays structural model parameters (spans, EI, step, nodes count).
class ModelInfoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ModelInfoPanel(QWidget* parent = nullptr);
    void setPaths(const jutils::Paths& p);

private:
    void reload();
    void addKV(const QString& key, const QString& value, int row, QColor valColor = Qt::white);

    QLabel*       header_  = nullptr;
    QTableWidget* kvTable_ = nullptr;
    QTableWidget* spanTable_ = nullptr;
    jutils::Paths paths_;
};
