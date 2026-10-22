#pragma once

#include "thermal_data.hpp"
class ParametricFunction;

class Parametrization { // handler for parametrizations of the whole scene
public:
	Parametrization(const scene_s& _scene, unsigned long vertexCount);
	~Parametrization();
	unsigned long getNumberOfParams(){ return nParams; }
	const Vec& getLastParams(){ return p; }
	void setParams(const Vec& p_){ if(p_.size()==getNumberOfParams()) p=p_; }
	void updateGeometry(const Vec& p);
	void projectGradient(Vec& dp, const Mat& dx);
	bool isNopTestActive(){return nopTestActive;}

	enum OptimizeMode {
		NOT_MOVING = 0,
		TRANSLATE_ONLY = 1,
		TRANSLATE_ROTATE = 2,
		ROTATE_ONLY = 3,
		TRANSLATE_ROTATE_XZ = 4,
		UNIFORM_SCALE = 5,
		TRANSLATE_AXIS = 6,
		SKEW_AXIS = 7,
		NOP_TEST1 = 96,
		CUSTOM97 = 97,
		CUSTOM99 = 99
	};

protected:
	Mat x0; // rest coordinates
	const scene_s scene;
	unsigned long nParams;
	Vec p;
	std::map<RefModel_s*, ParametricFunction*> fcns;
	std::map<RefModel_s*, unsigned long> fcnParamOffsets;
	bool nopTestActive=false;
};

class ParametricFunction { // a basic parametrization for translation
public:
	virtual unsigned long getNumberOfParams(){return 3;} 
	virtual void getPosition(Vec3&  x, const Vec3& x0, const Vec& p){x = x0 + p.segment<3>(0);}
	virtual void getJacobian(Mat& dx, const Vec3& x0, const Vec& p){dx.resize(3,3); dx.setIdentity();}
};

class ParametricFunctionAxisTranslation : public ParametricFunction {
public:
	ParametricFunctionAxisTranslation(Vec3 _axis) : mAxis(_axis) {}
	virtual unsigned long getNumberOfParams() { return 1; }
	virtual void getPosition(Vec3& x, const Vec3& x0, const Vec& p) { x = x0 + mAxis * p.segment<1>(0); }
	virtual void getJacobian(Mat& dx, const Vec3& x0, const Vec& p) { dx.resize(1, 3); dx.row(0) = mAxis; }

private:
	Vec3 mAxis = Vec3(0, 0, 1);
};

class ParametricFunctionAxisSkew : public ParametricFunction {
public:
	ParametricFunctionAxisSkew(Vec3 _projectionAxis, Vec3 _translationAxis) : projectionAxis(_projectionAxis), translationAxis(_translationAxis){}
	virtual unsigned long getNumberOfParams() { return 1; }
	virtual void getPosition(Vec3& x, const Vec3& x0, const Vec& p) { x = x0; x += translationAxis * projectionAxis.dot(x0) * p(0); }
	virtual void getJacobian(Mat& dx, const Vec3& x0, const Vec& p) { dx.resize(1, 3); dx = translationAxis.transpose() * projectionAxis.dot(x0); }

private:
	Vec3 projectionAxis = Vec3(0, 1, 0);
	Vec3 translationAxis = Vec3(1, 0, 0);
};

class ParametricRotation : public ParametricFunction { // a basic parametrization for rotation around the origin
public:
	virtual void getPosition(Vec3&  x, const Vec3& x0, const Vec& p);
	virtual void getJacobian(Mat& dx, const Vec3& x0, const Vec& p);
};

class ParametricTranslationRotation : public ParametricFunction { // combined translation and rotation
public:
	virtual unsigned long getNumberOfParams(){return 6;} 
	virtual void getPosition(Vec3&  x, const Vec3& x0, const Vec& p);
	virtual void getJacobian(Mat& dx, const Vec3& x0, const Vec& p);
};

