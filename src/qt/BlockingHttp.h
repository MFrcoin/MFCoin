//BlockingHttp.h BSD license
#pragma once
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QWaitCondition>
#include <QEventLoop>
#include <QThread>
#include <QMutex>
#include <QEventLoop>
#include <QSslError>
#include <QPointer>
#ifdef QT_NO_OPENSSL
class QSslError{ int v; };
#endif

class SignalEmitter: public QObject {
	Q_OBJECT
	public:
		SignalEmitter(QObject *parent=0);
		virtual ~SignalEmitter() {}
	public:
		//v means variant - by parameters
		void emitv();
		void emitv(QString str);
		void emitv(QString str, QByteArray data);
		void emitv(QString str, QString data);
		void emitv(int n);
	Q_SIGNALS:
		void v();
		void vStr(QString str);
		void vStr2(QString str, QString data);
		void vStrA(QString str, QByteArray data);
		void vInt(int n);
};

class BlockingHttp: public QThread {
	Q_OBJECT
	public:
		BlockingHttp();
		~BlockingHttp();
		using RawHeaderPair = QNetworkReply::RawHeaderPair;

		void addPersistentRawHeader(const QByteArray &headerName, const QByteArray &value);
		void addPersistentHeader(const QString &strHeaderName, const QString &strValue);
		//http code, contants
		struct Response {
			int _httpCode = -1;
			QByteArray _data;
			QList<RawHeaderPair> _headers;
			
			void clear();
			void setFrom(QNetworkReply* reply);
		};
		Response blockingPost(const QString & strUrl, QByteArray data);
		Response blockingGet(const QString & strUrl);
		Response blockingHead(const QString & strUrl);
		int _maxRedirectsAllowed = 2;
		bool _ignoreSslErrors = false;
	public Q_SLOTS:
		void replyFinished(QNetworkReply* reply);
		void networkError(QNetworkReply::NetworkError error);
		void doBlockingPost(QString strUrl, QByteArray data);
		void doBlockingGet(QString strUrl);
		void doBlockingHead(QString strUrl);
		void sslErrors(const QList<QSslError> & errors);
	protected:
		virtual void run()override;
		void addHeaders(QNetworkRequest*request);
		void addPersistentHeaders(QNetworkRequest*request);
		void addTempHeadersAndClear(QNetworkRequest*request);
		void waitForServerReply();
		void prepareSend();
		void connectSlots(QNetworkReply*reply);
		void onErrorCommon();

		QMutex _lock;
		QMutex _lockServerReply;
		QWaitCondition _waitServerReply;
		QList<RawHeaderPair> _persistentHeaders;
		QList<RawHeaderPair> _tempHeaders;
		Response _response;
		QPointer<QNetworkAccessManager> _manager;
		QPointer<QNetworkReply> _reply;
		QPointer<QEventLoop> _loop;
		SignalEmitter _emitGet;
		SignalEmitter _emitHead;
		SignalEmitter _emitPost;
		volatile bool _initialized = false;
		volatile bool _replyFinished = false;
		QUrl _urlRedirect;
};
