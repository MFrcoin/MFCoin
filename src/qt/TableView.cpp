//TableView.cpp
#include "TableView.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>

TableView::TableView(QWidget * parent): QTableView(parent) {
	{
		auto a = new QAction(tr("Copy text"));
		a->setShortcut(QKeySequence::Copy);
		a->setShortcutContext(Qt::WidgetShortcut);
		connect(a, &QAction::triggered, this, &TableView::copyText);
		addAction(a);
	}
#ifdef _DEBUG
	{
		auto a = new QAction(tr("Debug: class info"));
		connect(a, &QAction::triggered, this, &TableView::showClassInfo);
		addAction(a);
	}
#endif
	setContextMenuPolicy(Qt::ActionsContextMenu);
}
void TableView::copyText() {
	auto index = currentIndex();
	if(!index.isValid())
		return;
	QApplication::clipboard()->setText(index.data().toString());
}
#ifdef _DEBUG
void TableView::showClassInfo() {
	QString strLine = "_______________\n";
	QString str = tr("Information about table classes:\n");
	str += "Class hierarchy (to the parent direction):\n";
	QString str1;
	for(const QMetaObject *meta = metaObject(); 0!=meta; meta = meta->superClass()) {
		if(!str1.isEmpty())
			str1 += ": derived from\n";
		str1 += QString("class %1").arg(meta->className());
	}
	str += str1 + '\n';
	str += strLine;
	str += QString("Model: class %1\n").arg(model()->metaObject()->className()
		);
	str += strLine;
	str += "Information about parent windows:\n0) This window\n";
	int i=1;
	for(QWidget*p = parentWidget(); p!=0; p = p->parentWidget(), ++i) {
		str += QString("%1) %2\n").arg(i).arg(p->metaObject()->className());
	}
	str += strLine;
	auto delegate = itemDelegate();
	str += "Item delegate:\n";
	if(delegate) {
		for(const QMetaObject *meta = delegate->metaObject(); 0!=meta; meta = meta->superClass()) {
			str += QString("class %1\n").arg(meta->className());
		}
	} else {
		str += "empty";
	}
	QMessageBox::information(this, "Class info", str);
}
#endif
