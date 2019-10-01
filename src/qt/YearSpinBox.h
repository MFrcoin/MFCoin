#pragma once
#include <QSpinBox>

//displays empty string when set to 0
class YearSpinBox: public QSpinBox {
	public:
		YearSpinBox() {
			setMinimum(-10000);
			setMaximum(3000);
			setValue(0);//could be QDate::currentDate().year(), but make it empty
		}
		virtual QString textFromValue(int value)const override {
			if(value==0)
				return {};
			return QSpinBox::textFromValue(value);
		}
};
