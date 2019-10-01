//BlockingHttp.cpp BSD license
#include "BlockingHttp.h"
#include <QScopedValueRollback>

BlockingHttp::BlockingHttp():
	_lock(QMutex::Recursive)
{
	connect(&_emitGet, &SignalEmitter::vStr, this, &BlockingHttp::doBlockingGet);
	connect(&_emitHead, &SignalEmitter::vStr, this, &BlockingHttp::doBlockingHead);
	connect(&_emitPost, &SignalEmitter::vStrA, this, &BlockingHttp::doBlockingPost);

	moveToThread(this);
	start();
	while(!_initialized && !isFinished()) {
		msleep(1);
	}
}
BlockingHttp::~BlockingHttp() {
	while(1) {
		{
			QMutexLocker lock(&_lock);
			if(!_loop)
				break;
			_loop->exit(0);
		}
		msleep(1);
	}
	while(!isFinished())
		msleep(1);
}
void BlockingHttp::run() {
	QEventLoop loop;
	{
		QMutexLocker lock(&_lock);
		_loop = &loop;
	}

	QNetworkAccessManager manager;
	_manager = &manager;
	connect(&manager, &QNetworkAccessManager::finished, this, &BlockingHttp::replyFinished);
	_initialized = true;

	_loop->exec();
	{
		QMutexLocker lock(&_lock);
		_loop = 0;
	}
	_manager = 0;
}
void BlockingHttp::addPersistentHeader(const QString &strHeaderName, const QString & strValue) {
	//need encode special chars here...
	addPersistentRawHeader(strHeaderName.toLocal8Bit(), strValue.toLocal8Bit());
}
void BlockingHttp::addPersistentRawHeader(const QByteArray &headerName, const QByteArray &value) {
	QMutexLocker lock(&_lock);
	_persistentHeaders.push_back(RawHeaderPair(headerName, value));
}
void BlockingHttp::connectSlots(QNetworkReply*reply) {
	connect(reply, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),
		this, &BlockingHttp::networkError);
#ifndef QT_NO_OPENSSL
	connect(reply, &QNetworkReply::sslErrors, this, &BlockingHttp::sslErrors);
#endif
}
void BlockingHttp::doBlockingGet(QString strUrl) {
	QNetworkRequest request;
	request.setUrl(QUrl(strUrl));
	addHeaders(&request);

	_reply = _manager->get(request);
	connectSlots(_reply);
}
void BlockingHttp::doBlockingHead(QString strUrl) {
	QNetworkRequest request;
	request.setUrl(QUrl(strUrl));
	addHeaders(&request);

	_reply = _manager->head(request);
	connectSlots(_reply);
}
void BlockingHttp::doBlockingPost(QString strUrl, QByteArray data) {
	QNetworkRequest request;
	request.setUrl(QUrl(strUrl));
	addHeaders(&request);

	_reply = _manager->post(request, data);
	connectSlots(_reply);
}
void BlockingHttp::prepareSend() {
	QMutexLocker lock(&_lock);
	_response.clear();
	_replyFinished = false;
	_lockServerReply.lock();
}
BlockingHttp::Response BlockingHttp::blockingGet(const QString & strUrl) {
	prepareSend();
	_emitGet.emitv(strUrl);

	waitForServerReply();
	if(_urlRedirect.isValid() && _maxRedirectsAllowed>0) {
		QScopedValueRollback<int> maxRedirectsAllowed(_maxRedirectsAllowed);
		--_maxRedirectsAllowed;

		QString strUrl2 = _urlRedirect.toString();
		_urlRedirect.clear();
		return blockingGet(strUrl2);
	}
	{
		QMutexLocker lock(&_lock);
		auto ret = _response;
		return ret;
	}
}
BlockingHttp::Response BlockingHttp::blockingPost(const QString & strUrl, QByteArray data) {
	prepareSend();

	_emitPost.emitv(strUrl, data);

	waitForServerReply();
	if(_urlRedirect.isValid() && _maxRedirectsAllowed>0) {
		QScopedValueRollback<int> maxRedirectsAllowed(_maxRedirectsAllowed);
		--_maxRedirectsAllowed;
		QString strUrl2 = _urlRedirect.toString();
		_urlRedirect.clear();
		return blockingPost(strUrl2, data);
	}
	{
		QMutexLocker lock(&_lock);
		auto ret = _response;
		return ret;
	}
}
void BlockingHttp::addHeaders(QNetworkRequest*request) {
	addPersistentHeaders(request);
	addTempHeadersAndClear(request);
}
void BlockingHttp::addTempHeadersAndClear(QNetworkRequest*request) {
	QMutexLocker lock(&_lock);
	for(RawHeaderPair pr: _tempHeaders)
		request->setRawHeader(pr.first, pr.second);
	_tempHeaders.clear();
}
void BlockingHttp::addPersistentHeaders(QNetworkRequest*request) {
	QMutexLocker lock(&_lock);
	for(RawHeaderPair pr: _persistentHeaders)
		request->setRawHeader(pr.first, pr.second);
}
void BlockingHttp::Response::clear() {
	_httpCode = -1;
	_data.clear();
	_headers.clear();
}
void BlockingHttp::Response::setFrom(QNetworkReply* reply) {
	_data = reply->readAll();
	_httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	_headers = reply->rawHeaderPairs();
}
void BlockingHttp::replyFinished(QNetworkReply*reply) {
	QNetworkReply::NetworkError error = reply->error();
	Q_UNUSED(error);//for debugging
	if(reply!=_reply) {
		Q_ASSERT(0);
		reply->close();
		reply->deleteLater();
		return;
	}
	{
		//wait untill wait function will release mutex
		QMutexLocker lock(&_lockServerReply);
	}
	{
		QMutexLocker lock(&_lock);
		_urlRedirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
		if(_urlRedirect.isValid()) {
			_urlRedirect = reply->url().resolved(_urlRedirect);
		}
		_response.setFrom(reply);
		_replyFinished = true;
		reply->close();
		reply->deleteLater();
	}
	_waitServerReply.wakeAll();
}

