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

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace OBS {

/*
Scene package manifest format (scene-package.json):
{
  "format_version": 1,
  "id": "com.example.streamdeck",
  "name": "Stream Deck Scene Pack",
  "version": "1.0.0",
  "obs_min_version": "31.0.0",
  "description": "Curated scenes and assets.",
  "asset_root_token": "${scene_assets}",
  "preview": { "thumbnail": "previews/cover.png" },
  "resources": [
    { "path": "images/background.png", "url": "assets/background.png", "sha256": "...", "size": 12345 }
  ],
  "addons": [
    { "id": "com.example.lowerthird", "type": "filter" }
  ],
  "collection": { ... scene collection JSON payload ... }
}

Catalog API (shared with plugin portal):
GET /portal/api/scene-catalog/packages
  -> { "packages": [ { "id", "name", "version", "type", "summary", "obs_min_version", "preview_url",
                       "manifest_url", "package_url" } ] }
*/

struct SceneCatalogResource {
	std::string path;
	std::string sha256;
	std::string url;
	uint64_t size = 0;
};

struct SceneCatalogAddon {
	std::string id;
	std::string type;
};

struct SceneCatalogPackage {
	static constexpr int kSupportedFormatVersion = 1;

	int formatVersion = kSupportedFormatVersion;
	std::string id;
	std::string name;
	std::string version;
	std::string obsMinVersion;
	std::string description;
	std::string assetRootToken = "${scene_assets}";
	std::string collectionJson;
	std::string previewImageUrl;
	std::vector<SceneCatalogResource> resources;
	std::vector<SceneCatalogAddon> addons;

	static bool LoadFromJson(const std::string &json, SceneCatalogPackage &out, std::string &errorMessage);
	bool validateCompatibility(const std::string &currentVersion, std::string &errorMessage) const;
	bool validateResources(const std::filesystem::path &rootPath, std::string &errorMessage) const;
};

struct SceneCatalogEntry {
	std::string id;
	std::string name;
	std::string version;
	std::string type;
	std::string summary;
	std::string obsMinVersion;
	std::string previewUrl;
	std::string manifestUrl;
	std::string packageUrl;
};

struct SceneCatalogApiConfig {
	std::string baseUrl = "http://localhost:8080/portal/api";
	std::string packagesEndpoint = "/scene-catalog/packages";
	std::string packageEndpoint = "/scene-catalog/packages/%1";

	std::string packagesUrl() const;
	std::string packageUrl(const std::string &id) const;
};

int compareSemanticVersions(const std::string &lhs, const std::string &rhs);

} // namespace OBS
