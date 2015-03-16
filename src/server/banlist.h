#ifndef BANLIST_H
#define BANLIST_H


class BanList : public QObject
{
	Q_OBJECT
public:
	explicit BanList(QObject *parent = 0);
	~BanList();

signals:

public slots:
};

#endif // BANLIST_H
