#include "thermal_gui.hpp"

#include <math.h>
#include <imgui.h>

#include <tamashii/engine/common/common.hpp>

#include "thermal_export.hpp"
#include "thermal_objective.hpp"
#include "thermal_params.hpp"

void ThermalGui::autoAdjustDisplayRange(const ThermalData& _data)
{
	if (enableAutoAdjust)
	{
		std::tie(displayRangeMin, displayRangeMax) = _data.getMinMaxOfNonFixedValues();
	}
}

void ThermalGui::draw(
	uiConf_s* uc,
	unsigned int vertex_count,
	ThermalSolver& solver,
	ThermalVars_s& thermalVars,
	ThermalData& data,
	const ThermalObjects& objs)
{ 

#ifdef DISABLE_GUI
	assert(false);// , "drawUI called!");
#endif // DISABLE_GUI

	if (ImGui::Begin("Settings", NULL, 0))
	{
		ImGui::Separator();
		if (ImGui::Button("PRESS TO UPDATE SIMULATION"))
		{
			enableAutoAdjust = true;
			thermalVars.reset = true;
			thermalVars.recomputeTransport = true;
			thermalVars.remainingTimeSteps = (solver.compute_steady_state) ? 1 : std::max<int>(1, thermalVars.timeSteps);
			solver.clearRuntimesforNextRun(thermalVars.compressTransport);
		}
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Display"))
		{
			ImGui::Combo("Mode##vis", &visualization, "Emission to all\0Absorption from all\0Value\0");
			if (visualization != 2)
			{
				if (ImGui::InputInt("Source Vertex Index", &sourceVertexId, 1, 1))
					sourceVertexId = std::clamp<int>(sourceVertexId, 0, vertex_count - 1);
				ImGui::Text("Kelvin: %.2f", data.currentValueVector[sourceVertexId] / kelvinUnitFactor);
			}
			if (ImGui::InputFloat("Range Min", &displayRangeMin, 0.01, 1.0, "%6g")) {
				displayRangeMin = displayRangeMin; //std::max<SCALAR>(0.0, displayRangeMin); // disabled clamping so that visualizing gradients and other data can also work with negative values
			}
			if (ImGui::InputFloat("Range Max", &displayRangeMax, 0.01, 1.0, "%.6g")) {
				displayRangeMax = displayRangeMax; //std::max<SCALAR>(0.0, displayRangeMax);
			}
			if (ImGui::Button("Adjust to Data")) {
				bool tmp = enableAutoAdjust;
				enableAutoAdjust = true;
				autoAdjustDisplayRange(data);
				enableAutoAdjust = tmp;
			}
			ImGui::SameLine();
			ImGui::Checkbox("Enable Auto Adjust", &enableAutoAdjust);
		}
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Simulation"))
		{
			//ImGui::Combo("Mode##sim", &solver.mode, "Kelvin\0Radiance\0");
			if (solver.mode == 0)
			{
				if (ImGui::Checkbox("Compute Steady State", &solver.compute_steady_state)) updateObjectiveFunctionIfNeeded(solver, thermalVars, data);
				if (solver.compute_steady_state)
				{
					float tmp = (float) solver.step_size;
					if (ImGui::InputFloat("1 / Regularizer", &tmp, 1.0, 0.01, "%.3f"))
						solver.step_size = std::max<SCALAR>(0.0, (SCALAR) tmp);
				}
				else
				{
					float tmp = (float) solver.step_size;
					if (ImGui::InputFloat("Time Step", &tmp, 1.0, 0.01, "%.3f"))
						solver.step_size = std::max<SCALAR>(0.0, (SCALAR) tmp);
					if (ImGui::InputInt("Time Step Count", &thermalVars.timeSteps, 1, 10)) {
						thermalVars.timeSteps = std::max<int>(1, thermalVars.timeSteps);
						updateObjectiveFunctionIfNeeded(solver, thermalVars, data);
					}
					if (ImGui::Checkbox("Steady-State Init. Cond.", &thermalVars.steadyStateInitialCondition));
				}
				if (ImGui::InputInt("Max. Iters", &solver.max_iter, 1, 1000)) solver.max_iter = std::max<int>(1, solver.max_iter);
			}
			if (ImGui::Button("Run Solver"))
			{
				spdlog::info("Running...");
				thermalVars.remainingTimeSteps = thermalVars.timeSteps;
				thermalVars.outputFileCount = 0;
				solver.clearRuntimesforNextRun(thermalVars.compressTransport);
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset##solver")) {
				thermalVars.reset = true;
			}
			if (!solver.compute_steady_state) {
				ImGui::SameLine();
				ImGui::Text("Time: %.2f sec.", thermalVars.simulationTime / secondsUnitFactor);
			}
		}
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Transport"))
		{
			if( ImGui::DragFloat("Diff. edge ratio", &thermalVars.gradEdgeRatio, 0.01, -0.1, 0.9, "%.4g" )){
				thermalVars.gradEdgeRatio = std::max<float>(thermalVars.gradEdgeRatio, -0.1); // allow some negative values to disable edge samples
			}

			ImGui::Checkbox("Compress Transport Matrix", &thermalVars.compressTransport);
			if (thermalVars.compressTransport)
			{
				ImGui::Combo("Algorithm##vis", &thermalVars.compressionAlgorithm, "Simple ACA\0Complete ACA\0ACA+\0");
				if (ImGui::InputInt("ACA Iters", &thermalVars.compressionMaxIters, 0, 1000)) thermalVars.compressionMaxIters = std::max<int>(0, thermalVars.compressionMaxIters);
				ImGui::Checkbox("Apply Col. Sum Correction", &thermalVars.applyCollumnSumCorrection);
			}
			if (ImGui::InputInt("Ray Depth", &thermalVars.rayDepth, 1, 1)) {
				thermalVars.rayDepth = std::clamp<int>(thermalVars.rayDepth, 0, thermalVars.rayDepth);
			}
			if (ImGui::InputInt("Thread Per Triangle", &thermalVars.threadsPerTriangle, 1, 1)) {
				thermalVars.threadsPerTriangle = std::max<int>(thermalVars.threadsPerTriangle, 1);
			}
			if (ImGui::InputInt("Rays Per Thread", &thermalVars.raysPerThread, 1, 1)) {
				thermalVars.raysPerThread = std::max<int>(thermalVars.raysPerThread, 1);
			}
			if (ImGui::InputFloat("Tiny Eps.", &thermalVars.tinyEps, 0.01, 0.1)) {
				thermalVars.tinyEps = std::max<float>(thermalVars.tinyEps, 0.0);
			}
			if (ImGui::InputFloat("Tiny Dist.", &thermalVars.tinyDist, 0.01, 0.1)) {
				thermalVars.tinyDist = std::max<float>(thermalVars.tinyDist, 0.0);
			}
			if (ImGui::Button("Recompute Transport")) {
				thermalVars.recomputeTransport = true;
			}
			if (ImGui::Button("PRINT ACA COMP."))
			{
				solver.printACAComparison();
			}
		}

		// SKY DOME
		if (data.sky.hours.size() > 1)
		{
			ImGui::Separator();
			if (ImGui::CollapsingHeader("Sky Dome"))
			{
				ImGui::Checkbox("Show Sky", &showSky);
				if (data.sky.averaged)
				{
					ImGui::Text("HOY: Averaged");
				}
				else
				{
					int hour;
					float hour_frac;
					std::tie(hour, hour_frac) = data.sky.hourToIndexAndFraction(data.sky.current_hour);
					float hoy = data.sky.hours[hour] + hour_frac;

					std::string start_date;
					tm* start_time;
					std::tie(start_date, start_time) = hoy_to_date_time(hoy);
					ImGui::Text("Start - Day: %s, Time:%d:%d (HOY: %.1f)", start_date.c_str(), start_time->tm_hour, start_time->tm_min, hoy);

					float cur_hoy = hoy + thermalVars.simulationTime / 3600.0;
					std::string current_date;
					tm* current_time;
					std::tie(current_date, current_time) = hoy_to_date_time(cur_hoy);
					ImGui::Text("Curr. - Day: %s, Time:%d:%d (HOY: %.1f)", current_date.c_str(), current_time->tm_hour, current_time->tm_min, cur_hoy);
				}

				if (ImGui::Checkbox("Averaged", &data.sky.averaged))
					thermalVars.updateSky = true;

				if (!data.sky.averaged)
				{
					ImGui::SameLine();
					if (ImGui::Checkbox("Interpolated", &thermalVars.interpolate_sky))
						thermalVars.updateSky = true;

					if (ImGui::SliderFloat("Hour", &data.sky.current_hour, 0, data.sky.hours.size()))
					{
						if (!thermalVars.interpolate_sky)
							data.sky.current_hour = floor(data.sky.current_hour);
						thermalVars.updateSky = true;
					}
				}
			}
		}
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Misc"))
		{
			ImGui::Checkbox("Wireframe", &wireframe);
			ImGui::SliderFloat("Clip Distance", &clipDistance, 0.0, 100.0);
			ImGui::Checkbox("Backface Distance", &backfaceCulling);
			ImGui::SliderInt("AO Samples", &aoSamples, 0, 64);
			ImGui::SliderFloat("AO Distance", &aoDistance, 0.0, 100.0);
			ImGui::SliderFloat("AO Blending", &aoBlending, 0.0, 1.0);
		}
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Data"))
		{
			ImGui::Text("Time - Total: %.2f, N: %d", totalTime, computedStepCount);
			ImGui::SameLine();
			if (ImGui::Button("Reset##Timings")) {
				totalTime *= 0.0;
				computedStepCount = 0;
				stepSum	*= 0.0;
				transportSum *= 0.0;
			};
			ImGui::Text("A Avg. Step: %.2f, Avg. Trans.: %.2f",
				stepSum / (computedStepCount > 0 ? float(computedStepCount) : 1.0),
				transportSum / (computedStepCount > 0 ? float(computedStepCount) : 1.0));
			ImGui::Checkbox("Properties:", &showProperties);
			if (showProperties)
				for (int i = 0; i < objs.count; i++)
					ImGui::Text(objs.getUiStr(i));
			ImGui::Separator();
			ImGui::Checkbox("Statistics:", &showStatistics);
			if (showStatistics)
			{
				ImGui::Combo("Object##plot", &plotObjComboIndex, plotObjComboString.c_str());
				ImGui::PlotLines("Avg. Kelvin", avgKelvins.data(), avgKelvins.size(), 0, 0, avgKelvinMin, avgKelvinMax, ImVec2(200, 75), 4);
				if (ImGui::Button("Reset Plot##KelvinVals")) resetKelvinPlot();
				for (const ObjectStatistics_s& v : data.objectStatistics)
					ImGui::Text("%s | min: %.3f, avg: %.3f, max: %.3f", v.name.c_str(), v.min / kelvinUnitFactor, v.avg / kelvinUnitFactor, v.max / kelvinUnitFactor);
			}
		}
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Export"))
		{
			ImGui::Checkbox("Enabled", &thermalVars.exportEnabled);
			ImGui::SameLine();
			if (ImGui::Button("CSV"))
				export_to_csv_point_values(data.currentValueVector);
			ImGui::SameLine();
			if (ImGui::Button("VTK"))
			{
				int ring_ind = -1;
				for (int i = 0; i < objs.count; i++)
					if (objs.name[i].compare("Ring") == 0)
						ring_ind = i;

				//std::string path = "./scene.vtk";
				RenderScene* _scene = Common::getInstance().getRenderSystem()->getMainScene();
				std::filesystem::path sceneFile(_scene->getSceneFileName());
				std::string fnameNoExt = sceneFile.filename().string();
				if (fnameNoExt.find('.') != fnameNoExt.npos) fnameNoExt = fnameNoExt.substr(0, fnameNoExt.find_first_of('.'));
				std::string path = sceneFile.parent_path().string() + std::string(1, sceneFile.preferred_separator) + fnameNoExt + ".vtk";
				if (ring_ind > -1)
				{
					float dr = objs.diffuseReflectance[ring_ind];
					float sr = objs.specularReflectance[ring_ind];

					std::ostringstream ss;
					ss << "e" << std::setw(4) << std::setfill('0') << std::setprecision(2) << std::fixed << 1 - (dr + sr) << "-d" << dr << "-s" << sr << "-b" << thermalVars.rayDepth << ".vtk";
					path = ss.str();
				}
				export_vtk(path, data.currentValueVector);
			}
			ImGui::SameLine();
			if (ImGui::Button("GLTF"))
			{	
				string scene_file_path = get_scene_path_wo_ext();
				objs.setVertexColors(data.currentValueVector);
				Common::getInstance().exportScene(scene_file_path + "-state.gltf", (uint32_t)0x1);
			}
			ImGui::SameLine();
			if (ImGui::Button("Obj. Plot"))
			{
				string scene_file_path = get_scene_path_wo_ext();
				export_to_csv_float_vec(scene_file_path + "-obj-vals.csv", objectiveValues);
				export_to_csv_float_vec(scene_file_path + "-kelvin-vals.csv", avgKelvins);

				objs.setVertexColors(data.currentValueVector);
				Common::getInstance().exportScene(scene_file_path + "-final-opt-state.gltf", (uint32_t)0x1);

				Common::getInstance().takeScreenshot(scene_file_path + "-screenshot-ui.png", false);
				Common::getInstance().takeScreenshot(scene_file_path + "-screenshot-noui.png", true);
			}

			ImGui::SliderInt("Export Count", &thermalVars.exportIterationCount, 0, 1000);
			
			ImGui::Checkbox("Iter. Screenshots", &saveScreenshotForIteration);
			ImGui::SameLine();
			if (ImGui::Button("Screenshot"))
			{
				int ring_ind = -1;
				for (int i = 0; i < objs.count; i++)
					if (objs.name[i].compare("Ring") == 0)
						ring_ind = i;

				std::string scene_file_path = get_scene_path_wo_ext();
				if (ring_ind > -1)
				{
					float dr = objs.diffuseReflectance[ring_ind];
					float sr = objs.specularReflectance[ring_ind];

					std::ostringstream ss;
					ss << scene_file_path << "-e" << std::setw(4) << std::setfill('0') << std::setprecision(2) << std::fixed << 1 - (dr + sr) << "-d" << dr << "-s" << sr << "-b" << thermalVars.rayDepth << ".png";
					scene_file_path = ss.str();
				}
				else
				{
					scene_file_path += "-screenshot.png";
				}
				Common::getInstance().takeScreenshot(scene_file_path, false);
			}
		}
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Optimization Plot"))
		{ 
			ImGui::PlotLines("Obj. Value", objectiveValues.data(), objectiveValues.size(), 0, 0, objectiveValueMin, objectiveValueMax, ImVec2(200,75), 4);
			ImGui::PlotLines("Grad. Norm", gradientNorms.data(), gradientNorms.size(), 0, 0, gradientNormMin, gradientNormMax, ImVec2(200, 20), 4);
			if (ImGui::Button("Reset##OptVals")) reset();
			ImGui::SameLine();
			if (ImGui::Button("Print##OptVals")){
				spdlog::info("Obj values:");
				Eigen::Map<Eigen::VectorXf> tmp(objectiveValues.data(), objectiveValues.size() );
				std::cout << std::endl << "obj=[ " << tmp.transpose() << "]';" << std::endl;
			}
		}
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Optimization"))
		{
			ImGui::Text("Obj. Value: %.4f", objectiveValue);
			ImGui::Text("Grad. Norm: %.4f", gradientNorm);

			if (ImGui::Button("run fwd sim")) {
				thermalVars.runSingleFwd = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("run adj sim")) {
				thermalVars.runSingleAdj = true;
			}

			if (ImGui::Button("run FD check")) {
				thermalVars.runSingleFD = true;
			}
			ImGui::SameLine();
			ImGui::Combo("FD-mode##fdm", &(thermalVars.fdMode), "Standard FD\0Range FD\0T-op only FD\0");

			if (ImGui::Combo("Optimizer##optsel", &selectOptimizer, "Grad. Desc.\0Adam") || data.optimizer == nullptr) {
				if (selectOptimizer == 0) data.optimizer.reset(new OpenLoopOptimizer(thermalVars.optimStep));
				if (selectOptimizer == 1) data.optimizer.reset(new OpenLoopAdam(thermalVars.optimStep));
				spdlog::info("Optimizer changed, selected {}", selectOptimizer);
			}

			ImGui::SliderInt("Adjoint Batch Count", &thermalVars.adjointBatchCount, 1, 20);
			ImGui::Checkbox("Const. Seed", &thermalVars.constRandSeed);
			if (thermalVars.constRandSeed)
			{
				ImGui::SameLine();
				ImGui::SliderInt("Seed", &thermalVars.randSeed, 0, 32767);
			}

			if (ImGui::Checkbox("run optim", &thermalVars.runOptim)) {}
			ImGui::SameLine();
			if (ImGui::Button("load best state")) {
				data.params->setParams(data.optimizer->getBestPreviousParams());
				thermalVars.resetGeometryFromParams = true;
			}

			if (ImGui::Checkbox("use FD for optim", &thermalVars.useFDforOptim)) {}

			if (ImGui::DragFloat("optim step", &thermalVars.optimStep, 1e-4, 1e-6, 10, "%.4g")) {
				data.optimizer->reset(thermalVars.optimStep);
				spdlog::debug("Optimizer reset, step size {}", thermalVars.optimStep);
			}
			if (ImGui::DragInt("max steps", &thermalVars.maxSteps, 1, 0, 1000)) {}
			if (ImGui::DragFloat("FD step", &thermalVars.fdStep, 1e-3, 1e-3, 10, "%.4g")) {}

			if (ImGui::Button("new viz")) {
				thermalVars.visualizeGradient = 0; // add slider to select paramIdx
			}
			if (ImGui::Button("prep t2")) {
				thermalVars.doPrepT2 = true;
			}
		}

		ImGui::End();
	}

	// DRAW CONSTRAINT MESH FOR SELECTED MODEL

	Ref_s* obj = uc->scene->getSelection();
	if (obj != NULL)
	{
		Ref_s::Type type = obj->type;
		switch (type)
		{
		case Ref_s::Type::Model:
			{
				//spdlog::debug("Selected Model type.");
				RefModel_s* model = (RefModel_s*)obj;
				string data_str = model->model->getCustomProperty("constraint-data").getString();

				if (!data_str.empty())
				{
					auto str_to_vec3 = [&data_str](std::vector<glm::vec3>& _out_vec) {
						// fill polyPoints parsing string data
						std::stringstream cstr(data_str);
						std::string ptoken, ftoken;
						float nextX, nextY;
						bool haveX, haveY;
						while (std::getline(cstr, ptoken, ';')) { // separate points by semicolon
							std::stringstream pstr(ptoken);
							haveX = false; haveY = false;
							while (!haveY && std::getline(pstr, ftoken, ',')) { // separate floats by comma
								if (!haveX) { if (1 == sscanf(ftoken.c_str(), "%f", &nextX))  haveX = true; }
								else       if (1 == sscanf(ftoken.c_str(), "%f", &nextY)) {
									haveY = true;
									_out_vec.push_back(glm::vec3(nextX, 0, nextY));
									//spdlog::debug("polyPoint: {}, {}", _out_vec.back()[0], _out_vec.back()[1]);
								}
							}
						}
						};

					std::vector<glm::vec3> points_vec3;
					str_to_vec3(points_vec3);

					if (points_vec3.size() > 1)
					{
						RefCamera_s* cam = uc->scene->getCurrentCamera();
						glm::mat4 projection_matrix = cam->camera->getProjectionMatrix();
						glm::mat4 view_matrix = cam->view_matrix;

						ImGuiIO* mIo = &ImGui::GetIO();

						std::vector<ImVec2> points;
						points.reserve(points.size());
						size_t count = 0;
						for (auto _p : points_vec3)
						{
							glm::vec4 p = projection_matrix * (view_matrix * glm::vec4(_p, 1.0f));
							glm::vec3 ndcSpaceP = glm::vec3(p) / p.w;
							glm::vec2 windowSpaceP = ((glm::vec2(ndcSpaceP) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
							//if (p.x < 0 || p.y < 0 || p.x > io->DisplaySize.x || p.y > io->DisplaySize.y) continue;
							points.emplace_back(windowSpaceP.x, windowSpaceP.y);
							count++;
						}
						points.emplace_back(points[0]); // insert first point in the end to close the loop
						count++;

						ImGui::GetBackgroundDrawList()->AddPolyline(points.data(), count, ImColor::ImColor(49, 130, 189, 255), 0, 1);
					}
				}
			}
			break;
		default:
			break;
		}
	}	
}

void ThermalGui::updateObjectiveFunctionIfNeeded( const ThermalSolver& solver, const ThermalVars_s& thermalVars, ThermalData& thermalData){
	int timeSteps = thermalVars.timeSteps;
	if(solver.compute_steady_state) timeSteps=1;

	if( thermalData.objFcn.get()==NULL || thermalData.objFcn->nTimeStepsWithTargets() != timeSteps ){
		thermalData.objFcn.reset( new ObjectiveFunction(
			Common::getInstance().getRenderSystem()->getMainScene()->getSceneData(),
			thermalData.vertexAreaVector, thermalData.initialValueVector, timeSteps
		));
		if( thermalData.objFcn->needUpdateTargets() ){
			spdlog::warn("objective function reset for {} time steps with unspecified data - run forward sim to update!", timeSteps);
		}
	}
}