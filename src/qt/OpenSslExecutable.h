//OpenSslExecutable.h by MFCoin developers
#pragma once
#include <QProcess>
class CertLogger;
class QDir;

class OpenSslExecutable: public QProcess {
	public:
		OpenSslExecutable();
		QString errorString()const;
		QString exec(const QStringList & args);
		bool generateKeyAndCertificateRequest(const QString & baseName, const QString & subj);
		bool generateCertificate(const QString & baseName, const QString & configDir);
		bool createCertificatePair(const QString & baseName, const QString & configDir, const QString & pass);
		bool encryptInfocardAes(const QByteArray& data, const QString & outFile, const QString & pass);
		void setLogger(CertLogger*l);
		QString log(const QString & s);
		static bool isFoundOrMessageBox();
	protected:
		struct SpecifyPathDialog;
		static QString s_path;
		QString _strOutput;
		CertLogger* _logger = 0;
		QByteArray _dataToWrite;
		bool existsOrExit(const QDir & dir, const QString & file);
		bool deleteOrExit(QDir & dir, const QString & file, int tries=5);
		void readToMe();
		static QString defaultPath();
		static bool seemsOk(const QString & path);
		static QString cfgFilePath();
};
