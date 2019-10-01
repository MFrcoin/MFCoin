//EmailLineEdit.h by MFCoin developers
#pragma once
#include "EmailValidator.h"
#include <QLineEdit>

class EmailLineEdit: public QLineEdit {
	public:
		EmailLineEdit();
		EmailValidator* validator()const { return _validator; }
	protected:
		EmailValidator* _validator = 0;
};
