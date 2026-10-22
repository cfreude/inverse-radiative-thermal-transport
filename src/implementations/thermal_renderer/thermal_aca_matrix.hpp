#pragma once

#include <iostream>
#include <Eigen/Dense>
#include <Eigen/Core>
#include "thermal_common.hpp"

class ACAMatrix;

// Inheriting Eigen::MatrixXf traits
namespace Eigen {
    namespace internal {
        template<>
        struct traits<ACAMatrix> : public Eigen::internal::traits<Mat>
        {};
    }
}

// Wrapper class for ACA-compressed transport matrix
class ACAMatrix : public Eigen::EigenBase<ACAMatrix> {
public:
    typedef SCALAR Scalar;
    typedef SCALAR RealScalar;
    typedef int StorageIndex;

    enum {
        ColsAtCompileTime = Eigen::Dynamic,
        MaxColsAtCompileTime = Eigen::Dynamic,
        IsRowMajor = false
    };

    Index rows() const { return (!m_transposed)? m_U.rows() : m_V.cols(); }
    Index cols() const { return (!m_transposed)? m_V.cols() : m_U.rows(); }

    template<typename Rhs>
    Eigen::Product<ACAMatrix, Rhs, Eigen::AliasFreeProduct> operator*(const Eigen::MatrixBase<Rhs>& x) const {
        return Eigen::Product<ACAMatrix, Rhs, Eigen::AliasFreeProduct>(*this, x.derived());
    }

    ACAMatrix() {}

    void attachFixedVars(const Vec& fixedVars) {
        m_fixedVars = fixedVars;
        m_fixedVarsSet = true;
    }

    void transpose() {
        m_transposed = !m_transposed;
    }

    void resize(int size, int acaIters) {
        m_U.resize(size, acaIters);
        m_V.resize(acaIters, size);
        m_D.resize(size);    
        m_A.resize(size, size);
        m_fixedVars.resize(size);
        m_C.resize(size, size);
    }

    void setZero() {
        m_U.setZero();
        m_V.setZero();
        m_D.setConstant(1);
        m_A.setZero();
        m_fixedVars.setZero();
        m_C.setZero();

        m_fixedVarsSet = false;
        m_transposed = false;       
    }

    Mat& getU() { return m_U; }
    Mat& getV() { return m_V; }
    Vec& getD() { return m_D; }
    SpMat& getA() { return m_A; }
    SpMat& getC() { return m_C; }
    const Mat& getU() const { return m_U; }
    const Mat& getV() const { return m_V; }
    const Vec& getD() const { return m_D; }
    const SpMat& getA() const { return m_A; }
    const Vec& getFixedVars() const { return m_fixedVars; }
    const SpMat& getC() const { return m_C; }

    bool sparseAdditionAttached() const { return (m_A.nonZeros() > 0); }
    bool isTransposed() const { return m_transposed; }
    bool fixedVarsSet() const { return m_fixedVarsSet; }

protected:
    Mat m_U;
    Mat m_V;
    Vec m_D;
    SpMat m_A;
    SpMat m_C;
    Vec m_fixedVars;

    bool m_transposed = false;
    bool m_fixedVarsSet = false;
};

// Specialized implementation of ACAMatrix * Eigen::DenseVector Product (extending internal::generic_product_impl)
namespace Eigen {
    namespace internal {

        template<typename Rhs>
        struct generic_product_impl<ACAMatrix, Rhs, DenseShape, DenseShape, GemvProduct> // GEMV stands for matrix-vector
            : generic_product_impl_base<ACAMatrix, Rhs, generic_product_impl<ACAMatrix, Rhs> >
        {
            typedef typename Product<ACAMatrix, Rhs>::Scalar Scalar;

            template<typename Dest>
            static void scaleAndAddTo(Dest& dst, const ACAMatrix& lhs, const Rhs& rhs, const Scalar& alpha)
            {
                // This method should implement "dst += alpha * lhs * rhs" 
                // alpha == 1 for iterative solvers
                //assert(alpha == Scalar(1) && "scaling is not implemented");
                //EIGEN_ONLY_USED_FOR_DEBUG(alpha);

                const Mat& U = lhs.getU();
                const Mat& V = lhs.getV();
                const Vec& D = lhs.getD();
                const SpMat& A = lhs.getA();
                const SpMat& C = lhs.getC();

                if (lhs.isTransposed()) {
                    dst.noalias() += alpha * (((rhs.transpose() * D.asDiagonal()) * U) * V + rhs.transpose() * (A + C)).transpose(); // not sure of final transpose is important (irrelevant if rhs and dst are vectors, but could be matrices in the future)
                }
                else { // not transposed
                    dst.noalias() += alpha * (D.asDiagonal() * (U * (V * rhs)) + (A + C) * rhs);
                }

                if (lhs.fixedVarsSet()) {
                    const Vec& fixedVars = lhs.getFixedVars();
                    for (int i = 0; i < fixedVars.rows(); i++) {
                        if (fixedVars[i] > 0.0) {
                            dst[i] = rhs[i];
                        }
                    }
                }
            }
        };
    }
}