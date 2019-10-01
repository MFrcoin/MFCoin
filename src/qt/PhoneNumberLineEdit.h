//PhoneNumberLineEdit.h by MFCoin developers
#include <QLineEdit>

struct PhoneNumberLineEdit: public QLineEdit {
	PhoneNumberLineEdit();
	QString toPhoneNumber()const;
};
