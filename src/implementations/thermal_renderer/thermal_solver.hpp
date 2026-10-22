#pragma once

#include "thermal_common.hpp"

#include "../../../assets/shader/raytracing_thermal/defines.h"

#include <Eigen/Eigen>
#include <unsupported/Eigen/MatrixFunctions>
#include <unsupported/Eigen/NonLinearOptimization>
#include "thermal_aca_matrix.hpp"

using namespace Eigen;

class ThermalSolver {

public:

	Vec ThermalSolver::residual(const Vec& x, const Vec& _currentKelvin, const Vec& _lumpedMassMatrix, const Mat& _transportMatrix, const SpMat& _conductiveLaplacian, const Vec& _fixedVars, const ACAMatrix& _acaTransport, const bool _transportCompressed);
	void ThermalSolver::jacobian(const Vec& x, Mat& jac, const Vec& _lumpedMassMatrix, const Mat& _transportMatrix, const SpMat& _conductiveLaplacian, const Vec& _fixedVars, const ACAMatrix& _acaTransport, const bool _transportCompressed, ACAMatrix& jacACA, bool transpose = false);
	void ThermalSolver::solve(Vec& x, const Vec& _currentKelvin, const Vec& _lumpedMassMatrix, const Mat& _transportMatrix, const SpMat& _conductiveLaplacian, const Vec& _fixedVars, const ACAMatrix& _acaTransport, const bool _transportCompressed);
	void reset();

	void printACAComparison();
	void clearRuntimesforNextRun(bool compressed);
	Vec& getT() { return T; };
	Vec& getTACA() { return T_aca; };
	std::vector<SCALAR>& getRegCompTime() { return reg_comp_times; };
	std::vector<SCALAR>& getACACompTime() { return aca_comp_times; };
	SCALAR& getCompressionDuration() { return compressionDuration; };
	SCALAR& getTFrobError() { return T_frob_error; };
	SCALAR& getTInfError() { return T_inf_error; };
	SCALAR& getTFrobErrorRel() { return T_frob_error_rel; };
	SCALAR& getTInfErrorRel() { return T_inf_error_rel; };

	SCALAR step_size = 300.0 * secondsUnitFactor; // 300s = 5min
	bool compute_steady_state = true;
	int mode = 0;
	int max_iter = 100;

private:

	Mat jac;
	ACAMatrix jacACA;
	Vec dx;
	Vec res;
	Vec x_alpha_res;
	Vec prev_x;
	Vec prev_res;

	SCALAR x_tol = 1e-4; // temperature change
	SCALAR f_tol = 1e-6; // total residual
	SCALAR a_tol = 1e-6; // line-search factor
	unsigned int max_linesearch = 20; // note 2^-30 = 9.3132e-10,  2^-20 = 9.5367e-07 -> cf. a_tol

	BiCGSTAB<Mat> solver;
	BiCGSTAB<ACAMatrix, IdentityPreconditioner> solverACA;

	Vec T;
	Vec T_aca;
	std::vector<SCALAR> reg_comp_times;
	std::vector<SCALAR> aca_comp_times;
	SCALAR T_frob_error;
	SCALAR T_inf_error;
	SCALAR T_frob_error_rel;
	SCALAR T_inf_error_rel;

	SCALAR compressionDuration;
	int meshSize;
	bool aca_called = false;
	bool reg_called = false;
};