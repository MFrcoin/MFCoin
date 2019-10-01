//InfoCardItemDelegate.h by MFCoin developers
#pragma once
#include <QStyledItemDelegate>
class InfoCardTableView;
class InfoCardTableModel;

class InfoCardItemDelegate: public QStyledItemDelegate {
	public:
		InfoCardItemDelegate(InfoCardTableView*parent, InfoCardTableModel*model);
		QWidget * createEditor(QWidget * parent, const QStyleOptionViewItem & option, const QModelIndex & index)const override;
		virtual void setEditorData(QWidget *editor, const QModelIndex &index)const override;
		//virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index)const override;
	protected:
		InfoCardTableView*_parent = 0;
		InfoCardTableModel*_model = 0;
};
