//Settings.h by MFCoin developers
#pragma once
#include <QDir>

class Settings {
	public:
		static QDir configDir();
		static QString configPath();
		static QDir certDir();
};
