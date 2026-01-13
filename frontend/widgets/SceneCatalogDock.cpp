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

#include "SceneCatalogDock.hpp"

#include "OBSBasic.hpp"

#include <obs-app.hpp>
#include <obs-data.h>

#include <QApplication>
#include <QAbstractItemView>
#include <QEventLoop>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <filesystem>

namespace {
QByteArray fetchUrl(QNetworkAccessManager &network, const QUrl &url, std::string &errorMessage)
{
	QNetworkRequest request(url);
	QEventLoop loop;
	QNetworkReply *reply = network.get(request);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (reply->error() != QNetworkReply::NoError) {
		errorMessage = reply->errorString().toStdString();
		reply->deleteLater();
		return {};
	}

	QByteArray payload = reply->readAll();
	reply->deleteLater();
	return payload;
}

QString catalogItemLabel(const OBS::SceneCatalogEntry &entry)
{
	QString name = QString::fromStdString(entry.name);
	QString type = QString::fromStdString(entry.type.empty() ? "scene" : entry.type);
	return QString("%1 (%2)").arg(name, type);
}
} // namespace

SceneCatalogDockWidget::SceneCatalogDockWidget(OBSBasic *mainWindow, QWidget *parent)
	: QWidget(parent),
	  mainWindow_(mainWindow)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(6);

	auto *toolbarLayout = new QHBoxLayout();
	searchField_ = new QLineEdit(this);
	searchField_->setPlaceholderText(tr("Search scenes and addons"));

	refreshButton_ = new QPushButton(tr("Refresh"), this);
	toolbarLayout->addWidget(searchField_);
	toolbarLayout->addWidget(refreshButton_);

	listWidget_ = new QListWidget(this);
	listWidget_->setSelectionMode(QAbstractItemView::SingleSelection);

	auto *detailsLayout = new QVBoxLayout();
	titleLabel_ = new QLabel(tr("Select a package to see details."), this);
	titleLabel_->setWordWrap(true);
	versionLabel_ = new QLabel(this);
	compatibilityLabel_ = new QLabel(this);
	typeLabel_ = new QLabel(this);
	descriptionLabel_ = new QLabel(this);
	descriptionLabel_->setWordWrap(true);
	previewLabel_ = new QLabel(this);
	previewLabel_->setMinimumHeight(140);
	previewLabel_->setAlignment(Qt::AlignCenter);
	previewLabel_->setStyleSheet("background-color: #1b1b1b; border: 1px solid #333;");

	installButton_ = new QPushButton(tr("Install"), this);
	installButton_->setEnabled(false);

	detailsLayout->addWidget(titleLabel_);
	detailsLayout->addWidget(versionLabel_);
	detailsLayout->addWidget(compatibilityLabel_);
	detailsLayout->addWidget(typeLabel_);
	detailsLayout->addWidget(descriptionLabel_);
	detailsLayout->addWidget(previewLabel_);
	detailsLayout->addWidget(installButton_);
	detailsLayout->addStretch();

	layout->addLayout(toolbarLayout);
	layout->addWidget(listWidget_, 3);
	layout->addLayout(detailsLayout, 2);

	connect(refreshButton_, &QPushButton::clicked, this, &SceneCatalogDockWidget::refreshCatalog);
	connect(searchField_, &QLineEdit::textChanged, this, &SceneCatalogDockWidget::filterCatalog);
	connect(listWidget_, &QListWidget::currentRowChanged, this, &SceneCatalogDockWidget::handleSelectionChanged);
	connect(installButton_, &QPushButton::clicked, this, &SceneCatalogDockWidget::installSelectedPackage);

	refreshCatalog();
}

void SceneCatalogDockWidget::refreshCatalog()
{
	std::string errorMessage;
	if (!fetchCatalog(errorMessage)) {
		QMessageBox::warning(this, tr("Scene Catalog"), QString::fromStdString(errorMessage));
		return;
	}

	populateList(searchField_->text());
}

void SceneCatalogDockWidget::filterCatalog(const QString &text)
{
	populateList(text);
}

