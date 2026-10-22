#include "thermal_common.hpp"

#include <Eigen/Core>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>

#include <tamashii/engine/common/common.hpp>

using namespace Eigen;

SCALAR sky_value_to_kelvin(SCALAR _value, SCALAR _min) // value already in W / m^2 
{
	// area not needed as sky values are already in W/m2
	SCALAR kelvin = pow(_value / STEFAN_BOLTZMANN_CONST_RAW, 1.0 / 4.0);
	kelvin = std::max(kelvin, _min); // clamp
	return kelvin * kelvinUnitFactor;
}

// from https://stackoverflow.com/questions/33575478/how-can-you-find-the-condition-number-in-eigen
SCALAR condistion_number(Mat _mat)
{
	JacobiSVD<Mat> svd(_mat);
	Vec sigma = svd.singularValues();
	return sigma.maxCoeff() / sigma.minCoeff();
}

void printVramSize(rvk::LogicalDevice* _device)
{
	VkPhysicalDeviceMemoryBudgetPropertiesEXT props = _device->getMemoryBudgetProperties();
	for(int i=0;i<16; i++)
		spdlog::info("heap #{}, budget: {}, usage: {}", i, props.heapBudget[i], props.heapUsage[i]);
}

bool createDirectoryIfNotExists(const std::filesystem::path& path) {
	if (!std::filesystem::exists(path)) {
		try {
			std::filesystem::create_directory(path);
			return true; // Directory created successfully
		}
		catch (const std::filesystem::filesystem_error& e) {
			// Handle potential errors, e.g., permission denied
			std::cerr << "Error creating directory: " << e.what() << std::endl;
			return false; // Directory creation failed
		}
	}
	else {
		// Directory already exists
		return true;
	}
}

std::string get_scene_path_wo_ext(std::string _subfolder)
{
	tamashii::RenderScene* _scene = tamashii::Common::getInstance().getRenderSystem()->getMainScene();
	std::filesystem::path sceneFile(_scene->getSceneFileName());
	std::string fnameNoExt = sceneFile.filename().string();
	if (fnameNoExt.find('.') != fnameNoExt.npos) fnameNoExt = fnameNoExt.substr(0, fnameNoExt.find_first_of('.'));
	if (_subfolder.empty())
	{
		return sceneFile.parent_path().string() + std::string(1, sceneFile.preferred_separator) + fnameNoExt;
	}
	else
	{
		std::string folder = sceneFile.parent_path().string() + std::string(1, sceneFile.preferred_separator) + _subfolder + std::string(1, sceneFile.preferred_separator);
		createDirectoryIfNotExists(folder);
		return folder + fnameNoExt;
	}
}

std::tuple<std::string, tm*> hoy_to_date_time(float _hour)
{
	// Create a tm structure for the beginning of the year
	tm start_of_year = { 0 };
	start_of_year.tm_year = 124;
	start_of_year.tm_mon = 0;
	start_of_year.tm_mday = 0;
	start_of_year.tm_hour = 0;
	start_of_year.tm_min = 0;
	start_of_year.tm_sec = 0;

	// Add the specified number of hours to the start of the year
	time_t timestamp = mktime(&start_of_year);
	timestamp += _hour * 3600; // 3600 seconds in an hour

	// Convert the timestamp to a tm structure
	tm* result_time = localtime(&timestamp);

	// Format the result as a datetime string
	std::string date_string = std::to_string(result_time->tm_year + 1900) + "-" + std::to_string(result_time->tm_mon + 1) + "-" + std::to_string(result_time->tm_mday);
	return std::tuple<std::string, tm*> {date_string, result_time};
}