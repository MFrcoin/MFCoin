//ConfigFile.cpp by mfcoin developers
#include "ConfigFile.h"
#include <QTextCodec>
#include <QDir>
#include <QVariant>

std::string getConfigFile();

ConfigFile::ConfigFile(): QFile(path()), _fileName(path()) {
}
QString ConfigFile::path() {
	return QString::fromStdString(getConfigFile());
}
QString ConfigFile::load() {
	close();
	if(!open(QFile::ReadOnly)) {
		return tr("Can't open config file %1: %2").arg(QDir::toNativeSeparators(_fileName), errorString());
	}
	auto arr = readAll();
	auto codec = QTextCodec::codecForName("UTF-8");
	QString str = codec->toUnicode(arr);
	_lines = str.split('\n');
	return {};
}
QString ConfigFile::save() {
	close();
	if(!open(QFile::WriteOnly|QFile::Truncate)) {
		return tr("Can't open %1 for writing: %2")
			.arg(QDir::toNativeSeparators(_fileName)).arg(errorString());
	}
	auto arr = _lines.join('\n').toUtf8();
	const qint64 written = write(arr);
	if(written!=arr.count())
		return tr("Can't write all data to %1: %2")
			.arg(QDir::toNativeSeparators(_fileName)).arg(errorString());
	return {};
}
QVariant ConfigFile::option(const QString& name, const QString& strDefault)const {
	if(name.isEmpty()) {
		Q_ASSERT(0);
		return {};
	}
	const QString start = name + '=';
	for(auto& line: _lines) {
		if(line.startsWith(start)) {
			return line.mid(start.length());
		}
	}
	return strDefault;
}
void ConfigFile::setOption(const QString& name, const QString& value) {
	const QString start = name + '=';
	for(auto& line: _lines) {
		if(line.startsWith(start)) {
			line = start + value;
			return;
		}
	}
	_lines.append(start + value);
}
void ConfigFile::setOption(const QString& name, int n) {
	setOption(name, QString::number(n));
}
bool ConfigFile::server()const {
	return option(QStringLiteral("server")).toInt();
}
bool ConfigFile::listen()const {
	return option(QStringLiteral("listen")).toInt();
}
int ConfigFile::randPayTimeout()const {
	return option(QStringLiteral("rp_timeout"), "30").toInt();
}
double ConfigFile::randPayMaxAmount()const {
	return option(QStringLiteral("rp_max_amount")).toDouble();
}
double ConfigFile::randPayMaxPayment()const {
	return option(QStringLiteral("rp_max_payment")).toDouble();
}
bool ConfigFile::randPaySubmit()const {
	return option("rp_submit").toBool();
}
QString ConfigFile::rpcuser()const {
	return option(QStringLiteral("rpcuser")).toString();
}
QString ConfigFile::rpcpassword()const {
	return option(QStringLiteral("rpcpassword")).toString();
}
QString ConfigFile::debug()const {
	return option(QStringLiteral("debug")).toString();
}
void ConfigFile::setServer(bool b) {
	setOption(QStringLiteral("server"), b ? 1 : 0);
}
void ConfigFile::setListen(bool b) {
	setOption(QStringLiteral("listen"), b ? 1 : 0);
}
void ConfigFile::setRpcuser(const QString & s) {
	setOption(QStringLiteral("rpcuser"), s);
}
void ConfigFile::setRpcpassword(const QString & s) {
	setOption(QStringLiteral("rpcpassword"), s);
}
void ConfigFile::setDebug(const QString & s) {
	setOption(QStringLiteral("debug"), s);
}
