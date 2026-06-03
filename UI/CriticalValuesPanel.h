#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include "JsonUtils.h"

// Displays the 03_Critical_Values JSON files as a sortable table.
class CriticalValuesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CriticalValuesPanel(QWidget* parent = nullptr);
    void setPaths(const jutils::Paths& p);

private:
    void reload();
    QTableWidget* table_;
    QLabel*       header_;
    jutils::Paths paths_;
};
