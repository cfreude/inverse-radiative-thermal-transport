#include "thermal_solver.hpp"
#include "spdlog/spdlog.h"

// System headers
#include <iostream>

// Eigen headers
#include <Eigen/Core>
#include <Eigen/Sparse>

//ToDo: add conduction (surface Laplacian) ... 

Vec ThermalSolver::residual(const Vec& x, const Vec& _currentKelvin, const Vec& _lumpedMassMatrix, const Mat& _transportMatrix, const SpMat& _conductiveLaplacian, const Vec& _fixedVars, const ACAMatrix& _transportACA, const bool _transportCompressed) {
	Vec _x = x;
	_x = (_fixedVars.array() < 1.0).select(_x, _currentKelvin);
	Vec res;

	if (_transportCompressed) {
		if (compute_steady_state) {
			res = _transportACA * _x.array().pow(4.0).matrix() + _conductiveLaplacian * _x;
		}
		else {
			res = _lumpedMassMatrix.asDiagonal() * (_x - _currentKelvin) + step_size * (_transportACA * _x.array().pow(4.0).matrix()) + step_size * _conductiveLaplacian * _x;
		}
	}
	else {
		if (compute_steady_state) {
			res = _transportMatrix * _x.array().pow(4.0).matrix() + _conductiveLaplacian * _x;
		}
		else {
			res = _lumpedMassMatrix.asDiagonal() * (_x - _currentKelvin) + step_size * _transportMatrix * _x.array().pow(4.0).matrix() + step_size * _conductiveLaplacian * _x;
		}
	}

	res = (_fixedVars.array() < 1.0).select(res, 0.0);
	return res;
}

void ThermalSolver::jacobian(const Vec& x, Mat& jac, const Vec& _lumpedMassMatrix, const Mat& _transportMatrix, const SpMat& _conductiveLaplacian, const Vec& _fixedVars, const ACAMatrix& _transportACA, const bool _transportCompressed, ACAMatrix& jacACA, bool transpose) {

	int size = x.size();
	Vec diag = 4.0 * x.array().pow(3.0);

	if (_transportCompressed) {
		if (compute_steady_state) {
			Vec& leftMult = jacACA.getD();
			leftMult = diag;
			if (!jacACA.sparseAdditionAttached()) {
				SpMat& sparseAdditionMatrix = jacACA.getA();
				sparseAdditionMatrix.setIdentity();
				sparseAdditionMatrix *= (1.0 / step_size); // Identity regularizer adjustable using step size
				sparseAdditionMatrix += _conductiveLaplacian; 
			}
		}
		else {
			Vec& leftMult = jacACA.getD();
			leftMult = step_size * diag;
			if (!jacACA.sparseAdditionAttached()) {
				typedef Eigen::Triplet<SCALAR> T;
				std::vector<T> tripletList;
				tripletList.reserve(size);				
				
				for (int i = 0; i < size; i++) {
					tripletList.push_back(T(i, i, _lumpedMassMatrix[i]));
				}

				SpMat& sparseAdditionMatrix = jacACA.getA();
				sparseAdditionMatrix.setFromTriplets(tripletList.begin(), tripletList.end());
				sparseAdditionMatrix += step_size * _conductiveLaplacian;
			}
		}

		if (transpose) jacACA.transpose(); // for adjoint: transpose before overwriting for boundary conditions

		if (!jacACA.fixedVarsSet()) jacACA.attachFixedVars(_fixedVars);
	}
	else {
		if (compute_steady_state) {
			jac = diag.asDiagonal() * _transportMatrix + _conductiveLaplacian;
			jac += (1.0 / step_size) * Mat::Identity(size, size); // Identity regularizer adjustable using step size
		}
		else {
			jac = step_size * diag.asDiagonal() * _transportMatrix + step_size * _conductiveLaplacian;
			jac += _lumpedMassMatrix.asDiagonal();
		}

		if (transpose) jac.transposeInPlace(); // for adjoint: transpose before overwriting for boundary conditions

		for (int i = 0; i < size; i++) {
			if (_fixedVars[i] > 0.0) {
				jac.row(i).setConstant(0.0);
				jac(i, i) = 1.0;
			}
		}
	}
}

