//InfoCardItemDelegate.cpp by MFCoin developers
#include "InfoCardItemDelegate.h"
#include "InfoCardTableModel.h"
#include "InfoCardTableView.h"
#include "InfoCardDialog.h"

InfoCardItemDelegate::InfoCardItemDelegate(InfoCardTableView*parent, InfoCardTableModel*model): QStyledItemDelegate(parent), _parent(parent) {
	_model = model;
}
QWidget * InfoCardItemDelegate::createEditor(QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index)const {
	Q_UNUSED(option);
	auto item = _model->itemBy(index.row());
	if(!item)
		return 0;
	InfoCardDialog dlg(*item, _parent->logger(), parent);
	if(dlg.exec()!=QDialog::Accepted)
		return 0;
	_model->setData(index, dlg.text());
	return 0;
}
void InfoCardItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index)const {
	return QStyledItemDelegate::setEditorData(editor, index);
}
