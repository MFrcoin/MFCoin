//DpoRegisterDocWidget.h by MFCoin developers
#pragma once
#include "NameValueLineEdits.h"
class QComboBox;
class QBoxLayout;

class DpoRegisterDocWidget: public QWidget {
	public:
		DpoRegisterDocWidget();
		NameValueLineEdits* _NVPair = 0;
	protected:
		void openFileDialog();
		void recalcValue();
		QLabel* addText(QBoxLayout*lay, const QString& s);
		void showError(const QString & s);

		QComboBox* _chooseRoot = 0;
		QLineEdit* _editFile = 0;
		QLineEdit* _editHash = 0;
		SelectableLineEdit* _editSignature = 0;
		QLineEdit* _editDocName = 0;
};
