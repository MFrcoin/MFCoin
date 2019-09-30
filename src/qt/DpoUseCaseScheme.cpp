//DpoUseCaseScheme.cpp by MFCoin developers
#include "DpoUseCaseScheme.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPixmap>

DpoUseCaseScheme::DpoUseCaseScheme() {
	setWindowTitle(tr("Scheme"));

	auto lay = new QVBoxLayout(this);
	//auto text = new QLabel(tr(
	//R"HTML(<b>Assignment of responsibilities for DPO</b><br>
	//The system allows to verify users, who can add  authenicated documents that can be trusted)HTML"));
	//lay->addWidget(text);

	auto image = new QLabel;
	image->setPixmap(QPixmap(":/icons/DPO_use_case_2.png"));
	lay->addWidget(image);
}
