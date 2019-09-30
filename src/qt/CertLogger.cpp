//CertLogger.cpp by MFCoin developers
#include "CertLogger.h"
#include <QDir>
#include <QTimer>
#include <QShortcut>
#include <QCoreApplication>
#include <QTextStream>

CertLogger::CertLogger() {
	auto timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this]() {
		if(_file.isOpen())
			_file.flush();
	});
	timer->start(1000);
	
	auto copy = new QShortcut(QKeySequence("Ctrl+C"), this);
	copy->setContext(Qt::WidgetWithChildrenShortcut);
	connect(copy, &QShortcut::activated, this, &CertLogger::copy);
}
void CertLogger::append(const QString & s) {
	QTextBrowser::append(s);
	if(!_file.isOpen() && !_file.fileName().isEmpty()) {
		if(!_file.open(QFile::WriteOnly|QFile::Truncate))
			QTextBrowser::append(_file.errorString());
	}
	if(_file.isOpen()) {
		QString s2 = s;
		if(!s2.endsWith('\n'))
			s2 += '\n';
#ifdef Q_OS_WIN
		s2.replace("\n", "\r\n");
#endif
		_file.write(s2.toUtf8());
		_file.flush();
	}
	QCoreApplication::processEvents();
}
void CertLogger::clear(bool removeFile) {
	QTextBrowser::clear();
	if(_file.isOpen()) {
		_file.close();
	}
	if(removeFile) {
		_file.remove();
	}
}
void CertLogger::setFileNear(const QDir & dir, const QString & fileName) {
	QString p = dir.absoluteFilePath(QFileInfo(fileName).baseName() + ".log");
	setFile(p);
}
void CertLogger::setFile(const QString & path) {
	if(path==_file.fileName())
		return;
	clear();
	_file.setFileName(path);
	if(path.isEmpty())
		return;
	if(!_file.open(QFile::ReadWrite)) {
		QTextBrowser::append(_file.errorString());
	}
	QTextStream stream(&_file);
	QString s = stream.readAll();
	QTextBrowser::append(s);
	_file.close();
}
