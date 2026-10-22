
#include "thermal_objective.hpp"

ObjectiveFunction::ObjectiveFunction(const scene_s& _scene, const Vec& vertexAreas, const Vec& initialValueVector, unsigned int nTimeSteps){
	//weights for objective function
	objectiveWeight.resizeLike(initialValueVector);
	objectiveWeight.setZero();
	objectiveTargets.clear();
	objectiveTargets.push_back( initialValueVector );
	missingTargets = 0;
	unsigned int vertex_offset = 0;
	for (RefModel_s* refModel : _scene.refModels){
		spdlog::info("Objective function weight for {} is {}", refModel->model->getName(), refModel->model->getCustomProperty("objective-weight").getFloat());
		if( refModel->model->getCustomPropertyMap()->count("objective-target")>0 ) spdlog::info("Target value for {} is '{}' ({})", refModel->model->getName(), refModel->model->getCustomProperty("objective-target").getString(), refModel->model->getCustomProperty("objective-target").getFloat());

		for(RefMesh_s* refMesh : refModel->refMeshes){
			Mesh* m = refMesh->mesh;
			for(unsigned int id=0; id < m->getVertexCount(); ++id){
				if( refModel->model->getCustomPropertyMap()->count("objective-weight")>0 ){
					objectiveWeight[vertex_offset+id] = refModel->model->getCustomProperty("objective-weight").getFloat();
				}
				if( refModel->model->getCustomPropertyMap()->count("objective-target")>0 ){
					if( refModel->model->getCustomProperty("objective-target").getString().compare("solve")==0 ){
						objectiveTargets.back()[vertex_offset+id] = -1.0; // tag values that should be replaced after first solver execution
						missingTargets = 1;
					}else{
						objectiveTargets.back()[vertex_offset+id] = refModel->model->getCustomProperty("objective-target").getFloat();
					}
				}
			}
		vertex_offset += m->getVertexCount(); // track offsets
		}
	}
	objectiveWeight = objectiveWeight.cwiseProduct(vertexAreas);
	spdlog::debug("objective weights: {}", eigenToStr(objectiveWeight.transpose()));
	spdlog::debug("target temperatures: {}", eigenToStr(objectiveTargets.back().transpose()));

	if( nTimeSteps > 1 ){
		Vec copy = objectiveTargets.back();
		objectiveTargets.assign( nTimeSteps, copy );
		missingTargets *= nTimeSteps;
	}

	if( missingTargets>0 ) spdlog::warn("some target values will be set after first solve - run forward sim to specify");
}

double ObjectiveFunction::eval(const Vec& temperature, Vec& partialDerivative, unsigned int timeStep) {
	if( missingTargets>0 ){ partialDerivative.setZero(); return 0.0; }
	if( objectiveTargets.size() <= timeStep ){ //no targets for selected time step
		spdlog::debug("objective function not available for time step {}", timeStep);
		partialDerivative.setZero(); return 0.0;
	}
	Vec tDiff = ( temperature - objectiveTargets[timeStep] );
	partialDerivative =
		       objectiveWeight.cwiseProduct( tDiff ); // partial derivative of return value wrt. temperature
	return 0.5*objectiveWeight.cwiseProduct( tDiff.cwiseProduct(tDiff) ).sum();
}

bool ObjectiveFunction::updateTargetsWithData(const Vec& valueVector, unsigned int timeStep) {
	if( missingTargets>0 ){
		if( objectiveTargets.size() <= timeStep ){ //no targets for selected time step
			spdlog::debug("targets not allocated for time step {}", timeStep);
			return missingTargets>0;
		}
		for(unsigned int idx=0; idx < objectiveTargets[timeStep].size(); ++idx){
			if( objectiveTargets[timeStep][idx] == -1.0)
				objectiveTargets[timeStep][idx] = valueVector[idx];
		}
		spdlog::warn("target temperatures updated for time step {}", timeStep);
		spdlog::debug("target temperatures[{}]: {}", timeStep, eigenToStr(objectiveTargets[timeStep].transpose()));
		--missingTargets;
	}
	return missingTargets>0;
}
