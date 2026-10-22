#pragma once

#include "thermal_common.hpp"
#include "thermal_solver.hpp"
#include "thermal_data.hpp"

class ThermalGui {
	
public:

	void ThermalGui::autoAdjustDisplayRange(const ThermalData& _data);

	void draw(
		uiConf_s* uc,
		unsigned int vertex_count,
		ThermalSolver& solver,
		ThermalVars_s& thermalVars,
		ThermalData& data,
		const ThermalObjects& objs
	);
	void ThermalGui::updateObjectiveFunctionIfNeeded( const ThermalSolver& solver, const ThermalVars_s& thermalVars, ThermalData& thermalData);

	// display
	int		sourceVertexId = 0;
	int		visualization = 2;
	bool	customNormalization = false;
	float	normalizationFactor = 300.0 * kelvinUnitFactor;
	float	displayRangeMin = 0.0;
	float	displayRangeMax = 1.0;
	bool	enableAutoAdjust = true;
	float	clipDistance = 0.0;
	bool	backfaceCulling = false;
	bool	wireframe = false;
	bool	showSky = false;
	int		aoSamples = 32;
	float	aoDistance = 30.0;
	float	aoBlending = 0.3;

	// misc
	bool	showProperties = false;
	bool	showStatistics = false;
	bool    saveScreenshotForIteration = false;
	int     screenshotCounter = 0;
	int     selectOptimizer = 0;

	std::chrono::duration<double> totalTime;
	std::chrono::duration<double> stepSum;
	std::chrono::duration<double> transportSum;
	long computedStepCount = 0;

	// avg kelvin
	int plotObjComboIndex = 0;
	std::string plotObjComboString;

	float avgKelvin = 0.0;
	float avgKelvinMin = std::numeric_limits<float>::max();
	float avgKelvinMax = 0.0;
	std::vector<float> avgKelvins;

	void resetKelvinPlot()
	{
		avgKelvin = 0.0;
		avgKelvinMin = std::numeric_limits<float>::max();
		avgKelvinMax = 0.0;
		avgKelvins.clear();
	}

	// optimization values

	float objectiveValue = 0.0;
	float objectiveValueMin = std::numeric_limits<float>::max();
	float objectiveValueMax = 0.0;
	std::vector<float> objectiveValues;

	float gradientNorm = 0.0;
	float gradientNormMin = std::numeric_limits<float>::max();
	float gradientNormMax = 0.0;
	std::vector<float> gradientNorms;

	void reset()
	{
		objectiveValue = 0.0;
		objectiveValueMin = std::numeric_limits<float>::max();
		objectiveValueMax = 0.0;
		objectiveValues.clear();

		gradientNorm = 0.0;
		gradientNormMin = std::numeric_limits<float>::max();
		gradientNormMax = 0.0;
		gradientNorms.clear();
	}

private:
	// ...

};