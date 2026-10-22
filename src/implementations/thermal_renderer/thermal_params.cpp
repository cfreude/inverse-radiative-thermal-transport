
#include "thermal_params.hpp"
#include <sstream>

Parametrization::Parametrization(const scene_s& _scene, unsigned long vertexCount) : scene(_scene) {

	nParams=0;

	x0.resize(vertexCount, 3);
	x0.setZero();
	unsigned int vertex_offset = 0;
	for (RefModel_s* refModel : scene.refModels){
		// set parameter functions
		int opt_mode = refModel->model->getCustomProperty("optimize-mode").getInt();
		std::string obj_name = refModel->model->getName();
		switch(opt_mode){
		case TRANSLATE_ONLY:
			fcns[refModel] = new ParametricFunction(); // for plain translation simply use the base class
			spdlog::info("Translating {}", obj_name);
			break;
		case ROTATE_ONLY:
			fcns[refModel] = new ParametricRotation();
			spdlog::info("Rotating {}", obj_name);
			break;
		case TRANSLATE_ROTATE:
			fcns[refModel] = new ParametricTranslationRotation();
			spdlog::info("Rotating and translating {}", obj_name);
			break;
		case TRANSLATE_ROTATE_XZ:
		{
			tamashii::Value val = refModel->model->getCustomProperty("rotation-scale");
			SCALAR rotation_scale = 1.0;
			if (val.isEmpty())
				spdlog::warn("\"rotation-scale\" is empty!");
			else
				rotation_scale = val.getFloat();
			spdlog::info("\"rotation-scale\" set to: \"{}\"!", rotation_scale);
			fcns[refModel] = new ParametricTranslationRotationInPlaneXZ(rotation_scale);
			spdlog::info("Rotating and translating in XZ plane: {}", obj_name);
		}
			break;
		case UNIFORM_SCALE:
			fcns[refModel] = new ParametricUniformScale();
			spdlog::info("Uniformly scaling: {}", obj_name);
			break;
		case NOP_TEST1:
			fcns[refModel] = new ParametricNopTest1();
			spdlog::info("NOP_TEST {} - should be the one and only active parameter for testing!", obj_name);
			nopTestActive=true;
			break;
		case CUSTOM97:
			fcns[refModel] = new ParametricCustom97();
			spdlog::info("Custom bending {}", obj_name);
			break;

		case CUSTOM99:
			fcns[refModel] = new ParametricCustom99(false);
			spdlog::info("Custom deform {}", obj_name);
			break;
		case TRANSLATE_AXIS:
			fcns[refModel] = new ParametricFunctionAxisTranslation(Vec3(0,0,1));
			spdlog::info("Z Translation {}", obj_name);
			break;
		case SKEW_AXIS:
			//fcns[refModel] = new ParametricFunctionAxisSkew(Vec3(0, 1, 0), Vec(0.82, 0, 0.57)); // v9
			fcns[refModel] = new ParametricFunctionAxisSkew(Vec3(0, 1, 0), Vec3(1, 0, 0)); // v10
			spdlog::info("AxisSkew {}", obj_name);
			break;
		default:
			if(opt_mode != 0) spdlog::warn("Unknown optimize mode - obj. name: {}, value: {}!", obj_name, opt_mode);
			break;
		}
		
		// set parameter offsets
		if( fcns.count(refModel)==1){
			fcnParamOffsets[refModel] = nParams;
			nParams += fcns[refModel]->getNumberOfParams();
		}
		
		// store current coordinates as "rest pose"
		for(RefMesh_s* refMesh : refModel->refMeshes){
			Mesh* m = refMesh->mesh;
			vertex_s* verts = m->getVerticesArray();
			for(unsigned int id=0; id < m->getVertexCount(); ++id){
				x0.row(vertex_offset + id) = Vec3(verts[id].position.x, verts[id].position.y, verts[id].position.z).transpose();
			}
			vertex_offset += m->getVertexCount(); // track offsets
		}
	}

	p.resize(nParams);
	p.setZero();
}

Parametrization::~Parametrization() {
	for(std::map<RefModel_s*, ParametricFunction*>::iterator it=fcns.begin(); it!=fcns.end(); ++it)
		delete it->second;
	fcns.clear();
}

void Parametrization::updateGeometry(const Vec& p_){
	if( p_.size() != nParams ){
		spdlog::error("Parametrization::updateGeometry expected {} parameters, got {}", nParams, p_.size() );
		return;
	}
	p = p_; // update stored params
	// write update to mesh
	unsigned int vertex_offset = 0;
	for (RefModel_s* refModel : scene.refModels){
		for(RefMesh_s* refMesh : refModel->refMeshes){
			spdlog::info("updating vertices of {}", refModel->model->getName());
			Mesh* m = refMesh->mesh;
			vertex_s* verts = m->getVerticesArray();
			if( fcns.count(refModel)==1) for(unsigned int id=0; id < m->getVertexCount(); ++id){
				Vec3 x_ = x0.row(vertex_offset + id).transpose();
				Vec3 v = x_;
				fcns[refModel]->getPosition(v,x_,p.segment(fcnParamOffsets[refModel], fcns[refModel]->getNumberOfParams()));
				if( v.hasNaN() ){
					v=x_;
					spdlog::error("rejected vertex position update due to NaN value (vert.ID {})", vertex_offset + id);
				}
				verts[id].position = glVec4(v[0],v[1],v[2] , 1.0 );
				spdlog::debug("writing pos [{}]", eigenToStr(v.transpose()));
			}
			vertex_offset += m->getVertexCount(); // track offsets
		}
	}
}

