/******************************************************************************
    Copyright (C) 2025 by Patrick Heyer <PatTheMav@users.noreply.github.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <models/SceneCatalogPackage.hpp>

#include <QNetworkAccessManager>
#include <QPointer>
#include <QWidget>

#include <filesystem>
#include <vector>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class OBSBasic;

class SceneCatalogDockWidget : public QWidget {
	Q_OBJECT

public:
	explicit SceneCatalogDockWidget(OBSBasic *mainWindow, QWidget *parent = nullptr);

private slots:
	void refreshCatalog();
	void filterCatalog(const QString &text);
	void handleSelectionChanged();
	void installSelectedPackage();
	void loadPreviewImage();

private:
	bool fetchCatalog(std::string &errorMessage);
	bool downloadPackage(const OBS::SceneCatalogEntry &entry, OBS::SceneCatalogPackage &package,
			    std::filesystem::path &packageRoot, std::string &errorMessage);
	void populateList(const QString &filterText);
	void updateDetails(const OBS::SceneCatalogEntry *entry);

	OBSBasic *mainWindow_ = nullptr;
	OBS::SceneCatalogApiConfig apiConfig_{};
	std::vector<OBS::SceneCatalogEntry> entries_;
	QPointer<QListWidget> listWidget_;
	QPointer<QLineEdit> searchField_;
	QPointer<QLabel> titleLabel_;
	QPointer<QLabel> versionLabel_;
	QPointer<QLabel> compatibilityLabel_;
	QPointer<QLabel> typeLabel_;
	QPointer<QLabel> descriptionLabel_;
	QPointer<QLabel> previewLabel_;
	QPointer<QPushButton> installButton_;
	QPointer<QPushButton> refreshButton_;
	QNetworkAccessManager network_;
};
