// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTFILTERINPUT_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_SHORTCUTFILTERINPUT_H
#include <QWidget>

class QCheckBox;
class QLineEdit;

namespace dialogs {
namespace settingsdialog {

class ShortcutFilterInput : public QWidget {
    Q_OBJECT
public:
    ShortcutFilterInput(QWidget *parent = nullptr);

    bool isEmpty() const;

    void checkConflictBox();

signals:
    void filtered(const QString &text);
    void conflictBoxChecked(bool checked);

private:
    void handleFilterTextChanged(const QString &text);
    void handleConflictBoxStateChanged(int state);
    void updateFilterText(const QString &text);

    QLineEdit *m_filterEdit;
    QCheckBox *m_conflictBox;
    QString m_filterText;
};

}
}

#endif