void SceneCatalogDockWidget::handleSelectionChanged()
{
	QListWidgetItem *item = listWidget_->currentItem();
	if (!item) {
		updateDetails(nullptr);
		return;
	}

	bool ok = false;
	int index = item->data(Qt::UserRole).toInt(&ok);
	if (!ok || index < 0 || index >= static_cast<int>(entries_.size())) {
		updateDetails(nullptr);
		return;
	}

	updateDetails(&entries_[index]);
	loadPreviewImage();
}

void SceneCatalogDockWidget::installSelectedPackage()
{
	QListWidgetItem *item = listWidget_->currentItem();
	if (!item || !mainWindow_)
		return;

	bool ok = false;
	int index = item->data(Qt::UserRole).toInt(&ok);
	if (!ok || index < 0 || index >= static_cast<int>(entries_.size()))
		return;

	const OBS::SceneCatalogEntry &entry = entries_[index];
	OBS::SceneCatalogPackage package;
	std::filesystem::path packageRoot;
	std::string errorMessage;

	if (!downloadPackage(entry, package, packageRoot, errorMessage)) {
		QMessageBox::warning(this, tr("Scene Catalog"), QString::fromStdString(errorMessage));
		return;
	}

	if (!mainWindow_->ImportSceneCatalogPackage(package, packageRoot, errorMessage)) {
		QMessageBox::warning(this, tr("Scene Catalog"), QString::fromStdString(errorMessage));
		return;
	}

	QMessageBox::information(this, tr("Scene Catalog"), tr("Scene collection installed successfully."));
}

