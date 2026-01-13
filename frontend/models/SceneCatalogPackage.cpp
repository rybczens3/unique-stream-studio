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

#include "SceneCatalogPackage.hpp"

#include <obs-data.h>

#include <QCryptographicHash>
#include <QFile>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace OBS {
namespace {
std::vector<int> parseVersionParts(const std::string &version)
{
	std::vector<int> parts;
	std::stringstream stream(version);
	std::string part;

	while (std::getline(stream, part, '.')) {
		int value = 0;
		bool hasDigit = false;
		for (char ch : part) {
			if (!std::isdigit(static_cast<unsigned char>(ch)))
				break;
			value = value * 10 + (ch - '0');
			hasDigit = true;
		}
		parts.push_back(hasDigit ? value : 0);
	}

	return parts;
}

bool computeSha256(const std::filesystem::path &path, std::string &output)
{
	QFile file(QString::fromStdString(path.u8string()));
	if (!file.open(QIODevice::ReadOnly))
		return false;

	QCryptographicHash hash(QCryptographicHash::Sha256);
	if (!hash.addData(&file))
		return false;

	output = hash.result().toHex().toStdString();
	return true;
}
} // namespace

int compareSemanticVersions(const std::string &lhs, const std::string &rhs)
{
	std::vector<int> left = parseVersionParts(lhs);
	std::vector<int> right = parseVersionParts(rhs);
	const size_t maxSize = std::max(left.size(), right.size());
	left.resize(maxSize, 0);
	right.resize(maxSize, 0);

	for (size_t i = 0; i < maxSize; ++i) {
		if (left[i] < right[i])
			return -1;
		if (left[i] > right[i])
			return 1;
	}

	return 0;
}

std::string SceneCatalogApiConfig::packagesUrl() const
{
	return baseUrl + packagesEndpoint;
}

std::string SceneCatalogApiConfig::packageUrl(const std::string &id) const
{
	std::string url = baseUrl + packageEndpoint;
	const std::string token = "%1";
	const auto pos = url.find(token);
	if (pos != std::string::npos)
		url.replace(pos, token.size(), id);

	return url;
}

bool SceneCatalogPackage::LoadFromJson(const std::string &json, SceneCatalogPackage &out, std::string &errorMessage)
{
	out = SceneCatalogPackage{};
	OBSDataAutoRelease data = obs_data_create_from_json(json.c_str());
	if (!data) {
		errorMessage = "Invalid scene package manifest JSON.";
		return false;
	}

	int formatVersion = static_cast<int>(obs_data_get_int(data, "format_version"));
	if (formatVersion == 0)
		formatVersion = kSupportedFormatVersion;

	if (formatVersion != kSupportedFormatVersion) {
		errorMessage = "Unsupported scene package format version.";
		return false;
	}

	out.formatVersion = formatVersion;
	out.id = obs_data_get_string(data, "id");
	out.name = obs_data_get_string(data, "name");
	out.version = obs_data_get_string(data, "version");
	out.obsMinVersion = obs_data_get_string(data, "obs_min_version");
	out.description = obs_data_get_string(data, "description");

	const char *assetToken = obs_data_get_string(data, "asset_root_token");
	if (assetToken && *assetToken)
		out.assetRootToken = assetToken;

	OBSDataAutoRelease preview = obs_data_get_obj(data, "preview");
	if (preview) {
		out.previewImageUrl = obs_data_get_string(preview, "thumbnail");
	}

	OBSDataAutoRelease collection = obs_data_get_obj(data, "collection");
	if (!collection) {
		errorMessage = "Scene package is missing a collection payload.";
		return false;
	}

	out.collectionJson = obs_data_get_json(collection);

	OBSDataArrayAutoRelease resources = obs_data_get_array(data, "resources");
	if (resources) {
		size_t count = obs_data_array_count(resources);
		out.resources.reserve(count);
		for (size_t i = 0; i < count; ++i) {
			OBSDataAutoRelease resource = obs_data_array_item(resources, i);
			SceneCatalogResource item;
			item.path = obs_data_get_string(resource, "path");
			item.sha256 = obs_data_get_string(resource, "sha256");
			item.url = obs_data_get_string(resource, "url");
			item.size = static_cast<uint64_t>(obs_data_get_int(resource, "size"));
			out.resources.push_back(std::move(item));
		}
	}

	OBSDataArrayAutoRelease addons = obs_data_get_array(data, "addons");
	if (addons) {
		size_t count = obs_data_array_count(addons);
		out.addons.reserve(count);
		for (size_t i = 0; i < count; ++i) {
			OBSDataAutoRelease addon = obs_data_array_item(addons, i);
			SceneCatalogAddon item;
			item.id = obs_data_get_string(addon, "id");
			item.type = obs_data_get_string(addon, "type");
			out.addons.push_back(std::move(item));
		}
	}

	if (out.id.empty() || out.name.empty()) {
		errorMessage = "Scene package is missing required metadata fields.";
		return false;
	}

	return true;
}

bool SceneCatalogPackage::validateCompatibility(const std::string &currentVersion, std::string &errorMessage) const
{
	if (obsMinVersion.empty())
		return true;

	if (compareSemanticVersions(currentVersion, obsMinVersion) < 0) {
		errorMessage = "Scene package requires a newer OBS version.";
		return false;
	}

	return true;
}

bool SceneCatalogPackage::validateResources(const std::filesystem::path &rootPath, std::string &errorMessage) const
{
	for (const auto &resource : resources) {
		if (resource.path.empty()) {
			errorMessage = "Scene package resource entry is missing a path.";
			return false;
		}

		std::filesystem::path fullPath = rootPath / std::filesystem::u8path(resource.path);
		if (!std::filesystem::exists(fullPath)) {
			errorMessage = "Missing resource file: " + resource.path;
			return false;
		}

		if (resource.size > 0) {
			std::error_code error;
			auto size = std::filesystem::file_size(fullPath, error);
			if (error || size != resource.size) {
				errorMessage = "Resource size mismatch: " + resource.path;
				return false;
			}
		}

		if (!resource.sha256.empty()) {
			std::string computed;
			if (!computeSha256(fullPath, computed)) {
				errorMessage = "Failed to compute resource hash: " + resource.path;
				return false;
			}

			std::string expected = resource.sha256;
			std::transform(expected.begin(), expected.end(), expected.begin(), ::tolower);
			std::transform(computed.begin(), computed.end(), computed.begin(), ::tolower);
			if (!computed.empty() && computed != expected) {
				errorMessage = "Resource hash mismatch: " + resource.path;
				return false;
			}
		}
	}

	return true;
}

} // namespace OBS
