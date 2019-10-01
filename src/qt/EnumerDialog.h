//EnumerDialog.h by MFCoin developers
#pragma once
#include "NameValueLineEdits.h"
#include <QSpinBox>
class PhoneNumberLineEdit;

class EnumerDialog: public QWidget {
	public:
		EnumerDialog();
	protected:
		PhoneNumberLineEdit* _phone = 0;
		QSpinBox* _antiSquatter = new QSpinBox;
		
		NameValueLineEdits* _NVEdit = 0;
		void generateNVPair();
};