void ThermalSolver::reset()
{
	jac.setConstant(0);
	jacACA.setZero();
	
	reg_called = false;
	aca_called = false;
	reg_comp_times.clear();
	aca_comp_times.clear();
}

void ThermalSolver::clearRuntimesforNextRun(bool compressed)
{
	if (compressed) {
		aca_comp_times.clear();
	}
	else {
		reg_comp_times.clear();
	}
};

void ThermalSolver::solve(Vec& x, const Vec& _currentKelvin, const Vec& _lumpedMassMatrix, const Mat& _transportMatrix, const SpMat& _conductiveLaplacian, const Vec& _fixedVars, const ACAMatrix& _transportACA, const bool _transportCompressed)
{
	if ((_transportMatrix.maxCoeff() - _transportMatrix.minCoeff()) == 0.0) {
		spdlog::warn("Check that transport operator has been computed properly (all entries are {})",_transportMatrix(0,0));
	}
	
	meshSize = _transportMatrix.rows();
	if (_transportCompressed) {
		aca_called = true;
	}
	else {
		reg_called = true;
	}

	unsigned int vsize = x.rows() * x.cols();
	jac = Mat(vsize, vsize);
	jacACA = _transportACA;
	dx = Vec(vsize);
	res = Vec(vsize);
	x_alpha_res = Vec(vsize);
	SCALAR first_res_norm=0.0;

	if (mode == 1)
	{
		if (_transportCompressed) {
			x = _transportACA * x;
		}
		else {
			x = _transportMatrix * x;
		}
		return;
	}

	for (int i = 0; i < max_iter; i++)
	{
		res = residual(x, _currentKelvin, _lumpedMassMatrix, _transportMatrix, _conductiveLaplacian, _fixedVars, _transportACA, _transportCompressed);
		prev_x = x;
		prev_res = res;
		if (i == 0) first_res_norm = std::max<SCALAR>(1.0, res.squaredNorm());
		
		if (_transportCompressed) {		
			jacobian(x, jac, _lumpedMassMatrix, _transportMatrix, _conductiveLaplacian, _fixedVars, _transportACA, _transportCompressed, jacACA, false);					
			solverACA.compute(jacACA);		
			dx = -solverACA.solve(res);

			double resCheck = (_fixedVars.array() < 1.0).select((jacACA*dx+res), 0.0).squaredNorm();
			if (solverACA.info() == Eigen::ComputationInfo::NoConvergence  || resCheck > f_tol )
			{
				spdlog::warn("Linear solver not converged |Ax-b|={}", resCheck);
				//break;
			}
		}
		else {
			jacobian(x, jac, _lumpedMassMatrix, _transportMatrix, _conductiveLaplacian, _fixedVars, _transportACA, _transportCompressed, jacACA, false);
			solver.compute(jac);
			dx = -solver.solve(res);

			double resCheck = (_fixedVars.array() < 1.0).select((jac*dx+res), 0.0).squaredNorm();
			if (solver.info() == Eigen::ComputationInfo::NoConvergence  || resCheck > f_tol )
			{
				spdlog::warn("Linear solver not converged |Ax-b|={}", resCheck);
				//break;
			}
		}

		//spdlog::debug("x:\n{}", x);
		//spdlog::debug("res:\n{}", res);
		//spdlog::debug("jac:\n{}", jac);

		
		//spdlog::debug("dx:\n{}", dx);
		//spdlog::debug("norm: {}", (jac * dx + residual(x)).squaredNorm());

		// line search
		double alpha = 1.0;
		x_alpha_res = residual(x + alpha * dx, _currentKelvin, _lumpedMassMatrix, _transportMatrix, _conductiveLaplacian, _fixedVars, _transportACA, _transportCompressed);
		SCALAR res_norm = res.squaredNorm();
		SCALAR x_alpha_norm = x_alpha_res.squaredNorm();
		unsigned int lsStep = 0;
		while( (x_alpha_norm > res_norm || isnan(x_alpha_norm) ) && lsStep < max_linesearch ){
			++lsStep;
			alpha *= 0.5;
			x_alpha_res = residual(x + alpha * dx, _currentKelvin, _lumpedMassMatrix, _transportMatrix, _conductiveLaplacian, _fixedVars, _transportACA, _transportCompressed);
			x_alpha_norm = x_alpha_res.squaredNorm();
		}

		x += alpha * dx;

		res = x_alpha_res;
		res_norm = res.squaredNorm();
		SCALAR dx_norm = (x - prev_x).squaredNorm();
		SCALAR dres_norm = (res - prev_res).squaredNorm();
		
		if (isnan(res_norm))
		{
			spdlog::error("ThermalSolver - value norm is NaN");
			break;
		}

		spdlog::info("  solve {}/{}, r^2 {}, dr^2 {}, dx^2 {}, alpha: {}", i, max_iter, res_norm, dres_norm, dx_norm, alpha);

		if( res_norm < f_tol || dx_norm < x_tol || alpha < a_tol ){
			spdlog::info("ThermalSolver finished {} iters, res^2: {} < {} (f_tol) OR dx^2: {} < {} (x_tol) OR alpha: {} < {} (a_tol)",i, res_norm, f_tol, dx_norm, x_tol, alpha, a_tol);
			if( res_norm > f_tol ) spdlog::warn("residual over tolerance - solver may not have converged - residual^2 {} (rel {})", res_norm, res_norm/first_res_norm);
			//spdlog::info("Jacobian condition number ...");
			//spdlog::info("Jacobian condition number: {:.3}", condistion_number(jac));
			break;
		}
		
	}
}

