//InfoCardTableView.cpp by MFCoin developers
#include "InfoCardTableView.h"
#include "InfoCardTableModel.h"
#include "ShellImitation.h"
#include "Settings.h"
#include "CertTableView.h"
#include "InfoCardItemDelegate.h"
#include "CertLogger.h"
#include <QHeaderView>

InfoCardTableView::InfoCardTableView(CertLogger*logger): _logger(logger) {
	horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	horizontalHeader()->hide();
	_model = new InfoCardTableModel(this);
	setModel(_model);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);
	connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, &InfoCardTableView::reloadLog);
	setItemDelegate(new InfoCardItemDelegate(this, _model));
}
InfoCardTableModel* InfoCardTableView::model()const {
	return _model;
}
CertLogger*InfoCardTableView::logger()const {
	return _logger;
}
void InfoCardTableView::reloadLog() {
	if(_logger) {
		QString path = selectedLogPath();
		_logger->setFile(path);
	}
}
void InfoCardTableView::showInExplorer() {
	int nRow = selectedRow();
	QString path;
	if(const auto & row = _model->itemBy(nRow)) {
		path = row->_fileName;
	} else {
		path = Settings::certDir().absolutePath();
	}
	CertTableView::showInGraphicalShell(this, path);
}
int InfoCardTableView::selectedRow()const {
	auto rows = selectionModel()->selectedIndexes();
	if(!rows.isEmpty())
		return rows[0].row();
	return -1;
}
QString InfoCardTableView::selectedLogPath() {
	int nRow = selectedRow();
	if(const auto & row = _model->itemBy(nRow))
		return row->logFilePath();
	return {};
}
void InfoCardTableView::dataChanged(int row) {
	auto index = _model->index(row, 0);
	auto index2 = _model->index(row, _model->columnCount()-1);
	_model->dataChanged(index, index2);
}