class ParametricTranslationRotationInPlaneXZ : public ParametricFunction { // 3-dof translation in xz and rotation around y
public:
	ParametricTranslationRotationInPlaneXZ(float rotationScale) : mRotationScale(rotationScale) {}
	virtual unsigned long getNumberOfParams(){return 3;} 
	virtual void getPosition(Vec3&  x, const Vec3& x0, const Vec& p);
	virtual void getJacobian(Mat& dx, const Vec3& x0, const Vec& p);
private:
	float mRotationScale = 1.0;
};

class ParametricUniformScale : public ParametricFunction {
public:
	virtual unsigned long getNumberOfParams(){return 1;} 
	virtual void getPosition(Vec3&  x, const Vec3& x0, const Vec& p);
	virtual void getJacobian(Mat& dx, const Vec3& x0, const Vec& p);
};


class ParametricNopTest1 : public ParametricFunction { // a parametrization that doesn't do anything
public:
	virtual unsigned long getNumberOfParams(){return 1;} 
	virtual void getPosition(Vec3&  x, const Vec3& x0, const Vec& p){x=x0;}
	virtual void getJacobian(Mat& dx, const Vec3& x0, const Vec& p){ dx.resize(1, 3); dx.setZero(); }
};


class ParametricCustom97 : public ParametricFunction {
public:
	virtual unsigned long getNumberOfParams(){return 8;} 
	virtual void getPosition(Vec3&  x, const Vec3& x0, const Vec& p);
	virtual void getJacobian(Mat& dx, const Vec3& x0, const Vec& p);
	const float clampQuadraticValue = 0.3;
};

class ParametricCustom99 : public ParametricFunction {
public:
	ParametricCustom99(bool _forceConvex) : mForceConvex(_forceConvex) {}
	virtual unsigned long getNumberOfParams() { return 4; } // pre v2.1 return 3;
	virtual void getPosition(Eigen::Vector3f& x, const Eigen::Vector3f& x0, const Vec& p);
	virtual void getJacobian(Mat& dx, const Eigen::Vector3f& x0, const Vec& p);
	bool mForceConvex = true;
	const float mC = 10.0f;
	const float mS = 40.0f;
	const float mO = -60.310f;
	const float mYZf = 0.07f;
	const float mYf = 1.5f;
	const float mZf = 6.15;
};


//constraints (might move to separate file eventually)
class ConstraintFunction;
class InPlaneXZCollisionConstraint;

class Constraints { // handler for contraints of the whole scene
public:
	enum ConstraintMode{ UNCONSTRAINED=0, IN_POLYGON_XZ=9 };

	Constraints(const scene_s& _scene, unsigned long vertexCount);
	~Constraints(){}

	SCALAR evaluateAndAddToGradient(Mat& dx); // dx should have size (vertex-count x 3) already
protected:
	const scene_s scene;
	std::map<RefModel_s*, ConstraintFunction*> cfcns;
	std::map<unsigned int, InPlaneXZCollisionConstraint*> collisionGroupFcns;
};

class ConstraintFunction {
public:
	SCALAR penaltyFactor=1e2;
	virtual SCALAR computePenaltyAndDerivative(const Vec3& x, Vec3& dx)=0;
};

class InPolygonXZConstraint : public ConstraintFunction {
public:
	InPolygonXZConstraint(const SCALAR _penaltyFactor, const std::string& cdata);
	virtual SCALAR computePenaltyAndDerivative(const Vec3& x, Vec3& dx);
protected:
	std::vector<Eigen::Vector<SCALAR,2> > polyPoints;
};

class InPlaneXZCollisionConstraint {
public:
	InPlaneXZCollisionConstraint(const float _penaltyFactor, const unsigned int _collisionGroupID){ penaltyFactor=_penaltyFactor; collisionGroupID=_collisionGroupID;}
	SCALAR evaluateAndAddToGradient(Mat& dx); // dx should have size (vertex-count x 3) already
	unsigned int getCollisionGroupID(){return collisionGroupID;}
	void addModel(const RefModel_s* model, unsigned int vertex_offset);
protected:
	SCALAR penaltyFactor=1e2;
	std::vector<const RefModel_s*> models;
	std::vector<Vec> vertexWeights;
	std::vector<unsigned int > vertexOffsets;
	std::vector<SCALAR> boundingCircleRadii;
	unsigned int collisionGroupID;
};
