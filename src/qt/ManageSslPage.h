//ManageSslPage.h by MFCoin developers
#pragma once
#include <QDialog>
#include "NameValueLineEdits.h"
class CertTableView;
class CertLogger;
class QAbstractButton;

class ManageSslPage: public QDialog {
	public:
		ManageSslPage(QWidget*parent=0);
		QString name()const;
		QString value()const;
	protected:
		void onDelete();
		void onCreate();
		void enableButtons();
		static QString randName();

		QAbstractButton* _btnDelete = 0;
		QAbstractButton* _btnGenerate = 0;
		QAbstractButton* _btnOpenFolder = 0;
		CertTableView* _view = 0;
		struct TemplateDialog;
		CertLogger* _logger = 0;
};