void Parametrization::projectGradient(Vec& dp, const Mat& dx){// project gradient from vertices to parameters
	if( dp.size() != nParams ){ spdlog::error("Parametrization::projectGradient : dp has wrong size"); return;}
	unsigned int vertex_offset = 0;
	
	for (RefModel_s* refModel : scene.refModels){
		glm::mat4 model_matrix = refModel->model_matrix;
		for(RefMesh_s* refMesh : refModel->refMeshes){
			Mesh* m = refMesh->mesh;
			vertex_s* verts = m->getVerticesArray();
			if( fcns.count(refModel)==1) for(unsigned int id=0; id < m->getVertexCount(); ++id){
				Vec3 x_ = x0.row(vertex_offset + id).transpose();
				glVec3 dv( dx(vertex_offset + id, 0), dx(vertex_offset + id, 1), dx(vertex_offset + id, 2));
				// transform by (model matrix)^T, we  have world space grad dx, we want material space grad dy --> pullback by transpose Jacobian
				dv = glm::transpose(glMat3(model_matrix))*dv; //tested against FD on collision penalty only (no MC-noise) shows transpose works but inverse fails here
				Vec3 dv_( dv.x, dv.y, dv.z );
				if( dv_.hasNaN() ){
					dv_.setZero();
					spdlog::error("rejected gradient data due to NaN value (vert.ID {})", vertex_offset + id);
				}
				Mat dvdp;
				fcns[refModel]->getJacobian(dvdp,x_,p.segment(fcnParamOffsets[refModel], fcns[refModel]->getNumberOfParams()));
				dp.segment(fcnParamOffsets[refModel], fcns[refModel]->getNumberOfParams()) += dvdp * dv_;
			}
			vertex_offset += m->getVertexCount(); // track offsets
		}
	}
}


// =========================================================================
// local helper functions (Rodrigues formula and its Jacobian for rotations)
// =========================================================================

void rodriguesRotation(Mat3& rotMat, const Vec3& r){
	if( r.squaredNorm() > 1e-12 ){
		SCALAR t2 = r[0]*r[0];
		SCALAR t3 = r[1]*r[1];
		SCALAR t4 = r[2]*r[2];
		SCALAR t5 = t2+t3+t4;
		SCALAR t6 = 1.0/t5;
		SCALAR t7 = sqrt(t5);
		SCALAR t8 = 1.0/t7;
		SCALAR t9 = cos(t7);
		SCALAR t10 = sin(t7);
		SCALAR t11 = t9-1.0;
		SCALAR t12 = t2*t9;
		SCALAR t13 = t3*t9;
		SCALAR t14 = t4*t9;
		SCALAR t15 = r[0]*t8*t10;
		SCALAR t16 = r[1]*t8*t10;
		SCALAR t17 = r[2]*t8*t10;
		SCALAR t18 = r[0]*r[1]*t6*t11;
		SCALAR t19 = r[0]*r[2]*t6*t11;
		SCALAR t20 = r[1]*r[2]*t6*t11;
		SCALAR t21 = -t18;
		SCALAR t22 = -t19;
		SCALAR t23 = -t20;
		rotMat(0,0) = t6*(t2+t13+t14);
		rotMat(0,1) = -t17+t21;
		rotMat(0,2) = t16+t22;
		rotMat(1,0) = t17+t21;
		rotMat(1,1) = t6*(t3+t12+t14);
		rotMat(1,2) = -t15+t23;
		rotMat(2,0) = -t16+t22;
		rotMat(2,1) = t15+t23;
		rotMat(2,2) = t6*(t4+t12+t13);
	}else{ // norm of r too short --> I + [0.0, -r(3), r(2); r(3), 0.0, -r(1); -r(2), r(1), 0.0]
		rotMat.setIdentity();
		rotMat(0,1) = -r[2]; rotMat(0,2) =  r[1];
		rotMat(1,0) =  r[2]; rotMat(1,2) = -r[0];
		rotMat(2,0) = -r[1]; rotMat(2,1) =  r[0];
	}
}

