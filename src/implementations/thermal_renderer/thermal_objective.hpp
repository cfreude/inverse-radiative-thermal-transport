#pragma once

#include "thermal_data.hpp"

class ObjectiveFunction {
public:
	ObjectiveFunction(const scene_s& _scene, const Vec& vertexAreas, const Vec& initialValueVector, unsigned int nTimeSteps=1);
	~ObjectiveFunction(){ objectiveTargets.clear(); }
	double eval(const Vec& temperature, Vec& partialDerivative, unsigned int timeStep=0);
	bool updateTargetsWithData(const Vec& valueVector, unsigned int timeStep=0);
	bool needUpdateTargets(){ return (missingTargets>0); }
	unsigned int nTimeStepsWithTargets(){return objectiveTargets.size();}
protected:
	Vec objectiveWeight; // per-vertex weights
	std::vector<Vec> objectiveTargets; // per-vertex target temperatures per time step
	unsigned int missingTargets=0; // if true call updateTargetsWithData to set targets
};

class OpenLoopOptimizer { // base class implements basic gradient descent
public:
	OpenLoopOptimizer(double stepSize_=1.0){ stepSize = stepSize_; isBestSet=false; bestValue=DBL_MAX; }
	virtual void step(Vec& p, const Vec& grad, double val){
		updateBestState(p,val);
		p -= stepSize * grad;
		spdlog::debug("grad. desc. step (size {}, grad norm {})", stepSize, grad.norm() );
	}
	virtual void reset(double stepSize_=1.0){ stepSize = stepSize_; }

	const Vec& getBestPreviousParams() {
		if(!isBestSet) bestParams.resize(0);
		return bestParams;
	}
	double getStepSize(){ return stepSize; }
protected:
	void updateBestState(const Vec& p, double val){
		if(!isBestSet || val < bestValue ){
			isBestSet = true;
			bestValue = val;
			bestParams = p;
		}
	}

	double stepSize;
	double bestValue;
	Vec    bestParams;
	bool   isBestSet;
};

class OpenLoopAdam : public OpenLoopOptimizer { // base class implements basic gradient descent
public:
	OpenLoopAdam(double stepSize_=1.0, double beta1_=0.9, double beta2_=0.999){ beta1 = beta1_; beta2=beta2_; reset(stepSize_); }
	virtual void reset(double stepSize_=1.0){
		stepSize = stepSize_;
		m.setZero();
		v.setZero();
		beta1Exp = beta1;
		beta2Exp = beta2;
	}
	virtual void step(Vec& p, const Vec& grad, double val){
		updateBestState(p,val);
		if( m.size() != p.size() || v.size() != p.size() ){
			m.resizeLike(p); v.resizeLike(p);
			reset(stepSize);
		}
		m = beta1 * m + (1.0 - beta1) * grad;
		v = beta2 * v + (1.0 - beta2) * (grad.array().square().matrix());
		const double m_bias = (1.0 - beta1Exp); beta1Exp *= beta1;
		const double v_bias = (1.0 - beta2Exp); beta2Exp *= beta2;
		p -= stepSize * (sqrt(v_bias) / m_bias) * (m.array() / (v.array().sqrt() + tinyEps)).matrix();
		spdlog::debug("Adam step (size {}, grad norm {}, betaExp[{},{}])", stepSize, grad.norm(), beta1Exp, beta2Exp );
	}
protected:
	double beta1, beta2;
private:
	Vec m,v;
	double beta1Exp, beta2Exp;
	const double tinyEps = 1e-8;
};
