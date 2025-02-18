#include "QIntegerFieldWidget.h"

#include <QGridLayout>

namespace dyno
{
	QIntegerFieldWidget::QIntegerFieldWidget(FBase* field)
		: QFieldWidget(field)
	{
		FVar<int>* f = TypeInfo::cast<FVar<int>>(field);
		if (f == nullptr) {
			return;
		}

		//this->setStyleSheet("border:none");
		QGridLayout* layout = new QGridLayout;
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		this->setLayout(layout);

		QLabel* name = new QLabel();
		name->setFixedSize(100, 18);
		name->setText(FormatFieldWidgetName(field->getObjectName()));

		QSpinBox* spinner = new QSpinBox;
		spinner->setValue(f->getData());

		layout->addWidget(name, 0, 0);
		layout->addWidget(spinner, 0, 1, Qt::AlignRight);


		this->connect(spinner, SIGNAL(valueChanged(int)), this, SLOT(changeValue(int)));
	}

	void QIntegerFieldWidget::changeValue(int value)
	{
		FVar<int>* f = TypeInfo::cast<FVar<int>>(field());
		if (f == nullptr)
			return;

		f->setValue(value);
		f->update();

		emit fieldChanged();
	}

	QUIntegerFieldWidget::QUIntegerFieldWidget(FBase* field)
		: QFieldWidget(field)
	{
		FVar<uint>* f = TypeInfo::cast<FVar<uint>>(field);
		if (f == nullptr)
		{
			return;
		}
		//this->setStyleSheet("border:none");
		QGridLayout* layout = new QGridLayout;
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		this->setLayout(layout);

		QLabel* name = new QLabel();
		name->setFixedSize(100, 18);
		name->setText(FormatFieldWidgetName(field->getObjectName()));

		QSpinBox* spinner = new QSpinBox;
		spinner->setMaximum(1000000);
		spinner->setValue(f->getData());

		layout->addWidget(name, 0, 0);
		layout->addWidget(spinner, 0, 1, Qt::AlignRight);

		this->connect(spinner, SIGNAL(valueChanged(int)), this, SLOT(changeValue(int)));
	}

	void QUIntegerFieldWidget::changeValue(int value)
	{
		FVar<uint>* f = TypeInfo::cast<FVar<uint>>(field());
		if (f == nullptr)
			return;

		f->setValue(value);
		f->update();

		emit fieldChanged();
	}
}

