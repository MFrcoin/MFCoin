//Settings.cpp by MFCoin developers
#include "Settings.h"
#include <QStandardPaths>
#include <boost/filesystem.hpp>
#include "../util.h"
extern const char * const BITCOIN_CONF_FILENAME;

QString Settings::configPath() {
	std::string confPath = GetArg("-conf", BITCOIN_CONF_FILENAME);
	boost::filesystem::path boostPath = GetConfigFile(confPath);
	return QString::fromStdString(boostPath.string());
}
QDir Settings::configDir() {
	QDir dir = QFileInfo(configPath()).dir();
	Q_ASSERT(dir.exists());
	return dir;
}
QDir Settings::certDir() {
	QString str = configDir().absoluteFilePath("cert");
	str = QDir::toNativeSeparators(str);
	QDir dir = str;
	bool ok = dir.mkpath(".");
	Q_ASSERT(ok && dir.exists());
	return dir;
}
