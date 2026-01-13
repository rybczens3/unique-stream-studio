/******************************************************************************
    Copyright (C) 2025 by FiniteSingularity <finitesingularityttv@gmail.com>

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

#include "PluginManagerWindow.hpp"

#include <OBSApp.hpp>
#include <qt-wrappers.hpp>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

#include "moc_PluginManagerWindow.cpp"

namespace OBS {

PluginManagerWindow::PluginManagerWindow(PluginManager *manager, std::vector<ModuleInfo> const &modules,
					 PortalSession const &session, std::string const &portalBaseUrl,
					 QWidget *parent)
	: QDialog(parent),
	  manager_(manager),
	  modules_(modules),
	  portalSession_(session),
	  portalBaseUrl_(portalBaseUrl),
	  ui(new Ui::PluginManagerWindow)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->setupUi(this);

	ui->modulesListContainer->viewport()->setAutoFillBackground(false);
	ui->modulesListContents->setAutoFillBackground(false);

	// Set up sidebar entries
	ui->sectionList->clear();
	ui->sectionList->setSelectionMode(QAbstractItemView::SingleSelection);

	connect(ui->sectionList, &QListWidget::itemSelectionChanged, this,
		&PluginManagerWindow::sectionSelectionChanged);

	QListWidgetItem *browse = new QListWidgetItem(QTStr("PluginManager.Section.Discover"));
	ui->sectionList->addItem(browse);

	QListWidgetItem *installed = new QListWidgetItem(QTStr("PluginManager.Section.Manage"));
	ui->sectionList->addItem(installed);

	QListWidgetItem *updates = new QListWidgetItem(QTStr("PluginManager.Section.Updates"));
	ui->sectionList->addItem(updates);

	setSection(ui->sectionList->indexFromItem(installed));
	setupConnections_();

	std::sort(modules_.begin(), modules_.end(), [](const ModuleInfo &a, const ModuleInfo &b) {
		std::string aName = !a.display_name.empty() ? a.display_name : a.module_name;
		std::string bName = !b.display_name.empty() ? b.display_name : b.module_name;
		return aName < bName;
	});

	int row = 0;
	for (auto &metadata : modules_) {
		std::string id = metadata.module_name;
		// Check if the module is missing:
		bool missing = !obs_get_module(id.c_str()) && !obs_get_disabled_module(id.c_str());

		QString name = !metadata.display_name.empty() ? metadata.display_name.c_str()
							      : metadata.module_name.c_str();
		if (missing) {
			name += " " + QTStr("PluginManager.MissingPlugin");
		}

		auto item = new QCheckBox(name);
		item->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
		item->setChecked(metadata.enabled);

		if (!metadata.enabledAtLaunch) {
			item->setProperty("class", "text-muted");
		}

		if (missing) {
			item->setEnabled(false);
		}
		ui->modulesList->layout()->addWidget(item);

		connect(item, &QCheckBox::toggled, this, [this, row](bool checked) {
			modules_[row].enabled = checked;
			ui->manageRestartLabel->setVisible(isEnabledPluginsChanged());
		});

		row++;
	}

	ui->modulesList->adjustSize();
	ui->modulesListContents->adjustSize();

	ui->manageRestartLabel->setVisible(isEnabledPluginsChanged());

	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PluginManagerWindow::sectionSelectionChanged()
{
	auto selected = ui->sectionList->selectedItems();
	if (selected.count() != 1) {
		setSection(activeSectionIndex);
	} else {
		auto selectionIndex = ui->sectionList->indexFromItem(selected.first());
		setSection(selectionIndex);
	}
}

void PluginManagerWindow::setSection(QPersistentModelIndex index)
{
	if (ui->sectionList->itemFromIndex(index)) {
		activeSectionIndex = index;
		ui->sectionList->setCurrentIndex(index);
	}

	switch (index.row()) {
	case 0:
		ui->sectionStack->setCurrentWidget(ui->discoverPage);
		loadDiscover_();
		break;
	case 2:
		ui->sectionStack->setCurrentWidget(ui->updatesPage);
		loadUpdates_();
		break;
	default:
		ui->sectionStack->setCurrentWidget(ui->managePage);
		break;
	}
}

bool PluginManagerWindow::isEnabledPluginsChanged()
{
	bool result = false;
	for (auto &metadata : modules_) {
		if (metadata.enabledAtLaunch != metadata.enabled) {
			result = true;
			break;
		}
	}

	return result;
}

void PluginManagerWindow::setupConnections_()
{
	connect(ui->discoverSearchButton, &QPushButton::clicked, this, &PluginManagerWindow::loadDiscover_);
	connect(ui->discoverSearchInput, &QLineEdit::returnPressed, this, &PluginManagerWindow::loadDiscover_);
	connect(ui->updatesRefreshButton, &QPushButton::clicked, this, &PluginManagerWindow::loadUpdates_);
	connect(ui->accountLoginButton, &QPushButton::clicked, this, &PluginManagerWindow::handleLogin_);
	connect(ui->accountLogoutButton, &QPushButton::clicked, this, &PluginManagerWindow::handleLogout_);
	refreshAccountUi_();
}

void PluginManagerWindow::refreshAccountUi_()
{
	if (!portalSession_.accessToken.empty()) {
		ui->accountStatusLabel->setText(QString::fromStdString(portalSession_.username));
		ui->accountRoleLabel->setText(QString::fromStdString(portalSession_.role));
		ui->accountLoginButton->setEnabled(false);
		ui->accountLogoutButton->setEnabled(true);
	} else {
		ui->accountStatusLabel->setText(QTStr("PluginManager.Account.Guest"));
		ui->accountRoleLabel->setText(QTStr("PluginManager.Account.RoleGuest"));
		ui->accountLoginButton->setEnabled(true);
		ui->accountLogoutButton->setEnabled(false);
	}
}

void PluginManagerWindow::handleLogin_()
{
	QNetworkAccessManager network;
	QNetworkRequest request(QUrl(QString::fromStdString(portalBaseUrl_) + "/auth/login"));
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QJsonObject payload;
	payload.insert("username", ui->accountUsernameInput->text());
	payload.insert("password", ui->accountPasswordInput->text());

	QEventLoop loop;
	QNetworkReply *reply =
		network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (reply->error() != QNetworkReply::NoError) {
		OBSMessageBox::warning(this, QTStr("Warning"), QTStr("PluginManager.Account.LoginFailed"));
		reply->deleteLater();
		return;
	}

	const QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
	reply->deleteLater();
	if (!response.isObject()) {
		OBSMessageBox::warning(this, QTStr("Warning"), QTStr("PluginManager.Account.LoginFailed"));
		return;
	}

	const QJsonObject obj = response.object();
	portalSession_.username = obj.value("username").toString().toStdString();
	portalSession_.role = obj.value("role").toString().toStdString();
	portalSession_.accessToken = obj.value("access_token").toString().toStdString();
	portalSession_.refreshToken = obj.value("refresh_token").toString().toStdString();

	refreshAccountUi_();
}

void PluginManagerWindow::handleLogout_()
{
	portalSession_ = {};
	refreshAccountUi_();
}

std::vector<PluginManagerWindow::CatalogEntry> PluginManagerWindow::fetchCatalog_(const QString &query)
{
	std::vector<CatalogEntry> entries;
	QString url = QString::fromStdString(portalBaseUrl_) + "/plugins";
	if (!query.trimmed().isEmpty()) {
		url += "?query=" + QUrl::toPercentEncoding(query.trimmed());
	}

	QNetworkAccessManager network;
	QNetworkRequest request(QUrl(url));
	if (!portalSession_.accessToken.empty()) {
		request.setRawHeader("Authorization", QByteArray("Bearer ") +
							     QByteArray::fromStdString(portalSession_.accessToken));
	}

	QEventLoop loop;
	QNetworkReply *reply = network.get(request);
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (reply->error() != QNetworkReply::NoError) {
		reply->deleteLater();
		return entries;
	}

	const QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
	reply->deleteLater();
	if (!response.isArray()) {
		return entries;
	}

	for (const auto &item : response.array()) {
		if (!item.isObject()) {
			continue;
		}
		const QJsonObject obj = item.toObject();
		CatalogEntry entry;
		entry.id = obj.value("id").toString();
		entry.name = obj.value("name").toString();
		entry.version = obj.value("version").toString();
		entry.compatibility = obj.value("compatibility").toString();
		entry.packageUrl = obj.value("package_url").toString();
		entry.sha256 = obj.value("sha256").toString();
		entry.signature = obj.value("signature").toString();
		entries.push_back(entry);
	}

	return entries;
}

void PluginManagerWindow::loadDiscover_()
{
	const auto entries = fetchCatalog_(ui->discoverSearchInput->text());
	populateDiscoverList_(entries);
}

void PluginManagerWindow::loadUpdates_()
{
	const auto entries = fetchCatalog_(QString());
	populateUpdatesList_(entries);
}

void PluginManagerWindow::populateDiscoverList_(const std::vector<CatalogEntry> &entries)
{
	clearLayout_(ui->discoverListInnerLayout);
	for (const auto &entry : entries) {
		auto row = new QFrame(ui->discoverList);
		auto layout = new QHBoxLayout(row);

		auto nameLabel = new QLabel(entry.name.isEmpty() ? entry.id : entry.name, row);
		auto versionLabel = new QLabel(entry.version, row);
		auto installButton = new QPushButton(QTStr("PluginManager.Discover.Install"), row);

		layout->addWidget(nameLabel);
		layout->addWidget(versionLabel);
		layout->addStretch();
		layout->addWidget(installButton);

		connect(installButton, &QPushButton::clicked, this, [this, entry]() {
			if (!manager_) {
				return;
			}

			PluginPackageMetadata metadata;
			metadata.id = entry.id.toStdString();
			metadata.name = entry.name.toStdString();
			metadata.version = entry.version.toStdString();
			metadata.compatibility = entry.compatibility.toStdString();
			metadata.packageUrl = entry.packageUrl.toStdString();
			metadata.sha256 = entry.sha256.toStdString();
			metadata.signature = entry.signature.toStdString();

			std::string errorMessage;
			if (!manager_->downloadAndInstallPackage(metadata, portalSession_, errorMessage)) {
				OBSMessageBox::warning(this, QTStr("Warning"),
						       QString::fromStdString(errorMessage));
				return;
			}

			OBSMessageBox::information(this, QTStr("PluginManager.Success"),
						   QTStr("PluginManager.Discover.Installed"));
		});

		ui->discoverListInnerLayout->addWidget(row);
	}
	ui->discoverListInnerLayout->addStretch();
}

void PluginManagerWindow::populateUpdatesList_(const std::vector<CatalogEntry> &entries)
{
	clearLayout_(ui->updatesListInnerLayout);

	for (const auto &entry : entries) {
		auto it = std::find_if(modules_.begin(), modules_.end(),
				       [&entry](const ModuleInfo &module) {
					       return QString::fromStdString(module.module_name) == entry.id ||
						      QString::fromStdString(module.id) == entry.id;
				       });
		if (it == modules_.end()) {
			continue;
		}

		if (compareVersions_(QString::fromStdString(it->version), entry.version) >= 0) {
			continue;
		}

		auto row = new QFrame(ui->updatesList);
		auto layout = new QHBoxLayout(row);

		auto nameLabel = new QLabel(entry.name.isEmpty() ? entry.id : entry.name, row);
		auto updateButton = new QPushButton(QTStr("PluginManager.Updates.Update"), row);

		layout->addWidget(nameLabel);
		layout->addWidget(new QLabel(QString::fromStdString(it->version) + " → " + entry.version, row));
		layout->addStretch();
		layout->addWidget(updateButton);

		connect(updateButton, &QPushButton::clicked, this, [this, entry]() {
			if (!manager_) {
				return;
			}

			PluginPackageMetadata metadata;
			metadata.id = entry.id.toStdString();
			metadata.name = entry.name.toStdString();
			metadata.version = entry.version.toStdString();
			metadata.compatibility = entry.compatibility.toStdString();
			metadata.packageUrl = entry.packageUrl.toStdString();
			metadata.sha256 = entry.sha256.toStdString();
			metadata.signature = entry.signature.toStdString();

			std::string errorMessage;
			if (!manager_->downloadAndInstallPackage(metadata, portalSession_, errorMessage)) {
				OBSMessageBox::warning(this, QTStr("Warning"),
						       QString::fromStdString(errorMessage));
				return;
			}

			OBSMessageBox::information(this, QTStr("PluginManager.Success"),
						   QTStr("PluginManager.Updates.Installed"));
		});

		ui->updatesListInnerLayout->addWidget(row);
	}

	ui->updatesListInnerLayout->addStretch();
}

void PluginManagerWindow::clearLayout_(QLayout *layout)
{
	while (layout->count() > 0) {
		QLayoutItem *item = layout->takeAt(0);
		if (item->widget()) {
			item->widget()->deleteLater();
		}
		delete item;
	}
}

int PluginManagerWindow::compareVersions_(const QString &left, const QString &right) const
{
	const QStringList leftParts = left.split('.', Qt::SkipEmptyParts);
	const QStringList rightParts = right.split('.', Qt::SkipEmptyParts);

	const int maxCount = std::max(leftParts.size(), rightParts.size());
	for (int i = 0; i < maxCount; ++i) {
		const int leftValue = i < leftParts.size() ? leftParts[i].toInt() : 0;
		const int rightValue = i < rightParts.size() ? rightParts[i].toInt() : 0;
		if (leftValue != rightValue) {
			return leftValue < rightValue ? -1 : 1;
		}
	}

	return 0;
}

}; // namespace OBS
