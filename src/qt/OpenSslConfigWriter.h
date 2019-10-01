//OpenSslConfigWriter.h by MFCoin developers
#pragma once
class CertLogger;
class QString;

class OpenSslConfigWriter {
	public:
		OpenSslConfigWriter(CertLogger*logger);
		static QString configPath();
		QString checkAndWrite();
	protected:
		CertLogger* _logger = 0;
		bool writeIfAbsent(const QString & subPath, const char* strContents, QString & error);
		static QString winConfigPart();
};
