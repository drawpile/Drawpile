/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */
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