void BlockingHttp::onErrorCommon() {
	QObject* s = sender();
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(s);
	if(reply==_reply) {
		{
			//wait untill wait function will release mutex
			QMutexLocker lock(&_lockServerReply);
		}
		_replyFinished = true;
		_waitServerReply.wakeAll();
	}
}
#ifndef QT_NO_OPENSSL
void BlockingHttp::sslErrors(const QList<QSslError> & errors) {
	if(_ignoreSslErrors)
		_reply->ignoreSslErrors();
	bool bErrors = false;
	for(auto& err: errors) {
		if(err.error()==QSslError::NoError)
			continue;
		bErrors = true;
		QString str = err.errorString();
		qWarning() << QStringLiteral("BlockingHttp::sslErrors (%1): %2\n")
			.arg(err.error()).arg(str);
	}
	if(!bErrors)
		return;
	if(!_ignoreSslErrors)
		onErrorCommon();
}
#else
void BlockingHttp::sslErrors(const QList<QSslError> & errors) {
	Q_ASSERT(0);
}
#endif
void BlockingHttp::networkError(QNetworkReply::NetworkError nErr) {
	QObject* s = sender();
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(s);
	if(reply) {
		QString str = reply->errorString();
		qWarning() << QStringLiteral("BlockingHttp::networkError (%1): %2\n")
			.arg(nErr).arg(str);
		_response._data = str.toUtf8();
	}
	onErrorCommon();
}
void BlockingHttp::waitForServerReply() {
	if(_waitServerReply.wait(&_lockServerReply, 60*1000)) {
		Q_ASSERT(_replyFinished);
	}
	//_reply = 0;
	_lockServerReply.unlock();
}
//_____________
SignalEmitter::SignalEmitter(QObject *parent): QObject(parent) {
}
void SignalEmitter::emitv() {
	Q_EMIT v();
}
void SignalEmitter::emitv(QString str) {
	Q_EMIT vStr(str);
}
void SignalEmitter::emitv(QString str, QByteArray data) {
	Q_EMIT vStrA(str, data);
}
void SignalEmitter::emitv(QString str, QString data) {
	Q_EMIT vStr2(str, data);
}
void SignalEmitter::emitv(int n) {
	Q_EMIT vInt(n);
}
//__________________________
std::string FormatFullVersion();
// https request using Qt.
// Returns HTTP status code if OK, or -1 if error
// Ret contains server answer (if OK), orr error text (-1)
int blockingHttps(const std::string & host, const std::string &path, const char *post,
			const std::map<std::string,std::string> &header, std::string & ret)
{
	if(host.empty())
		return -1;
	QString url = QString::fromStdString(host) + QString::fromStdString(path);
	if(!url.startsWith("http", Qt::CaseInsensitive)) {
		url.prepend("https://");
	}
	BlockingHttp http;
	http.addPersistentHeader(QStringLiteral("Host"), QString::fromStdString(host));
	http.addPersistentHeader(QStringLiteral("User-Agent"),
			QStringLiteral("mfcoin-json-rpc/%1").arg(FormatFullVersion().c_str()));
	http.addPersistentHeader(QStringLiteral("Accept"), "application/json");
	http.addPersistentHeader(QStringLiteral("Connection"), "close");

	if(post) {
	  http.addPersistentHeader(QStringLiteral("Content-Type"), "application/json");
	  http.addPersistentHeader(QStringLiteral("Content-Length"), QString::number(strlen(post)));
	}

	// Additional header fields from client
	for(auto it = header.begin(); it != header.end(); ++it)
		http.addPersistentHeader(QString::fromStdString(it->first), QString::fromStdString(it->second));

	BlockingHttp::Response resp;
	if(post) {
		resp = http.blockingPost(url, QByteArray(post));
	} else {
		resp = http.blockingGet(url);
	}
	ret = resp._data.data();
	return resp._httpCode;
}

