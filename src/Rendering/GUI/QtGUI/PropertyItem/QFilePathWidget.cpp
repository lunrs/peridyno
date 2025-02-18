#include "QFilePathWidget.h"

#include "FilePath.h"

#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>

namespace dyno
{
	QStringFieldWidget::QStringFieldWidget(FBase* field)
		: QFieldWidget(field)
	{
		FVar<std::string>* f = TypeInfo::cast<FVar<std::string>>(field);

		//this->setStyleSheet("border:none");
		QHBoxLayout* layout = new QHBoxLayout;
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		this->setLayout(layout);

		QLabel* name = new QLabel();
		name->setFixedSize(150, 18);
		name->setText(FormatFieldWidgetName(field->getObjectName()));

		fieldname = new QLineEdit;
		fieldname->setText(QString::fromStdString(f->getValue()));

		layout->addWidget(name, 0);
		layout->addWidget(fieldname, 1);
		layout->setSpacing(5);

		connect(fieldname, &QLineEdit::textChanged, this, &QStringFieldWidget::updateField);
	}

	void QStringFieldWidget::updateField(QString str)
	{
		auto f = TypeInfo::cast<FVar<std::string>>(field());
		if (f == nullptr)
		{
			return;
		}
		f->setValue(str.toStdString());
		f->update();
	}

	QFilePathWidget::QFilePathWidget(FBase* field)
		: QFieldWidget(field)
	{
		FVar<FilePath>* f = TypeInfo::cast<FVar<FilePath>>(field);

		//this->setStyleSheet("border:none");
		QHBoxLayout* layout = new QHBoxLayout;
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		this->setLayout(layout);

		QLabel* name = new QLabel();
		name->setFixedSize(100, 18);
		name->setText(FormatFieldWidgetName(field->getObjectName()));

		location = new QLineEdit;
		location->setText(QString::fromStdString(f->getValue().string()));

		QPushButton* open = new QPushButton("open");
// 		open->setStyleSheet("QPushButton{color: black;   border-radius: 10px;  border: 1px groove black;background-color:white; }"
// 							"QPushButton:hover{background-color:white; color: black;}"  
// 							"QPushButton:pressed{background-color:rgb(85, 170, 255); border-style: inset; }" );
		open->setFixedSize(60, 24);

		layout->addWidget(name, 0);
		layout->addWidget(location, 1);
		layout->addWidget(open, 0, 0);
		layout->setSpacing(5);

		connect(location, &QLineEdit::textChanged, this, &QFilePathWidget::updateField);

		connect(open, &QPushButton::clicked, this, [=]() {
			QString path = QFileDialog::getOpenFileName(this, tr("Open File"), QString::fromStdString(getAssetPath()), tr("Text Files(*.*)"));
			if (!path.isEmpty()) {
				QFile file(path);
				if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
					QMessageBox::warning(this, tr("Read File"),
						tr("Cannot open file:\n%1").arg(path));
					return;
				}
				location->setText(path);
				file.close();
			}
			else {
				QMessageBox::warning(this, tr("Path"), tr("You do not select any file."));
			}
		});
	}

	void QFilePathWidget::updateField(QString str)
	{
		auto f = TypeInfo::cast<FVar<FilePath>>(field());
		if (f == nullptr)
		{
			return;
		}
		f->setValue(str.toStdString());
		f->update();

		emit fieldChanged();
	}
}