// derivative dx/dr for x = R(r)*v, where R = rodriguesRotation(r)
void rodriguesDerivative(Mat3& rotDeriv, const Vec3& r, const Vec3& v){
	if( r.squaredNorm() > 1e-12 ){
		SCALAR t2 = r[0]*r[0];
		SCALAR t3 = r[0]*r[0]*r[0];
		SCALAR t4 = r[1]*r[1];
		SCALAR t5 = r[1]*r[1]*r[1];
		SCALAR t6 = r[2]*r[2];
		SCALAR t7 = r[2]*r[2]*r[2];
		SCALAR t8 = t2+t4+t6;
		SCALAR t9 = 1.0/t8;
		SCALAR t11 = sqrt(t8);
		SCALAR t10 = t9*t9;
		SCALAR t12 = 1.0/t11;
		SCALAR t14 = cos(t11);
		SCALAR t15 = sin(t11);
		SCALAR t13 = t12*t12*t12;
		SCALAR t16 = r[0]*t14*2.0;
		SCALAR t17 = r[1]*t14*2.0;
		SCALAR t18 = r[2]*t14*2.0;
		SCALAR t19 = t14-1.0;
		SCALAR t23 = t2*t14;
		SCALAR t24 = t4*t14;
		SCALAR t25 = t6*t14;
		SCALAR t26 = r[0]*r[1]*t9*t14;
		SCALAR t27 = r[0]*r[2]*t9*t14;
		SCALAR t28 = r[1]*r[2]*t9*t14;
		SCALAR t29 = t12*t15;
		SCALAR t20 = -t16;
		SCALAR t21 = -t17;
		SCALAR t22 = -t18;
		SCALAR t30 = t9*t23;
		SCALAR t31 = t9*t24;
		SCALAR t32 = t9*t25;
		SCALAR t33 = -t26;
		SCALAR t34 = -t27;
		SCALAR t35 = -t28;
		SCALAR t36 = r[0]*r[1]*t13*t15;
		SCALAR t37 = r[0]*r[2]*t13*t15;
		SCALAR t38 = r[1]*r[2]*t13*t15;
		SCALAR t39 = -t29;
		SCALAR t40 = r[0]*t9*t19;
		SCALAR t41 = r[1]*t9*t19;
		SCALAR t42 = r[2]*t9*t19;
		SCALAR t43 = t3*t29;
		SCALAR t44 = t2*t13*t15;
		SCALAR t45 = t5*t29;
		SCALAR t46 = t4*t13*t15;
		SCALAR t47 = t7*t29;
		SCALAR t48 = t6*t13*t15;
		SCALAR t53 = r[0]*t4*t29;
		SCALAR t54 = r[1]*t2*t29;
		SCALAR t55 = r[0]*t6*t29;
		SCALAR t56 = r[2]*t2*t29;
		SCALAR t58 = r[1]*t6*t29;
		SCALAR t60 = r[2]*t4*t29;
		SCALAR t68 = r[0]*r[1]*r[2]*t10*t19*2.0;
		SCALAR t69 = r[0]*t4*t10*t19*2.0;
		SCALAR t70 = r[1]*t2*t10*t19*2.0;
		SCALAR t71 = r[0]*t6*t10*t19*2.0;
		SCALAR t72 = r[2]*t2*t10*t19*2.0;
		SCALAR t73 = r[1]*t6*t10*t19*2.0;
		SCALAR t74 = r[2]*t4*t10*t19*2.0;
		SCALAR t75 = t6+t23+t24;
		SCALAR t76 = t4+t23+t25;
		SCALAR t77 = t2+t24+t25;
		SCALAR t49 = r[2]*t36;
		SCALAR t50 = -t36;
		SCALAR t51 = -t37;
		SCALAR t52 = -t38;
		SCALAR t57 = r[0]*t46;
		SCALAR t59 = r[1]*t44;
		SCALAR t61 = r[0]*t48;
		SCALAR t62 = r[2]*t44;
		SCALAR t63 = r[1]*t48;
		SCALAR t64 = r[2]*t46;
		SCALAR t65 = -t40;
		SCALAR t66 = -t41;
		SCALAR t67 = -t42;
		rotDeriv(0,0) = v[1]*(t34+t37+t59+t66+t70)+v[2]*(t26+t50+t62+t67+t72)-t9*v[0]*(r[0]*-2.0+t53+t55)-r[0]*t10*t77*v[0]*2.0;
		rotDeriv(0,1) = v[0]*(t27+t51+t59+t66+t70)+v[2]*(-t30+t39+t44+t49+t68)-t9*v[1]*(t20+t43+t55)-r[0]*t10*t76*v[1]*2.0;
		rotDeriv(0,2) = v[0]*(t33+t36+t62+t67+t72)+v[1]*(t29+t30-t44+t49+t68)-t9*v[2]*(t20+t43+t53)-r[0]*t10*t75*v[2]*2.0;
		rotDeriv(1,0) = v[1]*(t35+t38+t57+t65+t69)+v[2]*(t29+t31-t46+t49+t68)-t9*v[0]*(t21+t45+t58)-r[1]*t10*t77*v[0]*2.0;
		rotDeriv(1,1) = v[0]*(t28+t52+t57+t65+t69)+v[2]*(t33+t36+t64+t67+t74)-t9*v[1]*(r[1]*-2.0+t54+t58)-r[1]*t10*t76*v[1]*2.0;
		rotDeriv(1,2) = v[1]*(t26+t50+t64+t67+t74)+v[0]*(-t31+t39+t46+t49+t68)-t9*v[2]*(t21+t45+t54)-r[1]*t10*t75*v[2]*2.0;
		rotDeriv(2,0) = v[2]*(t28+t52+t61+t65+t71)+v[1]*(-t32+t39+t48+t49+t68)-t9*v[0]*(t22+t47+t60)-r[2]*t10*t77*v[0]*2.0;
		rotDeriv(2,1) = v[2]*(t34+t37+t63+t66+t73)+v[0]*(t29+t32-t48+t49+t68)-t9*v[1]*(t22+t47+t56)-r[2]*t10*t76*v[1]*2.0;
		rotDeriv(2,2) = v[0]*(t35+t38+t61+t65+t71)+v[1]*(t27+t51+t63+t66+t73)-t9*v[2]*(r[2]*-2.0+t56+t60)-r[2]*t10*t75*v[2]*2.0;
	}else{
		//small angle approx --> [   0, -v_3,  v_2;  v_3,    0, -v_1;  -v_2,  v_1,    0]
		rotDeriv(0,1) = -v[2]; rotDeriv(0,2) =  v[1];
		rotDeriv(1,0) =  v[2]; rotDeriv(1,2) = -v[0];
		rotDeriv(2,0) = -v[1]; rotDeriv(2,1) =  v[0];
	}
}

