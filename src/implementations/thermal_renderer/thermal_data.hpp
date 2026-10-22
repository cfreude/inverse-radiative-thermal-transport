#pragma once

#include <tamashii/engine/scene/render_scene.hpp>
#include <tamashii/engine/render/render_backend_implementation.hpp>

#include "thermal_common.hpp"
#include "thermal_objects.hpp"

T_USE_NAMESPACE

class ObjectiveFunction;
class Parametrization;
class Constraints;
class OpenLoopOptimizer;

class ThermalData {

public:
	struct SkyData
	{
		Mat matrix;
		std::vector<int> hours;
		std::vector<uint32_t> indices;
		bool averaged = true;
		float current_hour = 0.0;

		static constexpr size_t VALUE_COUNT = 145;

		static constexpr float MINIMAL_KELVIN = 111.575; // manual compensation for zero values during night sky

		void init(
			const std::vector<float>& _values,
			const std::vector<unsigned int>& _hours,
			const int _vertex_offset,
			const std::vector<uint32_t>& _indices
			)
		{
			size_t sky_value_count = _values.size();
			size_t sky_hours_count = _hours.size();

			assert((sky_value_count / VALUE_COUNT) == sky_hours_count);

			hours.assign(_hours.begin(), _hours.end());

			matrix = Mat::Zero(sky_hours_count, VALUE_COUNT);
			size_t svi_offset = 0;
			for (int hi = 0; hi < sky_hours_count; hi++)
			{
				float sum = 0.0;
				for (int svi = 0; svi < VALUE_COUNT; svi++)
				{
					float sky_value = _values[svi_offset++];
					sum += sky_value;
					matrix(hi, svi) = sky_value;
				}
				spdlog::info("Sky Hour {} sum = {}", hours[hi], sum);
			}

			int indices_count = VALUE_COUNT * 6;
			assert(_indices.size() == indices_count);

			indices.clear();
			indices.reserve(indices_count);
			for(int i=0; i< indices_count; i++)
			{
				indices.push_back(_vertex_offset + _indices[i]);
			}
		}

		std::tuple<int, float> hourToIndexAndFraction(float _hour) {

			int hour_count = hours.size();
			float hours_mod = fmod(_hour, (float)hour_count);
			float hour;
			float hour_frac = modff(hours_mod, &hour);
			return std::tuple<int, float> {(int)hour, hour_frac};
		}

		float setVector(
			const Vec _values,
			Vec& _initial_value_vector,
			float _minimum_kelvin = 0.0,
			bool _values_in_kelvin = false)
		{
			SCALAR avg_sky_value = 0.0;
			unsigned int vertex_index = 0;

			for (float sky_value : _values) // sky value already converted from kWh/m2 to W / m^2
			{
				avg_sky_value += sky_value;

				for (int i = 0; i < 6; i++)
				{		
					if(_values_in_kelvin)
						_initial_value_vector(indices[vertex_index + i]) = sky_value;
					else
						_initial_value_vector(indices[vertex_index + i]) = sky_value_to_kelvin(sky_value, _minimum_kelvin);
				}
				vertex_index += 6;
			}

			spdlog::info("Total sky value: {}, max kelvin: {}", avg_sky_value, _initial_value_vector.maxCoeff());

			avg_sky_value /= ((float)VALUE_COUNT);
			return avg_sky_value;
		}

		/*
		int setHourByIndex(
			const int _index,
			Vec& _initial_value_vector,
			float _minimum_kelvin = 0.0
			)
		{
			Vec sky_vals;
			if (averaged)
				sky_vals = matrix.colwise().mean();
			else
				sky_vals = matrix.row(_index);

			SCALAR avg_sky_value = setVector(sky_vals, _initial_value_vector, _minimum_kelvin);

			return hours[_index];
		}
		*/

		float setHourByTime(
			const float _hour,
			Vec& _initial_value_vector,
			float _minimum_kelvin = 0.0
		)
		{
			int hour;
			float hour_frac;
			std::tie(hour, hour_frac) = hourToIndexAndFraction(_hour);

			Vec sky_vals = Vec::Zero(matrix.cols());
			if (averaged)
			{
				if (0) // clip kelvin
				{
					for (const auto row : matrix.rowwise())
					{
						for (int i = 0; i < matrix.cols(); i++)
						{
							sky_vals(i) += sky_value_to_kelvin(row(i), _minimum_kelvin);
						}
					}

					sky_vals /= matrix.rows();
					SCALAR avg_sky_value = setVector(sky_vals, _initial_value_vector, _minimum_kelvin, true);
				}
				else // clip W/m2
				{
					sky_vals = matrix.cwiseMax(STEFAN_BOLTZMANN_CONST_RAW * pow(_minimum_kelvin, 4.0)).colwise().mean();
					SCALAR avg_sky_value = setVector(sky_vals, _initial_value_vector, _minimum_kelvin, false);
				}
			}
			else 			
			{
				int hour_count = hours.size();

				spdlog::info("Setting Sky hour - time (h): {}, sky hour: {}, fraction: {}", _hour, hour, hour_frac);

				int hind_a = (int)hour;
				int hind_b = (hind_a + 1) % hour_count;
				const Vec& a = matrix.row(hind_a);
				const Vec& b = matrix.row(hind_b);
				//hour_frac = 0.0;
				sky_vals = a * (1 - hour_frac) + b * hour_frac;
				SCALAR avg_sky_value = setVector(sky_vals, _initial_value_vector, _minimum_kelvin);
			}
			return hours[(int)hour] + hour_frac;
		}

	};

	void init(scene_s _scene, const ThermalObjects& _thermal_objects, unsigned int _vertex_count, unsigned int _triangle_count);
	void load(scene_s _scene, ThermalObjects& _thermal_objects, SCALAR _sky_minimum_kelvin = 0.0, unsigned int _timeSteps=1);
	void computeLumpedThermalMass(scene_s _scene, ThermalObjects& _thermal_objects);
	void computeConductiveLaplacian(scene_s _scene, ThermalObjects& _thermal_objects);
	void reset();
	void unload();

	float setSkyHour(float _hour);

	void recomputeAreaVectors(const scene_s& _scene);
	const std::tuple<SCALAR, SCALAR> ThermalData::getMinMaxOfNonFixedValues() const;
	void setObjectStatistics(const ThermalObjects& objects, const Vec& _values);

	EIGEN_MAKE_ALIGNED_OPERATOR_NEW

	Vec	initialValueVector;
	Vec	currentValueVector;
	Vec	emissionVector;
	Vec	absorptionVector;

	Vec vertexAreaVector;
	VectorXi vertexTriangleCountVector;
	Vec triangleAreaVector;
	Vec fixedVarsVector;

	Vec lumpedMassMatrix; // use as diagonal matrix
	SpMat conductiveLaplacian; //cotan-Laplacian weighted by thermal conductivity
	Mat	objectiveGradX; // gradient of optimization objective wrt. vertex positions
	std::shared_ptr<ObjectiveFunction> objFcn;
	std::shared_ptr<Parametrization> params;
	std::shared_ptr<Constraints> constraints;
	std::shared_ptr<OpenLoopOptimizer> optimizer;

	SkyData sky;

	std::vector<ObjectStatistics_s> objectStatistics;

private:
	// ...
	void updateAreaVectorsForMesh(RefModel_s* refModel, Mesh* m, unsigned int vertex_offset, unsigned int& triangle_index_offset);

};