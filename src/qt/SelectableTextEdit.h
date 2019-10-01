//SelectableTextEdit.h by MFCoin developers
#pragma once
#include <QPlainTextEdit>
#include <QWidget>
#include <QPointer>
class QLineEdit;

class SelectableTextEdit: public QPlainTextEdit {
	public:
		SelectableTextEdit();

		QPointer<QWidget> _buddyToShowToolip;//use this window if not set
		virtual void mousePressEvent(QMouseEvent *e)override;
		void copyAndShowTooltip(QLineEdit*toDeselect = 0);
		virtual QSize sizeHint()const override;
};