void SceneCatalogDockWidget::loadPreviewImage()
{
	QListWidgetItem *item = listWidget_->currentItem();
	if (!item) {
		previewLabel_->setText(tr("No preview available."));
		previewLabel_->setPixmap(QPixmap());
		return;
	}

	bool ok = false;
	int index = item->data(Qt::UserRole).toInt(&ok);
	if (!ok || index < 0 || index >= static_cast<int>(entries_.size())) {
		previewLabel_->setText(tr("No preview available."));
		previewLabel_->setPixmap(QPixmap());
		return;
	}

	const OBS::SceneCatalogEntry &entry = entries_[index];
	if (entry.previewUrl.empty()) {
		previewLabel_->setText(tr("No preview available."));
		previewLabel_->setPixmap(QPixmap());
		return;
	}

	std::string errorMessage;
	QByteArray payload = fetchUrl(network_, QUrl(QString::fromStdString(entry.previewUrl)), errorMessage);
	if (payload.isEmpty()) {
		previewLabel_->setText(tr("Preview unavailable."));
		previewLabel_->setPixmap(QPixmap());
		return;
	}

	QPixmap pixmap;
	pixmap.loadFromData(payload);
	previewLabel_->setPixmap(pixmap.scaled(previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	previewLabel_->setText(QString());
}

bool SceneCatalogDockWidget::fetchCatalog(std::string &errorMessage)
{
	entries_.clear();
	QUrl url(QString::fromStdString(apiConfig_.packagesUrl()));
	QByteArray payload = fetchUrl(network_, url, errorMessage);
	if (payload.isEmpty()) {
		if (errorMessage.empty())
			errorMessage = "Empty response from scene catalog.";
		return false;
	}

	OBSDataAutoRelease data = obs_data_create_from_json(payload.constData());
	if (!data) {
		errorMessage = "Unable to parse scene catalog response.";
		return false;
	}

	OBSDataArrayAutoRelease packages = obs_data_get_array(data, "packages");
	if (!packages) {
		errorMessage = "Scene catalog response did not include packages.";
		return false;
	}

	size_t count = obs_data_array_count(packages);
	entries_.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		OBSDataAutoRelease item = obs_data_array_item(packages, i);
		OBS::SceneCatalogEntry entry;
		entry.id = obs_data_get_string(item, "id");
		entry.name = obs_data_get_string(item, "name");
		entry.version = obs_data_get_string(item, "version");
		entry.type = obs_data_get_string(item, "type");
		entry.summary = obs_data_get_string(item, "summary");
		entry.obsMinVersion = obs_data_get_string(item, "obs_min_version");
		entry.previewUrl = obs_data_get_string(item, "preview_url");
		entry.manifestUrl = obs_data_get_string(item, "manifest_url");
		entry.packageUrl = obs_data_get_string(item, "package_url");
		if (!entry.id.empty())
			entries_.push_back(std::move(entry));
	}

	return true;
}

bool SceneCatalogDockWidget::downloadPackage(const OBS::SceneCatalogEntry &entry, OBS::SceneCatalogPackage &package,
				     std::filesystem::path &packageRoot, std::string &errorMessage)
{
	std::string manifestUrl = entry.manifestUrl.empty() ? entry.packageUrl : entry.manifestUrl;
	if (manifestUrl.empty()) {
		errorMessage = "Scene catalog entry is missing a manifest URL.";
		return false;
	}

	QByteArray manifestPayload = fetchUrl(network_, QUrl(QString::fromStdString(manifestUrl)), errorMessage);
	if (manifestPayload.isEmpty()) {
		if (errorMessage.empty())
			errorMessage = "Failed to download scene package manifest.";
		return false;
	}

	if (!OBS::SceneCatalogPackage::LoadFromJson(manifestPayload.toStdString(), package, errorMessage))
		return false;

	const std::string currentVersion = App()->GetVersionString();
	if (!package.validateCompatibility(currentVersion, errorMessage))
		return false;

	packageRoot = App()->userScenesLocation / std::filesystem::u8path("scene-packages") /
		      std::filesystem::u8path(package.id) / std::filesystem::u8path(package.version);

	std::filesystem::create_directories(packageRoot);

	QUrl manifestBase(QString::fromStdString(manifestUrl));
	manifestBase.setPath(QFileInfo(manifestBase.path()).path() + "/");

	for (const auto &resource : package.resources) {
		if (resource.url.empty())
			continue;

		QUrl resourceUrl = manifestBase.resolved(QUrl(QString::fromStdString(resource.url)));
		QByteArray resourceData = fetchUrl(network_, resourceUrl, errorMessage);
		if (resourceData.isEmpty()) {
			errorMessage = "Failed to download resource: " + resource.path;
			return false;
		}

		std::filesystem::path destination = packageRoot / std::filesystem::u8path(resource.path);
		std::filesystem::create_directories(destination.parent_path());

		QFile file(QString::fromStdString(destination.u8string()));
		if (!file.open(QIODevice::WriteOnly)) {
			errorMessage = "Failed to write resource file: " + resource.path;
			return false;
		}
		file.write(resourceData);
		file.close();
	}

	if (!package.validateResources(packageRoot, errorMessage))
		return false;

	return true;
}

void SceneCatalogDockWidget::populateList(const QString &filterText)
{
	listWidget_->clear();

	QString lowered = filterText.toLower();
	for (size_t i = 0; i < entries_.size(); ++i) {
		const auto &entry = entries_[i];
		QString name = QString::fromStdString(entry.name).toLower();
		QString summary = QString::fromStdString(entry.summary).toLower();
		QString type = QString::fromStdString(entry.type).toLower();

		if (!lowered.isEmpty() && !name.contains(lowered) && !summary.contains(lowered) && !type.contains(lowered))
			continue;

		auto *item = new QListWidgetItem(catalogItemLabel(entry));
		item->setData(Qt::UserRole, static_cast<int>(i));
		listWidget_->addItem(item);
	}

	updateDetails(nullptr);
}

void SceneCatalogDockWidget::updateDetails(const OBS::SceneCatalogEntry *entry)
{
	if (!entry) {
		titleLabel_->setText(tr("Select a package to see details."));
		versionLabel_->clear();
		compatibilityLabel_->clear();
		typeLabel_->clear();
		descriptionLabel_->clear();
		previewLabel_->setText(tr("No preview available."));
		previewLabel_->setPixmap(QPixmap());
		installButton_->setEnabled(false);
		return;
	}

	QString name = QString::fromStdString(entry->name);
	titleLabel_->setText(name);
	versionLabel_->setText(tr("Version: %1").arg(QString::fromStdString(entry->version)));
	compatibilityLabel_->setText(tr("OBS min: %1").arg(QString::fromStdString(entry->obsMinVersion)));
	typeLabel_->setText(tr("Type: %1").arg(QString::fromStdString(entry->type)));
	descriptionLabel_->setText(QString::fromStdString(entry->summary));
	previewLabel_->setText(tr("Loading preview..."));
	installButton_->setEnabled(true);
}