// =========================================================================
// implementations of subclasses of ParametricFunction
// =========================================================================

void ParametricRotation::getPosition(Vec3& x, const Vec3& x0, const Vec& p) {
	Mat3 rot = Mat3::Identity();
	rodriguesRotation(rot, p.segment<3>(0));
	x = rot*x0;
}

void ParametricRotation::getJacobian(Mat& dx, const Vec3& x0, const Vec& p) {
	Mat3 drot = Mat3::Zero();
	rodriguesDerivative(drot, p.segment<3>(0),x0);
	dx = drot;
}


void ParametricTranslationRotation::getPosition(Vec3& x, const Vec3& x0, const Vec& p) {
	Mat3 rot = Mat3::Identity();
	rodriguesRotation(rot, p.segment<3>(0));
	x = rot*x0 + p.segment<3>(3);
}

void ParametricTranslationRotation::getJacobian(Mat& dx, const Vec3& x0, const Vec& p) {
	Mat3 drot = Mat3::Zero();
	rodriguesDerivative(drot, p.segment<3>(0),x0);
	dx.resize(6,3);
	dx.block<3,3>(0,0) = drot;
	dx.block<3,3>(3,0).setIdentity();
}

void ParametricTranslationRotationInPlaneXZ::getPosition(Vec3& x, const Vec3& x0, const Vec& p) {
	
	SCALAR p1_scaled = p[1] * mRotationScale;

	SCALAR cosA = cos(p1_scaled), sinA = sin(p1_scaled);
	Mat3 rot = Mat3::Zero();
	rot(0,0) =  cosA; rot(0,2) = sinA;
	rot(1,1) =  1.0f;
	rot(2,0) = -sinA; rot(2,2) = cosA;

	x = rot*x0;
	x[0] += p[0]; x[2] += p[2];
}

void ParametricTranslationRotationInPlaneXZ::getJacobian(Mat& dx, const Vec3& x0, const Vec& p) {
	
	SCALAR p1_scaled = p[1] * mRotationScale;
	
	Mat3 drot = Mat3::Zero();
	SCALAR cosA = mRotationScale * cos(p1_scaled), sinA = mRotationScale * sin(p1_scaled);
	drot(0,0) = -sinA; drot(0,2) =  cosA;
	drot(2,0) = -cosA; drot(2,2) = -sinA;

	dx.resize(3,3);
	dx.block<1,3>(1,0) = drot*x0;
	dx.block<1,3>(0,0) = Vec3::UnitX();
	dx.block<1,3>(2,0) = Vec3::UnitZ();
}

void ParametricUniformScale::getPosition(Vec3& x, const Vec3& x0, const Vec& p) {
	x = (1.0+p[0])*x0; // define such that p=0 means no change
}
void ParametricUniformScale::getJacobian(Mat& dx, const Vec3& x0, const Vec& p) {
	dx.resize(1,3);
	dx.row(0) = x0.transpose();
}

void ParametricCustom97::getPosition(Vec3& x, const Vec3& x0, const Vec& p) {
	Mat3 rot = Mat3::Identity();
	rodriguesRotation(rot, p.segment<3>(0));
	x = rot*x0 + p.segment<3>(3);
	SCALAR a=p[6], b=p[7];
	if( a > clampQuadraticValue) a = clampQuadraticValue; else if( a < -clampQuadraticValue ) a = -clampQuadraticValue; // clamp
	x[0] += 0.5*a*(x0[0]-b)*(x0[0]-b);
}

void ParametricCustom97::getJacobian(Mat& dx, const Vec3& x0, const Vec& p) {
	Mat3 drot = Mat3::Zero();
	rodriguesDerivative(drot, p.segment<3>(0),x0);
	dx.resize(8,3); dx.setZero();
	dx.block<3,3>(0,0) = drot;
	dx.block<3,3>(3,0).setIdentity();
	SCALAR a=p[6], b=p[7];
	dx(6,0) += 0.5*(x0[0]-b)*(x0[0]-b);
	dx(7,0) +=  -a*(x0[0]-b);
	if( a > clampQuadraticValue){  // clamp
		a = clampQuadraticValue;
		if( dx(6,0) < 0 ) dx(6,0) = 0.0;
	}else if( a < -clampQuadraticValue ){
		a = -clampQuadraticValue;
		if( dx(6,0) > 0 ) dx(6,0) = 0.0;
	}
}


