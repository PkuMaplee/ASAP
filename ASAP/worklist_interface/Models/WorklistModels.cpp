#include "WorklistModels.h"

#include <cctype>
#include <QtConcurrent\qtconcurrentrun.h>

#include "../GUI/IconCreator.h"
#include "../GUI/WorklistWindow.h"

namespace ASAP
{
	WorklistModels::WorklistModels(void) : images(new QStandardItemModel(0, 0)), patients(new QStandardItemModel(0, 0)), studies(new QStandardItemModel(0, 0)), worklists(new QStandardItemModel(0, 0))
	{
	}

	void WorklistModels::SetWorklistItems(const DataTable& items)
	{
		if (worklists->rowCount() == 0)
		{
			QStandardItem* model_item(new QStandardItem("All"));
			worklists->setItem(worklists->rowCount(), 0, model_item);
		}

		worklists->removeRows(1, worklists->rowCount() - 1);
		for (size_t item = 0; item < items.Size(); ++item)
		{
			std::vector<const std::string*> record(items.At(item, std::vector<std::string>{ "id", "title" }));
			QStandardItem* model_item(new QStandardItem(QString(record[1]->data())));

			model_item->setData(QVariant(record[0]->data()));
			worklists->setItem(worklists->rowCount(), 0, model_item);
		}
	}

	void WorklistModels::SetPatientsItems(const DataTable& items)
	{
		patients->removeRows(0, patients->rowCount());
		patients->setRowCount(items.Size());
		for (size_t item = 0; item < items.Size(); ++item)
		{
			std::vector<const std::string*> record(items.At(item, DataTable::FIELD_SELECTION::VISIBLE));
			for (size_t field = 0; field < record.size(); ++field)
			{
				QStandardItem* model_item = new QStandardItem(QString(record[field]->data()));
				model_item->setData(QVariant(items.At(item, { "id" })[0]->data()));
				patients->setItem(item, field, model_item);
			}
		}
	}

	void WorklistModels::SetStudyItems(const DataTable& items)
	{
		studies->removeRows(0, studies->rowCount());
		studies->setRowCount(items.Size());
		for (size_t item = 0; item < items.Size(); ++item)
		{
			std::vector<const std::string*> record(items.At(item, DataTable::FIELD_SELECTION::VISIBLE));
			for (size_t field = 0; field < record.size(); ++field)
			{
				QStandardItem* model_item = new QStandardItem(QString(record[field]->data()));
				model_item->setData(QVariant(items.At(item, { "id" })[0]->data()));
				studies->setItem(item, field, model_item);
			}
		}
	}

	void WorklistModels::SetImageItems(const DataTable& items, WorklistWindow* window, bool& continue_loading)
	{
		continue_loading = true;

		images->removeRows(0, images->rowCount());
		QtConcurrent::run([items, images=images, window=window, continue_loading=&continue_loading, size=200]()
		{
			IconCreator creator;

			QObject::connect(&creator,
					&IconCreator::RequiresItemRefresh,
					window,
					&WorklistWindow::UpdateImageIcons);

			QObject::connect(&creator,
					&IconCreator::RequiresStatusBarChange,
					window,
					&WorklistWindow::UpdateStatusBar);

			creator.InsertIcons(items, images, size, *continue_loading);
		});
	}

	void WorklistModels::UpdateHeaders(std::vector<std::pair<std::set<std::string>, QAbstractItemView*>>& header_view_couple)
	{
		std::vector<QStandardItemModel*> models({ worklists, patients, studies, images });
		for (size_t m = 0; m < models.size(); ++m)
		{
			if (header_view_couple[m].second)
			{
				SetHeaders_(header_view_couple[m].first, models[m], header_view_couple[m].second);
			}
		}
	}

	void WorklistModels::SetHeaders_(const std::set<std::string> headers, QStandardItemModel* model, QAbstractItemView* view)
	{
		QStringList q_headers;
		for (const std::string& column : headers)
		{
			std::string capital_column = column;
			capital_column[0] = std::toupper(capital_column[0]);
			q_headers.push_back(QString(capital_column.c_str()));
		}
		model->setColumnCount(q_headers.size());
		model->setHorizontalHeaderLabels(q_headers);
		view->update();
	}
}