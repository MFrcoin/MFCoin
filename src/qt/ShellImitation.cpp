//ShellImitation.cpp by MFCoin developers
#include "ShellImitation.h"
#include "CertLogger.h"
#include <QDir>
#include <QThread>
#include <QApplication>
#include <QDirIterator>

ShellImitation::ShellImitation(CertLogger* logger): _logger(logger) {
}
void ShellImitation::maybeLog(const QString & s) {
	if(_logger && QThread::currentThread()==qApp->thread()) {
		_logger->append(s);
	}
}
QString ShellImitation::tr(const char*c) {
	return QObject::tr(c);
}
bool ShellImitation::touch(const QDir & dir, const QString & fileName, QString & err) {
	QString path = dir.absoluteFilePath(fileName);
	maybeLog(tr("Touching %1").arg(QDir::toNativeSeparators(path)));
	QFile file(path);
	if(!file.open(QIODevice::WriteOnly)) {
		err = tr("Can't open file %1: %2").arg(QDir::toNativeSeparators(path)).arg(file.errorString());
		maybeLog(err);
		return false;
	}
	file.close();
	err.clear();
	return true;
}
bool ShellImitation::mkpath(const QDir & dir, const QString & path, QString & error, int tries) {
	maybeLog(tr("mkpath %1").arg(QDir::toNativeSeparators(dir.absoluteFilePath(path))));
	for(int i = 0; i < tries; ++i) {
		if(i>0)
			QThread::msleep(50);
		if(dir.mkpath(path)) {
			if(dir.exists(path)) {
				error.clear();
				return true;
			}
		}
		maybeLog(tr("Trying again..."));
	}
	error = tr("Can't create directory %1").arg(QDir::toNativeSeparators(dir.absoluteFilePath(path)));
	maybeLog(error);
	return false;
}
bool ShellImitation::write(const QString & path, const QByteArray & what, QString &err) {
	maybeLog(tr("Writing %1").arg(path));
	QDir dir = QFileInfo(path).dir();
	if(!dir.exists()) {
		maybeLog(tr("Creating directory %1").arg(dir.absolutePath()));
		if(!dir.mkpath(".")) {
			err = tr("Can't create %1").arg(dir.absolutePath());
			maybeLog(err);
			return false;
		}
	}
	QFile file(path);
	if(!file.open(QFile::WriteOnly | QFile::Truncate)) {
		err = tr("Can't open file %1: %2").arg(path).arg(file.errorString());
		maybeLog(err);
		return false;
	}
	auto written = file.write(what);
	if(written == what.size()) {
		err.clear();
		return true;
	}
	err = tr("Can't write %1: only %2 bytes written instead of %3, reason: %4")
		.arg(path)
		.arg(written)
		.arg(what.size())
		.arg(file.errorString());
	maybeLog(err);
	return false;
}
bool ShellImitation::remove(const QString &file) {
	maybeLog(tr("Removing file %1...").arg(file));
	if(QFile::remove(file))
		return true;
	maybeLog(tr("Can't remove file %1").arg(file));
	return false;
}
bool ShellImitation::removeRecursiveFilesOnly(QDir & dir, QString &err) {
	maybeLog(tr("Removing files recursive from %1").arg(dir.absolutePath()));
	err.clear();
	QDirIterator it(dir, QDirIterator::Subdirectories);
	while(it.hasNext()) {
		const QString path = it.next();
		QFileInfo info(path);
		if(info.isDir())
			continue;
		maybeLog(tr("... removing %1").arg(path));
		if(!dir.remove(path)) {
			err = tr("Can't delete %1").arg(path);
			maybeLog(err);
			return false;
		}
	}
	return true;
}
