//DpoCreateRootWidget.h by MFCoin developers
#pragma once
#include "NameValueLineEdits.h"
#include <QScrollArea>
class QPlainTextEdit;
class QFormLayout;
class QLineEdit;

class DpoCreateRootWidget: public QWidget {
	public:
		DpoCreateRootWidget();
		void updateSettings(bool save);
		NameValueLineEdits* _NVPair = 0;
    protected:
		QLineEdit* _editName = 0;
		QPlainTextEdit* _editOther = 0;
		QList<QLineEdit*> _edits;
		void recalcValue();
		QLineEdit* addLineEdit(QFormLayout*form, const QString& name, const QString& text, const QString& tooltip, bool readOnly = false);
};
