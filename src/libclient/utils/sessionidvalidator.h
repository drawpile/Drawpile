// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONIDVALIDATOR_H
#define SESSIONIDVALIDATOR_H

#include <QValidator>

class SessionIdAliasValidator final : public QValidator
{
	Q_OBJECT
public:
	explicit SessionIdAliasValidator(QObject *parent=nullptr);

	State validate(QString &input, int &pos) const override;
};

#endif // SESSIONIDVALIDATOR_H
