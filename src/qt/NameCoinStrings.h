#pragma once
#include <QObject>

class NameCoinStrings: public QObject {
	public:
		static QString nameExistOrError(const QString & name);
		static QString nameExistOrError(const QString & name, const QString & prefix);
		static QString trNameNotFound(const QString & name);
		static QString trNameIsFree(const QString & name, bool ok = true);
		static QString trNameIsMy(const QString & name);
		static QString trNameAlreadyRegistered(const QString & name, bool ok);

		static const QChar charCheckOk;//like checkbox
		static const QChar charX;//cross like X
		static QChar charBy(bool ok);
};
