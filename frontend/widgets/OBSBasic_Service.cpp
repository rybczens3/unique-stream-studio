/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>
                          Zachary Lund <admin@computerquip.com>
                          Philippe Groarke <philippe.groarke@gmail.com>

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

#include "OBSBasic.hpp"

constexpr std::string_view OBSServiceFileName = "service.json";
constexpr std::string_view OBSMultiServiceType = "rtmp_multi";
constexpr std::string_view OBSMultiServiceTargetsKey = "targets";

static void AppendServiceTarget(OBSDataArray &targets, obs_service_t *target)
{
	if (!target)
		return;

	OBSDataAutoRelease targetData = obs_data_create();
	OBSDataAutoRelease targetSettings = obs_service_get_settings(target);

	obs_data_set_string(targetData, "type", obs_service_get_type(target));
	obs_data_set_obj(targetData, "settings", targetSettings);
	obs_data_array_push_back(targets, targetData);
}

void OBSBasic::SaveService()
{
	if (!service && streamServices.empty())
		return;

	const OBSProfile &currentProfile = GetCurrentProfile();

	const std::filesystem::path jsonFilePath = currentProfile.path / std::filesystem::u8path(OBSServiceFileName);

	OBSDataAutoRelease data = obs_data_create();
	if (!streamServices.empty()) {
		if (streamServices.size() == 1) {
			OBSDataAutoRelease settings = obs_service_get_settings(streamServices.front());
			obs_data_set_string(data, "type", obs_service_get_type(streamServices.front()));
			obs_data_set_obj(data, "settings", settings);
		} else {
			OBSDataAutoRelease settings = obs_data_create();
			OBSDataArrayAutoRelease targets = obs_data_array_create();

			for (const auto &target : streamServices) {
				AppendServiceTarget(targets, target);
			}

			obs_data_set_array(settings, OBSMultiServiceTargetsKey.data(), targets);
			obs_data_set_string(data, "type", OBSMultiServiceType.data());
			obs_data_set_obj(data, "settings", settings);
		}
	} else {
		OBSDataAutoRelease settings = obs_service_get_settings(service);
		obs_data_set_string(data, "type", obs_service_get_type(service));
		obs_data_set_obj(data, "settings", settings);
	}

	if (!obs_data_save_json_safe(data, jsonFilePath.u8string().c_str(), "tmp", "bak")) {
		blog(LOG_WARNING, "Failed to save service");
	}
}

bool OBSBasic::LoadService()
{
	OBSDataAutoRelease data;

	try {
		const OBSProfile &currentProfile = GetCurrentProfile();

		const std::filesystem::path jsonFilePath =
			currentProfile.path / std::filesystem::u8path(OBSServiceFileName);

		data = obs_data_create_from_json_file_safe(jsonFilePath.u8string().c_str(), "bak");

		if (!data) {
			return false;
		}
	} catch (const std::invalid_argument &error) {
		blog(LOG_ERROR, "%s", error.what());
		return false;
	}

	const char *type;
	obs_data_set_default_string(data, "type", "rtmp_common");
	type = obs_data_get_string(data, "type");

	streamServices.clear();

	if (strcmp(type, OBSMultiServiceType.data()) == 0) {
		OBSDataAutoRelease settings = obs_data_get_obj(data, "settings");
		OBSDataArrayAutoRelease targets = obs_data_get_array(settings, OBSMultiServiceTargetsKey.data());

		size_t targetCount = obs_data_array_count(targets);
		for (size_t i = 0; i < targetCount; ++i) {
			OBSDataAutoRelease targetData = obs_data_array_item(targets, i);
			const char *targetType = obs_data_get_string(targetData, "type");
			OBSDataAutoRelease targetSettings = obs_data_get_obj(targetData, "settings");

			if (!targetType || !*targetType)
				targetType = "rtmp_common";

			obs_service_t *targetService =
				obs_service_create(targetType, "stream_target_service", targetSettings, nullptr);
			if (!targetService)
				continue;

			streamServices.emplace_back(targetService);
			obs_service_release(targetService);
		}
	} else {
		OBSDataAutoRelease settings = obs_data_get_obj(data, "settings");
		OBSDataAutoRelease hotkey_data = obs_data_get_obj(data, "hotkeys");

		service = obs_service_create(type, "default_service", settings, hotkey_data);
		obs_service_release(service);

		if (service)
			streamServices.emplace_back(service);
	}

	if (streamServices.empty())
		return false;

	service = streamServices.front();

	/* Enforce Opus on WHIP if needed */
	if (strcmp(obs_service_get_protocol(service), "WHIP") == 0) {
		const char *option = config_get_string(activeConfiguration, "SimpleOutput", "StreamAudioEncoder");
		if (strcmp(option, "opus") != 0)
			config_set_string(activeConfiguration, "SimpleOutput", "StreamAudioEncoder", "opus");

		option = config_get_string(activeConfiguration, "AdvOut", "AudioEncoder");

		const char *encoder_codec = obs_get_encoder_codec(option);
		if (!encoder_codec || strcmp(encoder_codec, "opus") != 0)
			config_set_string(activeConfiguration, "AdvOut", "AudioEncoder", "ffmpeg_opus");
	}

	return true;
}

bool OBSBasic::InitService()
{
	ProfileScope("OBSBasic::InitService");

	if (LoadService())
		return true;

	service = obs_service_create("rtmp_common", "default_service", nullptr, nullptr);
	if (!service)
		return false;
	obs_service_release(service);
	streamServices.clear();
	streamServices.emplace_back(service);

	return true;
}

obs_service_t *OBSBasic::GetService()
{
	if (!service) {
		service = obs_service_create("rtmp_common", NULL, NULL, nullptr);
		obs_service_release(service);
		streamServices.clear();
		streamServices.emplace_back(service);
	}
	return service;
}

const std::vector<OBSService> &OBSBasic::GetServices() const
{
	return streamServices;
}

void OBSBasic::SetService(obs_service_t *newService)
{
	if (newService) {
		service = newService;
		streamServices.clear();
		streamServices.emplace_back(service);
	}
}

void OBSBasic::SetServices(const std::vector<OBSService> &services)
{
	streamServices = services;
	if (!streamServices.empty()) {
		service = streamServices.front();
	}
}
