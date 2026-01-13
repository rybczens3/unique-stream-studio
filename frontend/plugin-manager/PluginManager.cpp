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

#include "PluginManager.hpp"
#include "PluginManagerWindow.hpp"

#include <OBSApp.hpp>
#include <qt-wrappers.hpp>
#include <widgets/OBSBasic.hpp>

#include <QMessageBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

extern bool restart;

namespace OBS {

void addModuleToPluginManagerImpl(void *param, obs_module_t *newModule)
{
	auto &instance = *static_cast<OBS::PluginManager *>(param);
	std::string moduleName = obs_get_module_file_name(newModule);
	moduleName = moduleName.substr(0, moduleName.rfind("."));

	if (!obs_get_module_allow_disable(moduleName.c_str()))
		return;

	const char *display_name = obs_get_module_name(newModule);
	std::string module_name = moduleName;
	const char *id = obs_get_module_id(newModule);
	const char *version = obs_get_module_version(newModule);

	auto it = std::find_if(instance.modules_.begin(), instance.modules_.end(),
			       [&](OBS::ModuleInfo module) { return module.module_name == moduleName; });

	if (it == instance.modules_.end()) {
		instance.modules_.push_back({display_name ? display_name : "", module_name, id ? id : "",
					     version ? version : "", true, true});
	} else {
		it->display_name = display_name ? display_name : "";
		it->module_name = module_name;
		it->id = id ? id : "";
		it->version = version ? version : "";
	}
}

constexpr std::string_view OBSPluginManagerPath = "obs-studio/plugin_manager";
constexpr std::string_view OBSPluginManagerModulesFile = "modules.json";
constexpr std::string_view OBSPluginManagerPortalFile = "portal.json";

void PluginManager::preLoad()
{
	loadModules_();
	loadPortalSession_();
	disableModules_();
}

void PluginManager::postLoad()
{
	// Find any new modules and add to Plugin Manager.
	obs_enum_modules(addModuleToPluginManager, this);
	// Get list of valid module types.
	addModuleTypes_();
	saveModules_();
	// Add provided features from any unloaded modules
	linkUnloadedModules_();
}

std::filesystem::path PluginManager::getConfigFilePath_()
{
	std::filesystem::path path = App()->userPluginManagerSettingsLocation /
				     std::filesystem::u8path(OBSPluginManagerPath) /
				     std::filesystem::u8path(OBSPluginManagerModulesFile);
	return path;
}

std::filesystem::path PluginManager::getPortalConfigFilePath_()
{
	std::filesystem::path path = App()->userPluginManagerSettingsLocation /
				     std::filesystem::u8path(OBSPluginManagerPath) /
				     std::filesystem::u8path(OBSPluginManagerPortalFile);
	return path;
}

void PluginManager::loadModules_()
{
	auto modulesFile = getConfigFilePath_();
	if (std::filesystem::exists(modulesFile)) {
		std::ifstream jsonFile(modulesFile);
		nlohmann::json data;
		try {
			data = nlohmann::json::parse(jsonFile);
		} catch (const nlohmann::json::parse_error &error) {
			modules_.clear();
			blog(LOG_ERROR, "Error loading modules config file: %s", error.what());
			blog(LOG_ERROR, "Generating new config file.");
			return;
		}
		modules_.clear();
		for (auto it : data) {
			ModuleInfo obsModule;
			try {
				obsModule = {it.at("display_name"),
					     it.at("module_name"),
					     it.at("id"),
					     it.at("version"),
					     it.at("enabled"),
					     it.at("enabled"),
					     it.at("sources"),
					     it.at("outputs"),
					     it.at("encoders"),
					     it.at("services"),
					     {},
					     {},
					     {},
					     {}};
			} catch (const nlohmann::json::out_of_range &error) {
				blog(LOG_WARNING, "Error loading module info: %s", error.what());
				continue;
			}
			modules_.push_back(obsModule);
		}
	}
}

void PluginManager::loadPortalSession_()
{
	auto sessionFile = getPortalConfigFilePath_();
	if (!std::filesystem::exists(sessionFile)) {
		return;
	}

	std::ifstream jsonFile(sessionFile);
	nlohmann::json data;
	try {
		data = nlohmann::json::parse(jsonFile);
	} catch (const nlohmann::json::parse_error &error) {
		blog(LOG_ERROR, "Error loading portal config file: %s", error.what());
		return;
	}

	portalSession_.username = data.value("username", "");
	portalSession_.role = data.value("role", "");
	portalSession_.accessToken = data.value("access_token", "");
	portalSession_.refreshToken = data.value("refresh_token", "");
	portalBaseUrl_ = data.value("base_url", portalBaseUrl_);
}

void PluginManager::linkUnloadedModules_()
{
	for (const auto &moduleInfo : modules_) {
		if (!moduleInfo.enabled) {
			auto obsModule = obs_get_disabled_module(moduleInfo.module_name.c_str());
			if (!obsModule) {
				continue;
			}
			for (const auto &source : moduleInfo.sources) {
				obs_module_add_source(obsModule, source.c_str());
			}
			for (const auto &output : moduleInfo.outputs) {
				obs_module_add_output(obsModule, output.c_str());
			}
			for (const auto &encoder : moduleInfo.encoders) {
				obs_module_add_encoder(obsModule, encoder.c_str());
			}
			for (const auto &service : moduleInfo.services) {
				obs_module_add_service(obsModule, service.c_str());
			}
		}
	}
}

void PluginManager::saveModules_()
{
	auto modulesFile = getConfigFilePath_();
	std::ofstream outFile(modulesFile);
	nlohmann::json data = nlohmann::json::array();

	for (auto const &moduleInfo : modules_) {
		nlohmann::json modData;
		modData["display_name"] = moduleInfo.display_name;
		modData["module_name"] = moduleInfo.module_name;
		modData["id"] = moduleInfo.id;
		modData["version"] = moduleInfo.version;
		modData["enabled"] = moduleInfo.enabled;
		modData["sources"] = moduleInfo.sources;
		modData["outputs"] = moduleInfo.outputs;
		modData["encoders"] = moduleInfo.encoders;
		modData["services"] = moduleInfo.services;
		data.push_back(modData);
	}
	outFile << std::setw(4) << data << std::endl;
}

void PluginManager::savePortalSession_()
{
	auto sessionFile = getPortalConfigFilePath_();
	std::filesystem::create_directories(sessionFile.parent_path());
	std::ofstream outFile(sessionFile);
	nlohmann::json data;
	data["username"] = portalSession_.username;
	data["role"] = portalSession_.role;
	data["access_token"] = portalSession_.accessToken;
	data["refresh_token"] = portalSession_.refreshToken;
	data["base_url"] = portalBaseUrl_;
	outFile << std::setw(4) << data << std::endl;
}

bool PluginManager::verifyPackageHash_(const std::filesystem::path &packagePath, const std::string &expectedSha256,
				       std::string &errorMessage) const
{
	if (expectedSha256.empty()) {
		errorMessage = "Missing expected SHA256 hash for package.";
		return false;
	}

	QFile file(QString::fromStdString(packagePath.u8string()));
	if (!file.open(QIODevice::ReadOnly)) {
		errorMessage = "Unable to read downloaded package for hash verification.";
		return false;
	}

	QCryptographicHash hash(QCryptographicHash::Sha256);
	if (!hash.addData(&file)) {
		errorMessage = "Unable to compute package hash.";
		return false;
	}

	const QString computed = hash.result().toHex();
	if (computed.compare(QString::fromStdString(expectedSha256), Qt::CaseInsensitive) != 0) {
		errorMessage = "Package hash mismatch.";
		return false;
	}

	return true;
}

bool PluginManager::verifyPackageSignature_(const std::string &sha256, const std::string &signature,
					    std::string &errorMessage) const
{
	if (signature.empty()) {
		errorMessage = "Missing package signature.";
		return false;
	}

	const std::string expectedSignature = "sha256:" + sha256;
	if (signature != expectedSignature) {
		errorMessage = "Package signature verification failed.";
		return false;
	}

	return true;
}

bool PluginManager::downloadAndInstallPackage(const PluginPackageMetadata &metadata, const PortalSession &session,
					      std::string &errorMessage)
{
	if (metadata.packageUrl.empty()) {
		errorMessage = "Missing package URL.";
		return false;
	}

	QNetworkAccessManager network;
	QNetworkRequest request(QUrl(QString::fromStdString(metadata.packageUrl)));
	if (!session.accessToken.empty()) {
		const QByteArray token = "Bearer " + QByteArray::fromStdString(session.accessToken);
		request.setRawHeader("Authorization", token);
	}

	QEventLoop loop;
	QNetworkReply *reply = network.get(request);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (reply->error() != QNetworkReply::NoError) {
		errorMessage = "Failed to download package.";
		reply->deleteLater();
		return false;
	}

	QByteArray packageData = reply->readAll();
	reply->deleteLater();

	std::filesystem::path tempDir = std::filesystem::temp_directory_path() /
					std::filesystem::path("obs-plugin-packages");
	std::filesystem::create_directories(tempDir);

	std::string tempFileName = metadata.id + "-" + metadata.version + ".pkg";
	std::filesystem::path tempPath = tempDir / tempFileName;

	QFile tempFile(QString::fromStdString(tempPath.u8string()));
	if (!tempFile.open(QIODevice::WriteOnly)) {
		errorMessage = "Unable to write downloaded package.";
		return false;
	}
	tempFile.write(packageData);
	tempFile.close();

	if (!verifyPackageHash_(tempPath, metadata.sha256, errorMessage)) {
		return false;
	}

	if (!verifyPackageSignature_(metadata.sha256, metadata.signature, errorMessage)) {
		return false;
	}

	std::filesystem::path pluginsRoot = App()->userPluginManagerSettingsLocation /
					    std::filesystem::u8path("obs-studio/plugins");
	std::filesystem::path pluginDir = pluginsRoot / std::filesystem::u8path(metadata.id);
	std::filesystem::path installPath = pluginDir / std::filesystem::u8path(tempFileName);

	std::filesystem::path backupDir;
	if (std::filesystem::exists(pluginDir)) {
		std::string timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMddhhmmss").toStdString();
		backupDir = pluginDir;
		backupDir += ".backup-" + timestamp;
		std::filesystem::rename(pluginDir, backupDir);
	}

	try {
		std::filesystem::create_directories(pluginDir);
		std::filesystem::copy_file(tempPath, installPath, std::filesystem::copy_options::overwrite_existing);
	} catch (const std::filesystem::filesystem_error &error) {
		errorMessage = std::string("Failed to install package: ") + error.what();
		if (!backupDir.empty() && std::filesystem::exists(backupDir)) {
			std::filesystem::rename(backupDir, pluginDir);
		}
		return false;
	}

	if (!backupDir.empty() && std::filesystem::exists(backupDir)) {
		std::filesystem::remove_all(backupDir);
	}

	return true;
}

void PluginManager::addModuleTypes_()
{
	const char *source_id;
	int i = 0;
	while (obs_enum_source_types(i, &source_id)) {
		i += 1;
		obs_module_t *obsModule = obs_source_get_module(source_id);
		if (!obsModule) {
			continue;
		}
		std::string moduleName = obs_get_module_file_name(obsModule);
		moduleName = moduleName.substr(0, moduleName.rfind("."));
		auto it = std::find_if(modules_.begin(), modules_.end(),
				       [moduleName](ModuleInfo const &m) { return m.module_name == moduleName; });
		if (it != modules_.end()) {
			it->sourcesLoaded.push_back(source_id);
		}
	}

	const char *output_id;
	i = 0;
	while (obs_enum_output_types(i, &output_id)) {
		i += 1;
		obs_module_t *obsModule = obs_output_get_module(output_id);
		if (!obsModule) {
			continue;
		}
		std::string moduleName = obs_get_module_file_name(obsModule);
		moduleName = moduleName.substr(0, moduleName.rfind("."));
		auto it = std::find_if(modules_.begin(), modules_.end(),
				       [moduleName](ModuleInfo const &m) { return m.module_name == moduleName; });
		if (it != modules_.end()) {
			it->outputsLoaded.push_back(output_id);
		}
	}

	const char *encoder_id;
	i = 0;
	while (obs_enum_encoder_types(i, &encoder_id)) {
		i += 1;
		obs_module_t *obsModule = obs_encoder_get_module(encoder_id);
		if (!obsModule) {
			continue;
		}
		std::string moduleName = obs_get_module_file_name(obsModule);
		moduleName = moduleName.substr(0, moduleName.rfind("."));
		auto it = std::find_if(modules_.begin(), modules_.end(),
				       [moduleName](ModuleInfo const &m) { return m.module_name == moduleName; });
		if (it != modules_.end()) {
			it->encodersLoaded.push_back(encoder_id);
		}
	}

	const char *service_id;
	i = 0;
	while (obs_enum_service_types(i, &service_id)) {
		i += 1;
		obs_module_t *obsModule = obs_service_get_module(service_id);
		if (!obsModule) {
			continue;
		}
		std::string moduleName = obs_get_module_file_name(obsModule);
		moduleName = moduleName.substr(0, moduleName.rfind("."));
		auto it = std::find_if(modules_.begin(), modules_.end(),
				       [moduleName](ModuleInfo const &m) { return m.module_name == moduleName; });
		if (it != modules_.end()) {
			it->servicesLoaded.push_back(service_id);
		}
	}

	for (auto &moduleInfo : modules_) {
		if (moduleInfo.enabledAtLaunch) {
			moduleInfo.sources = moduleInfo.sourcesLoaded;
			moduleInfo.encoders = moduleInfo.encodersLoaded;
			moduleInfo.outputs = moduleInfo.outputsLoaded;
			moduleInfo.services = moduleInfo.servicesLoaded;
		} else {
			for (auto const &source : moduleInfo.sources) {
				disabledSources_.push_back(source);
			}
			for (auto const &output : moduleInfo.outputs) {
				disabledOutputs_.push_back(output);
			}
			for (auto const &encoder : moduleInfo.encoders) {
				disabledEncoders_.push_back(encoder);
			}
			for (auto const &service : moduleInfo.services) {
				disabledServices_.push_back(service);
			}
		}
	}
}

void PluginManager::disableModules_()
{
	for (const auto &moduleInfo : modules_) {
		if (!moduleInfo.enabled) {
			obs_add_disabled_module(moduleInfo.module_name.c_str());
		}
	}
}

void PluginManager::open()
{
	auto main = OBSBasic::Get();
	PluginManagerWindow pluginManagerWindow(this, modules_, portalSession_, portalBaseUrl_, main);
	auto result = pluginManagerWindow.exec();
	if (result == QDialog::Accepted) {
		modules_ = pluginManagerWindow.result();
		portalSession_ = pluginManagerWindow.portalSessionResult();
		saveModules_();
		savePortalSession_();

		bool changed = false;

		for (auto const &moduleInfo : modules_) {
			if (moduleInfo.enabled != moduleInfo.enabledAtLaunch) {
				changed = true;
				break;
			}
		}

		if (changed) {
			QMessageBox::StandardButton button =
				OBSMessageBox::question(main, QTStr("Restart"), QTStr("NeedsRestart"));

			if (button == QMessageBox::Yes) {
				restart = true;
				main->close();
			}
		}
	}
}

}; // namespace OBS
