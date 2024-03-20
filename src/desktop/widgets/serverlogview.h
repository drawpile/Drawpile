// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_SERVERLOGVIEW
#define DESKTOP_WIDGETS_SERVERLOGVIEW
#include <QListView>

namespace widgets {

class ServerLogView final : public QListView {
    Q_OBJECT
public:
    ServerLogView(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
};

}

#endif