void ParametricCustom99::getPosition(Eigen::Vector3f& x, const Eigen::Vector3f& x0, const Vec& p) {
	//auto tanh_fac = [](float x, float c, float s, float o) {
	//	return tanhf((x+c+o)*s);
	//	};
	//auto falloff = [tanh_fac](float x, float c, float s, float o) {
	//	return tanh_fac(x, -c, -s, o)*tanh_fac(x, c, s, o) * 0.5f + 0.5f;
	//	};

	//float falloff_fac = falloff(x[0], mC, mS, mO);
	//float yf = x[2] * mYZf - mYf;
	//float zf = x[1] * mYZf - mZf;

	//float param = p[0];

	//if (mForceConvex)
	//	param = (param + 1) * (param + 1);

	//float deform = sin(yf) * sin(zf) * falloff_fac * param;
	//
	//x[0] = x0[0] + deform;

	x = x0;
	const float& x0_1 = x0[0]; const float& x0_2 = x0[1]; const float& x0_3 = x0[2];
	const float&  p_1 =  p[0]; const float&  p_2 =  p[1]; const float&  p_3 =  p[2];
	const float skew_factor=0.0, lowF_factor= 3.0, hiF_factor=2.0, vhf_factor = 0.8;
	const float skew_steep =0.5, lowF_steep = 1.0, hiF_steep =2.0, vhf_steep  = 8.0; // ToDo: move to class def and constructor-configurable
	// v1: x[0] = x0_1+skew_factor*x0_2*tanh(p_1*skew_steep)+lowF_factor*tanh(lowF_steep*fabs(p_2+1.0/1.0E+2))*cos(x0_3*3.141592653589793*(5.0/2.04E+2))*sin(x0_2*3.141592653589793*(5.0/2.04E+2))+hiF_factor*cos(x0_3*3.141592653589793*(2.5E+1/2.04E+2))*sin(x0_2*3.141592653589793*(2.5E+1/2.04E+2))*tanh(hiF_steep*p_3);
	// v2: x[0] = x0_1+skew_factor*x0_2*tanh(p_1*skew_steep)+hiF_factor*sin(x0_3*3.141592653589793*(5.0/5.1E+1))*sin(x0_2*3.141592653589793*(5.0/1.02E+2))*tanh(hiF_steep*p_3)+lowF_factor*sin(x0_3*3.141592653589793*(5.0/1.02E+2))*sin(x0_2*3.141592653589793*(5.0/2.04E+2))*tanh(lowF_steep*p_2);
	// v2.1:
	const float&  p_4 =  p[3];
	const float t2 = x0_3*3.141592653589793*(5.0/1.02E+2);
	const float t3 = sin(t2);
	x[0] = x0_1+skew_factor*x0_2*tanh(p_1*skew_steep)+lowF_factor*t3*sin(x0_2*3.141592653589793*(5.0/2.04E+2))*tanh(lowF_steep*p_2)+t3*vhf_factor*sin(x0_2*3.141592653589793*(2.5E+1/2.04E+2))*tanh(p_4*vhf_steep)+hiF_factor*sin(x0_3*3.141592653589793*(5.0/5.1E+1))*sin(x0_2*3.141592653589793*(5.0/1.02E+2))*tanh(hiF_steep*p_3);

	//spdlog::info("{} --> {}", x0.transpose() , x.transpose() );
}

void ParametricCustom99::getJacobian(Mat& dx, const Eigen::Vector3f& x0, const Vec& p) {
	dx.resize(getNumberOfParams(), 3);
	dx.setZero();
	const float& x0_1 = x0[0]; const float& x0_2 = x0[1]; const float& x0_3 = x0[2];
	const float&  p_1 =  p[0]; const float&  p_2 =  p[1]; const float&  p_3 =  p[2];
	const float skew_factor=0.0, lowF_factor= 3.0, hiF_factor=2.0, vhf_factor = 0.8;
	const float skew_steep =0.5, lowF_steep = 1.0, hiF_steep =2.0, vhf_steep  = 8.0; // ToDo: move to class def and constructor-configurable
	// v1:
	//const float t2 = p_2+1.0/1.0E+2;
	//dx(0,0) = -skew_steep*skew_factor*x0_2*(pow(tanh(p_1*skew_steep),2.0)-1.0);
	//dx(1,0) = -lowF_steep*lowF_factor*cos(x0_3*3.141592653589793*(5.0/2.04E+2))*sin(x0_2*3.141592653589793*(5.0/2.04E+2))*((t2/fabs(t2)))*(pow(tanh(lowF_steep*fabs(t2)),2.0)-1.0);
	//dx(2,0) = -hiF_steep*hiF_factor*cos(x0_3*3.141592653589793*(2.5E+1/2.04E+2))*sin(x0_2*3.141592653589793*(2.5E+1/2.04E+2))*(pow(tanh(hiF_steep*p_3),2.0)-1.0);
	// v2:
	//dx(0,0) = -skew_steep*skew_factor*x0_2*(pow(tanh(p_1*skew_steep),2.0)-1.0);
	//dx(1,0) = -lowF_steep*lowF_factor*sin(x0_3*3.141592653589793*(5.0/1.02E+2))*sin(x0_2*3.141592653589793*(5.0/2.04E+2))*(pow(tanh(lowF_steep*p_2),2.0)-1.0);
	//dx(2,0) = -hiF_steep*hiF_factor*sin(x0_3*3.141592653589793*(5.0/5.1E+1))*sin(x0_2*3.141592653589793*(5.0/1.02E+2))*(pow(tanh(hiF_steep*p_3),2.0)-1.0);
	// v2.1:
	const float&  p_4 =  p[3];
	const float t2 = x0_3*3.141592653589793*(5.0/1.02E+2);
	const float t3 = sin(t2);
	dx(0,0) = -skew_steep*skew_factor*x0_2*(pow(tanh(p_1*skew_steep),2.0)-1.0);
	dx(1,0) = -lowF_steep*lowF_factor*t3*sin(x0_2*3.141592653589793*(5.0/2.04E+2))*(pow(tanh(lowF_steep*p_2),2.0)-1.0);
	dx(2,0) = -hiF_steep*hiF_factor*sin(x0_3*3.141592653589793*(5.0/5.1E+1))*sin(x0_2*3.141592653589793*(5.0/1.02E+2))*(pow(tanh(hiF_steep*p_3),2.0)-1.0);
	dx(3,0) = -t3*vhf_steep*vhf_factor*sin(x0_2*3.141592653589793*(2.5E+1/2.04E+2))*(pow(tanh(p_4*vhf_steep),2.0)-1.0);


	//dx.resize(1, 3);
	//dx.setZero();
	//float x0_x = x0[0];
	//dx(0, 0) = 0.5 - 0.5 * tanh(mS * (-mC + mO + x0_x)) * tanh(mS * (mC + mO + x0_x));

	//if (mForceConvex)
	//	dx(0, 0) *= 2.0f * (p[0]+1);
}



