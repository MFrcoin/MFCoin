//ManageDnsPage.h by mfcoin developers
#pragma once
#include <QDialog>
#include "NameValueLineEdits.h"
class QLineEdit;
class QFormLayout;
class QString;

class ManageDnsPage: public QDialog {
    public:
	    ManageDnsPage(QWidget*parent=0);
		QString name()const;
		QString value()const;
    protected:
		NameValueLineEdits* _NVPair = 0;
		QLineEdit* _editName = 0;
		QList<QLineEdit*> _edits;

		void recalcValue();
		QLineEdit* addLineEdit(QFormLayout*form, const QString& dnsParam, const QString& text, const QString& tooltip);
};