void ThermalSolver::printACAComparison() {
	if (aca_called && reg_called) {
		if (compute_steady_state) {
			SCALAR error = (T_aca - T).norm() / T.norm();
			spdlog::warn("Mesh Size: {} vertecies ", meshSize);
			spdlog::warn("ACA Temperature Error: {} | reg. dur.: {} s - ACA dur.: {} s (solver: {} s, compression: {} s )", error, reg_comp_times.at(reg_comp_times.size() - 1), aca_comp_times.at(aca_comp_times.size() - 1) + compressionDuration, aca_comp_times.at(aca_comp_times.size() - 1), compressionDuration);
			spdlog::warn("ACA max abs err. (Inf. Norm) {} (rel {})", T_inf_error, T_inf_error_rel);
			spdlog::warn("ACA max abs err. (Frob. Norm) {} (rel {})", T_frob_error, T_frob_error_rel);
		} 
		else {
			SCALAR reg_times_sum = 0;
			for (auto& t : reg_comp_times)
				reg_times_sum += t;
			SCALAR aca_times_sum = 0;
			for (auto& t : aca_comp_times)
				aca_times_sum += t;
			SCALAR error = (T_aca - T).norm() / T.norm();
			spdlog::warn("Mesh Size: {} vertecies ", meshSize);
			spdlog::warn("ACA Temperature Error: {} | reg. dur.: {} s - ACA dur.: {} s (solver: {} s, compression: {} s )", error, reg_times_sum, aca_times_sum + compressionDuration, aca_times_sum, compressionDuration);
			spdlog::warn("ACA max abs err. (Inf. Norm) {} (rel {})", T_inf_error, T_inf_error_rel);
			spdlog::warn("ACA max abs err. (Frob. Norm) {} (rel {})", T_frob_error, T_frob_error_rel);
		}
	}
	else {
		spdlog::warn("Please conduct one regular and one compressed run first!");
	}

};