Constraints::Constraints(const scene_s& _scene, unsigned long vertexCount) : scene(_scene) {
	unsigned int vertex_offset = 0;
	for (RefModel_s* refModel : scene.refModels){
		// set parameter functions
		int constr_mode = refModel->model->getCustomProperty("constraint-mode").getInt();
		std::string obj_name = refModel->model->getName();
		switch(constr_mode){
		case IN_POLYGON_XZ:
			cfcns[refModel] = new InPolygonXZConstraint( 
				refModel->model->getCustomProperty("constraint-weight").getFloat(),
				refModel->model->getCustomProperty("constraint-data").getString()
			);
			spdlog::info("In-Polygon XZ constrained {}", obj_name);
			spdlog::info("weight: {}", refModel->model->getCustomProperty("constraint-weight").getFloat());
			spdlog::info("CDATA:  {}", refModel->model->getCustomProperty("constraint-data").getString());
			break;
		default:
			if (constr_mode != 0) spdlog::warn("Unknown constraint mode - obj. name: {}, value: {}!", obj_name, constr_mode);
			break;
		}

		InPlaneXZCollisionConstraint* collisionFcn;
		int collision_group = refModel->model->getCustomProperty("collision-group").getInt();
		if( collision_group > 0){
			if( collisionGroupFcns.count(collision_group)>0 ) collisionFcn = collisionGroupFcns[collision_group];
			else{
				collisionFcn = new InPlaneXZCollisionConstraint(refModel->model->getCustomProperty("collision-weight").getFloat(), collision_group); 
				collisionGroupFcns[collision_group] = collisionFcn;
				spdlog::info("Collision group {} weight: {}", collision_group,refModel->model->getCustomProperty("collision-weight").getFloat());
			}
			collisionFcn->addModel(refModel, vertex_offset);
			spdlog::info("In-plane XZ collisions for {} in group {}", obj_name, collision_group);
		}

		for(RefMesh_s* refMesh : refModel->refMeshes){ vertex_offset += refMesh->mesh->getVertexCount(); } // track offsets
	}
}

SCALAR Constraints::evaluateAndAddToGradient(Mat& dx) {
	unsigned int vertex_offset = 0;
	SCALAR sum = 0.0f;
	if( dx.rows()==0 || dx.cols()!=3 ){
		spdlog::error("Constraints::evaluateAndAddToGradient(Mat& dx) --- dx must have size (vertex-count x 3)");
		return 0.0;
	}
	for (RefModel_s* refModel : scene.refModels){
		glm::mat4 model_matrix = refModel->model_matrix;
		for(RefMesh_s* refMesh : refModel->refMeshes){
			Mesh* m = refMesh->mesh;
			vertex_s* verts = m->getVerticesArray();
			if( cfcns.count(refModel)==1) for(unsigned int id=0; id < m->getVertexCount(); ++id){
				glm::vec4 x = model_matrix * verts[id].position;
				Vec3 x_( x.x, x.y, x.z ), dx_;
				sum += cfcns[refModel]->computePenaltyAndDerivative(x_,dx_);
				dx.row(vertex_offset + id) += dx_;
			}
			vertex_offset += m->getVertexCount(); // track offsets
		}
	}

	for (std::map<unsigned int, InPlaneXZCollisionConstraint*>::iterator it = collisionGroupFcns.begin(); it != collisionGroupFcns.end(); ++it) {
		sum += it->second->evaluateAndAddToGradient(dx);
	}

	return sum;
}



