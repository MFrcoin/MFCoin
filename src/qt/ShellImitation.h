//ShellImitation.h by MFCoin developers
#pragma once
#include "CertLogger.h"
#include <QPointer>

struct ShellImitation {
	ShellImitation(CertLogger* logger);
	void maybeLog(const QString & s);

	bool touch(const QDir & dir, const QString & fileName, QString & err);
	bool mkpath(const QDir & dir, const QString & path, QString & error, int tries = 5);
	bool write(const QString & path, const QByteArray & what, QString &err);
	bool removeRecursiveFilesOnly(QDir & dir, QString &err);
	bool remove(const QString &file);
	protected:
		static QString tr(const char*c);
		QPointer<CertLogger> _logger;
};
using Shell = ShellImitation;
