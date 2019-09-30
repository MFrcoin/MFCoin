//CertLogger.h by MFCoin developers
#pragma once
#include <QTextBrowser>
#include <QFile>
class QDir;

class CertLogger: public QTextBrowser {
	public:
		CertLogger();
		void append(const QString & s);
		void setFile(const QString & path);//read from and log to
		void setFileNear(const QDir & dir, const QString & fileName);
		void clear(bool removeFile = false);
	protected:
		QFile _file;
};
