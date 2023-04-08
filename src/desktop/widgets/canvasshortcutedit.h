// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CANVASSHORTCUTEDIT_H
#define CANVASSHORTCUTEDIT_H

#include <QSet>
#include <QWidget>
#include <Qt>

class QLabel;
class QLineEdit;
class QPushButton;

namespace widgets {

class CanvasShortcutEdit final : public QWidget {
	Q_OBJECT
public:
	explicit CanvasShortcutEdit(QWidget *parent = nullptr);

	Qt::KeyboardModifiers mods() const;
	void setMods(Qt::KeyboardModifiers mods);

	const QSet<Qt::Key> keys() const;
	void setKeys(const QSet<Qt::Key> &keys);

	Qt::MouseButton button() const;
	void setButton(Qt::MouseButton button);

	unsigned int type() const;
	void setType(unsigned int type);

signals:
	void shortcutChanged();

protected:
	bool event(QEvent *event) override;
	bool eventFilter(QObject *obj, QEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;

private slots:
	void toggleEdit();

private:
	void setCapturing(bool capturing, bool apply);
	static bool looksLikeValidKey(int key);
	bool haveValidCapture();
	void updateShortcut();
	void updateLineEditText();
	void updateEditButton();
	void updateDescription();

	bool m_capturing;
	unsigned int m_type;
	Qt::KeyboardModifiers m_mods;
	QSet<Qt::Key> m_keys;
	Qt::MouseButton m_button;
	Qt::KeyboardModifiers m_capturedMods;
	QSet<Qt::Key> m_capturedKeys;
	Qt::MouseButton m_capturedButton;
	QLineEdit *m_lineEdit;
	QPushButton *m_editButton;
	QLabel *m_description;
};

}

#endif