InPolygonXZConstraint::InPolygonXZConstraint(const SCALAR _penaltyFactor, const std::string& cdata) {
	penaltyFactor = _penaltyFactor;
	// fill polyPoints parsing string data
	std::stringstream cstr(cdata);
	std::string ptoken, ftoken;
	double nextX,nextY;
	bool haveX,haveY;
	while(std::getline(cstr, ptoken, ';')){ // separate points by semicolon
		std::stringstream pstr(ptoken);
		haveX=false; haveY=false;
		while( !haveY && std::getline(pstr,ftoken, ',')){ // separate floats by comma
			if(!haveX){if(1==sscanf(ftoken.c_str(), "%lf", &nextX))  haveX=true; }
			else       if(1==sscanf(ftoken.c_str(), "%lf", &nextY)){ haveY=true;
				polyPoints.push_back(Eigen::Vector<SCALAR,2>((SCALAR)nextX,(SCALAR)nextY));
				spdlog::debug("polyPoint: {}, {}", polyPoints.back()[0], polyPoints.back()[1]);
			}
		}
	}
}
SCALAR InPolygonXZConstraint::computePenaltyAndDerivative(const Vec3& x_, Vec3& dx) {
	dx.setZero();

	Eigen::Vector<SCALAR,2> p1, p2 = polyPoints.back();
	Eigen::Vector<SCALAR,2> closestPoint = p2;
	Eigen::Vector<SCALAR,2> x(x_[0],x_[2]); // select XZ components
	SCALAR dist = (x-p2).norm();
	int   wn = 0;
	for(unsigned int idx=0; idx<polyPoints.size(); ++idx){
		p1 = p2;
	    p2 = polyPoints[idx];

	    if( p1[0]==p2[0] && p1[1]==p2[1]) continue; // skip if points are duplicated (esp. if poly is already closed and last==first point)

	    // find closest point
		Eigen::Vector<SCALAR,2> lineDir = p2-p1;
		SCALAR lineLen = lineDir.norm();
		lineDir = lineDir/lineLen;
		Eigen::Vector<SCALAR,2> v = x-p1;
		SCALAR d = v.dot(lineDir);
		if( d<0 ) d=0; else if(d>lineLen) d=lineLen; // clamp to [0, length]
		Eigen::Vector<SCALAR,2> p = p1 + d * lineDir;
		if( (x-p).norm()<dist ){
			dist = (x-p).norm();
			closestPoint = p;
		}

		// winding number by ray cast
		if( p1[1] > x[1] ){
			if( (p2[1] <= x[1]) && (p1[0] - x[0]) * (p2[1] - x[1]) < (p2[0] - x[0]) * (p1[1] - x[1]) ){
				wn = wn + 1;
			}
		}else{
			if( (p2[1] > x[1]) && (p1[0] - x[0]) * (p2[1] - x[1]) > (p2[0] - x[0]) * (p1[1] - x[1]) ){
				wn = wn - 1;
			}
		}
	}
	bool isInside = (wn % 2) != 0; // even/odd rule

	if( !isInside ){
		// set gradient ...
		Eigen::Vector<SCALAR,2> g = penaltyFactor * (x-closestPoint);
		dx[0] = g[0]; dx[2] = g[1]; // write to XZ components
		// return penalty function value
		return penaltyFactor * 0.5*dist*dist;
	}
	return 0.0;
}

void InPlaneXZCollisionConstraint::addModel(const RefModel_s* refModel, unsigned int vertex_offset){
	glm::mat4 model_matrix = refModel->model_matrix;

	unsigned int num_verts=0;
	models.push_back( refModel );
	vertexOffsets.push_back(vertex_offset);
	vertexWeights.push_back( Vec() );

	for (RefMesh_s* refMesh : refModel->refMeshes) num_verts += refMesh->mesh->getVertexCount();
	vertexWeights.back().resize(num_verts);
	vertexWeights.back().setZero();

	glVec3 minAABB(FLT_MAX,FLT_MAX,FLT_MAX), maxAABB(-FLT_MAX,-FLT_MAX,-FLT_MAX);

	vertex_offset = 0;
	for (RefMesh_s* refMesh : refModel->refMeshes) {
		Mesh* m = refMesh->mesh;

		for (int vi = 0; vi < m->getIndexCount(); vi += 3){
			unsigned int ind0 = m->getIndicesVector()->at(vi);
			unsigned int ind1 = m->getIndicesVector()->at(vi + 1);
			unsigned int ind2 = m->getIndicesVector()->at(vi + 2);
			glm::vec4 v0_ = refModel->model_matrix * m->getVerticesVector()->at(ind0).position;
			glm::vec4 v1_ = refModel->model_matrix * m->getVerticesVector()->at(ind1).position;
			glm::vec4 v2_ = refModel->model_matrix * m->getVerticesVector()->at(ind2).position;
			glVec3 v0 = glVec3(v0_.x, v0_.y, v0_.z);
			glVec3 v1 = glVec3(v1_.x, v1_.y, v1_.z);
			glVec3 v2 = glVec3(v2_.x, v2_.y, v2_.z);

			if(v0.x < minAABB.x ) minAABB.x = v0.x; if(v0.y < minAABB.y ) minAABB.y = v0.y; if(v0.z < minAABB.z ) minAABB.z = v0.z;
			if(v1.x < minAABB.x ) minAABB.x = v1.x; if(v1.y < minAABB.y ) minAABB.y = v1.y; if(v1.z < minAABB.z ) minAABB.z = v1.z;
			if(v2.x < minAABB.x ) minAABB.x = v2.x; if(v2.y < minAABB.y ) minAABB.y = v2.y; if(v2.z < minAABB.z ) minAABB.z = v2.z;
			if(v0.x > maxAABB.x ) maxAABB.x = v0.x; if(v0.y > maxAABB.y ) maxAABB.y = v0.y; if(v0.z > maxAABB.z ) maxAABB.z = v0.z;
			if(v1.x > maxAABB.x ) maxAABB.x = v1.x; if(v1.y > maxAABB.y ) maxAABB.y = v1.y; if(v1.z > maxAABB.z ) maxAABB.z = v1.z;
			if(v2.x > maxAABB.x ) maxAABB.x = v2.x; if(v2.y > maxAABB.y ) maxAABB.y = v2.y; if(v2.z > maxAABB.z ) maxAABB.z = v2.z;


			SCALAR area = glm::length(glm::cross<SCALAR>(v1 - v0, v2 - v0)) * 0.5;
			vertexWeights.back()(vertex_offset+ind0) += area/3.0 ;
			vertexWeights.back()(vertex_offset+ind1) += area/3.0 ;
			vertexWeights.back()(vertex_offset+ind2) += area/3.0 ;
		}
		vertexWeights.back() /= vertexWeights.back().sum();
		//vertexWeights.back().setConstant(1.0 / vertexWeights.back().size());
		spdlog::info("weights size {}, sum {}", vertexWeights.back().size(), vertexWeights.back().sum());
		vertex_offset += m->getVertexCount();
	}

	minAABB.y=0.0; maxAABB.y=0.0; // discard y component
	//spdlog::debug("minAABBxz {}, {} maxAABBxz {}, {}", minAABB.x,minAABB.z, maxAABB.x,maxAABB.z);
	boundingCircleRadii.push_back( 0.5 * glm::length(maxAABB-minAABB) ); // radius of bounding circle is half the bounding box diagonal
	//spdlog::debug("radius {}",boundingCircleRadii.back());
}

