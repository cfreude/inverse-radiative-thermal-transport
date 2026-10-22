#pragma once

#include <Eigen/Eigen>
#include <string>
#include <sstream>

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#include "rvk/rvk.hpp"

#include <glm/glm.hpp>
#include <filesystem>

using namespace Eigen;

#define CHECK_EMPTY_SCENE(SCENE) if (SCENE.refModels.size() == 0) { spdlog::warn("Scene has no models."); return; }

/**/ // float --- make sure to sync with raytracing_thermal/defines.h
typedef float SCALAR;
typedef glm::vec3 glVec3;
typedef glm::mat3 glMat3;
typedef glm::vec4 glVec4;
/*/ // double
typedef double SCALAR;
typedef glm::dvec3 glVec3;
typedef glm::dmat3 glMat3;
typedef glm::dvec4 glVec4;
/**/

typedef Matrix<SCALAR, Eigen::Dynamic, 1> Vec;
typedef Matrix<SCALAR, 3, 1> Vec3;
typedef Matrix<SCALAR, 3, 3> Mat3;
typedef Matrix<SCALAR, Eigen::Dynamic, Eigen::Dynamic, RowMajor> Mat;
typedef Eigen::SparseMatrix<SCALAR> SpMat;

const SCALAR kelvinUnitFactor =  1.0; // K ... 1e-3; // kilo kelvin ... 
const SCALAR secondsUnitFactor = 1.0; // s ... 1.0 / 3600.0; // hours ... 
const SCALAR STEFAN_BOLTZMANN_CONST_RAW = 5.670374419e-8;
const SCALAR STEFAN_BOLTZMANN_CONST = STEFAN_BOLTZMANN_CONST_RAW / (pow(kelvinUnitFactor, 4.0) * pow(secondsUnitFactor, 3.0)); // W m ^ -2 K ^ -4 

constexpr char THERMAL_RENDERER_NAME[] = "Thermal Renderer";

SCALAR sky_value_to_kelvin(SCALAR _value, SCALAR _min);
SCALAR condistion_number(Mat _mat);
void printVramSize(rvk::LogicalDevice* _device);
bool _createDirectoryIfNotExists(const std::filesystem::path& path);
std::string get_scene_path_wo_ext(std::string _subfolder = "");
std::tuple<std::string, tm*> hoy_to_date_time(float _hour);

template <typename T>
void logEigenBase(const std::string tag, const DenseBase<T>&v)
{
#ifndef RUNTIME_OPTIMIZED
	// Materialize expression templates (e.g. .transpose(), a/b) into a concrete
	// matrix and turn it into a plain string before passing to spdlog. fmt v12
	// cannot format Eigen proxy types (DenseBase expressions, WithFormat, ...).
	const auto m = v.eval();
	if (m.cols() < 40)
	{
		std::ostringstream oss;
		oss << m.format(Eigen::IOFormat(Eigen::FullPrecision, 0, ", ", "\n", "[", "]"));
		spdlog::debug("{}:\n{}", tag, oss.str());
	}
	else
	{
		spdlog::debug("{}:", tag);
		spdlog::debug("\tmin: {}", m.minCoeff());
		spdlog::debug("\tmean: {}", m.mean());
		spdlog::debug("\tmax : {}", m.maxCoeff());
	}
#endif // !RUNTIME_OPTIMIZED
}

// fmt v12 cannot format Eigen expression templates (Transpose, WithFormat, ...).
// Stream them through Eigen's operator<< into a plain string first.
template <typename T>
std::string eigenToStr(const T& aValue)
{
	std::ostringstream oss;
	oss << aValue;
	return oss.str();
}

typedef struct ThermalVars_s {
	// simulation
	bool reset = false;
	bool recomputeTransport = false;
	int	timeSteps = 180;
	int remainingTimeSteps = 0;
	float simulationTime = 0.0f;
	int rayDepth = 0;
	int threadsPerTriangle = 32768;
	int raysPerThread = 8;
	bool steadyStateInitialCondition = false;
	float tinyEps = 1e-2;
	float tinyDist = 1e-3;

	bool exportEnabled = false;
	int exportIterationCount = 0;
	int outputFileCount = 0;

	bool resetGeometryFromParams = false;

	bool updateSky = false;
	bool interpolate_sky = true;

	bool compressTransport = false;
	bool applyCollumnSumCorrection = false;
	int compressionMaxIters = 50;
	int compressionAlgorithm = 1;

	bool runOptim=false;
	bool useFDforOptim=false;
	float optimStep = 0.001;
	int maxSteps = 1;

	bool runSingleFwd=false;
	bool runSingleAdj=false;
	bool runSingleFD=false;
	float fdStep = 0.01;
	int  fdMode = 0; // select in GUI "Standard FD\0Range FD\0T-op only FD\0"
	int adjointBatchCount = 1;
	float gradEdgeRatio = 0.05;

	bool constRandSeed = true;
	int randSeed = 1234;

	int visualizeGradient = -1; // set to parameter ID to display selected gradient

	// special functions
	bool doPrepT2 = false;
} ThermalVars_s;

typedef struct ObjectStatistics_s {
	std::string											name;
	SCALAR												min;
	SCALAR												max;
	SCALAR												avg;
} ObjectStatistics_s;