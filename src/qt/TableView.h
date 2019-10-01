//TableView.h
#pragma once
#include <QTableView>

class TableView: public QTableView {
	public:
		TableView(QWidget * parent = 0);
		void copyText();
#ifdef _DEBUG
		void showClassInfo();
#endif
};