SCALAR InPlaneXZCollisionConstraint::evaluateAndAddToGradient(Mat& dx){
	if( dx.rows()==0 || dx.cols()!=3 ){
		spdlog::error("Constraints::evaluateAndAddToGradient(Mat& dx) --- dx must have size (vertex-count x 3)");
		return 0.0;
	}

	SCALAR sum=0.0;
	// compute weighted centroids for all models
	std::vector<glVec3> ctrs;
	ctrs.assign( models.size(), glVec3(0.0));

	for(unsigned int mdl=0; mdl<models.size(); ++mdl){
		unsigned int vertex_offset = 0;
		for (RefMesh_s* refMesh : models[mdl]->refMeshes) {
			Mesh* m = refMesh->mesh;
			for(unsigned int ind0 = 0; ind0 < m->getVertexCount(); ++ind0){
				glm::vec4 v0_ = models[mdl]->model_matrix * m->getVerticesVector()->at(ind0).position;
				glVec3 v0 = glVec3(v0_.x, v0_.y, v0_.z);
				ctrs[mdl] += vertexWeights[mdl](vertex_offset+ind0) * v0;
			}
			vertex_offset += m->getVertexCount();
		}
		ctrs[mdl].y=0.0; // discard y component
		spdlog::info("Model centroid {} is ({}, {}, {}), radius {}" , mdl, ctrs[mdl].x,ctrs[mdl].y,ctrs[mdl].z, boundingCircleRadii[mdl]);
	}
	// pair-wise test for overlap
	for(unsigned int mdl_i=0; mdl_i<models.size(); ++mdl_i){
		for(unsigned int mdl_j=mdl_i+1; mdl_j<models.size(); ++mdl_j){
			// for each overlapping pair, add to penalty and gradient for all vertices according to weights
			glVec3 ctrDiff = ctrs[mdl_i]-ctrs[mdl_j];
			ctrDiff.y=0.0; // discard y component
			SCALAR overlap     = glm::length(ctrDiff)-boundingCircleRadii[mdl_i]-boundingCircleRadii[mdl_j];
			spdlog::debug("Pair-wise distance for {}-{} is {}", mdl_i, mdl_j, overlap);
			if( overlap < 0.0 ){
				sum += penaltyFactor * 0.5*overlap*overlap;
				// dx +=  penaltyFactor*vertexWeights[mdl] * +/-overlap*normalized(ctrDiff) ...
				glVec3 mdlGrad = penaltyFactor*overlap*glm::normalize(ctrDiff);

				unsigned int vertex_offset = 0;
				for (RefMesh_s* refMesh : models[mdl_i]->refMeshes) {
					Mesh* m = refMesh->mesh;
					for(unsigned int ind0 = 0; ind0 < m->getVertexCount(); ++ind0){
						dx(vertexOffsets[mdl_i]+vertex_offset+ind0, 0) += vertexWeights[mdl_i](vertex_offset+ind0) * mdlGrad.x;
						dx(vertexOffsets[mdl_i]+vertex_offset+ind0, 1) += vertexWeights[mdl_i](vertex_offset+ind0) * mdlGrad.y;
						dx(vertexOffsets[mdl_i]+vertex_offset+ind0, 2) += vertexWeights[mdl_i](vertex_offset+ind0) * mdlGrad.z;
					}
					vertex_offset += m->getVertexCount();
				}
				vertex_offset = 0;
				for (RefMesh_s* refMesh : models[mdl_j]->refMeshes) {
					Mesh* m = refMesh->mesh;
					for(unsigned int ind0 = 0; ind0 < m->getVertexCount(); ++ind0){
						dx(vertexOffsets[mdl_j]+vertex_offset+ind0, 0) -= vertexWeights[mdl_j](vertex_offset+ind0) * mdlGrad.x;
						dx(vertexOffsets[mdl_j]+vertex_offset+ind0, 1) -= vertexWeights[mdl_j](vertex_offset+ind0) * mdlGrad.y;
						dx(vertexOffsets[mdl_j]+vertex_offset+ind0, 2) -= vertexWeights[mdl_j](vertex_offset+ind0) * mdlGrad.z;
					}
					vertex_offset += m->getVertexCount();
				}
			}
		}
	}
	return sum;
}
