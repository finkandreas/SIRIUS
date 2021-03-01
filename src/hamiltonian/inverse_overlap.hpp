#ifndef INVERSE_OVERLAP_H
#define INVERSE_OVERLAP_H

#include <iostream>
#include <stdexcept>

#include "non_local_operator.hpp"
#include "context/simulation_context.hpp"
#include "k_point/k_point.hpp"
#include  "beta_projectors/beta_projectors.hpp"
#include "memory.h"

namespace sirius {

class Overlap_operator
{
  public:
    Overlap_operator(const Simulation_context& simulation_context)
        : ctx_(simulation_context)
    {
    }

    const Simulation_context& ctx() const
    {
        return ctx_;
    }

  protected:
    const Simulation_context& ctx_;
};

template <class numeric_t>
class InverseS_k : public Overlap_operator
{
  public:
    InverseS_k(const Simulation_context& simulation_context, const Q_operator& q_op,
               const Beta_projectors_base& bp, int ispn)
        : Overlap_operator(simulation_context)
        , q_op(q_op)
        , bp(bp)
        , ispn(ispn)
    {
        initialize(bp);
    }

    mdarray<numeric_t, 2> apply(const mdarray<numeric_t, 2>& X);
    void apply(mdarray<numeric_t, 2>& Y, const mdarray<numeric_t, 2>& X);

    const std::string label{"inverse overlap"};

  private:
    void initialize(const Beta_projectors_base& bp);
    const Q_operator& q_op;
    const Beta_projectors_base& bp;
    const int ispn;

    mdarray<numeric_t, 2> LU;
    mdarray<int, 1> ipiv;
};

template <class numeric_t>
class S_k : public Overlap_operator
{
  public:
    S_k(const Simulation_context& ctx, const Q_operator& q_op, const Beta_projectors_base& bp, int ispn)
        : Overlap_operator(ctx)
        , q_op(q_op)
        , bp(bp)
        , ispn(ispn)
    { /* empty */
    }

    mdarray<numeric_t, 2> apply(const mdarray<numeric_t, 2>& X);
    void apply(mdarray<numeric_t, 2>& Y, const mdarray<numeric_t, 2>& X);

    const std::string label{"overlap"};

  private:
    const Q_operator& q_op;
    const Beta_projectors_base& bp;
    const int ispn;
};

template <class numeric_t>
void
InverseS_k<numeric_t>::initialize(const Beta_projectors_base& beta_projectors)
{
    auto linalg_t         = ctx_.blas_linalg_t();
    auto preferred_memory = ctx_.preferred_memory_t();

    // auto& beta_projectors = kp.beta_projectors();
    auto B              = inner_beta(beta_projectors, ctx_); // on preferred memory
    sddk::matrix<numeric_t> BQ(B.size(0), q_op.size(1));

    if (ctx_.processing_unit() == device_t::GPU) {
        BQ.allocate(memory_t::device);
    }
    // mat * Q
    q_op.lmatmul(BQ, B, this->ispn, preferred_memory);
    int n = BQ.size(0);

    if (is_device_memory(preferred_memory)) {
        BQ.allocate(memory_t::host);
        BQ.copy_to(memory_t::host);
        BQ.deallocate(memory_t::device);
    }
    // add identity matrix
    std::vector<double_complex> ones(n, double_complex{1, 0});
    linalg(linalg_t::blas).axpy(n, &linalg_const<double_complex>::one(), ones.data(), 1, BQ.at(memory_t::host),
                          n + 1);

    LU = sddk::empty_like(BQ);
    sddk::copy(LU, BQ, device_t::CPU);
    // compute inverse...
    ipiv = mdarray<int, 1>(n);
    // compute LU factorization, TODO: use GPU if needed
    linalg(linalg_t::lapack).getrf(n, n, LU.at(memory_t::host), LU.ld(), ipiv.at(memory_t::host));

    // copy LU factorization to device if needed
    auto mem = ctx_.preferred_memory_t();
    if(is_device_memory(mem)) {
        ipiv.allocate(mem);
        ipiv.copy_to(mem);

        LU.allocate(mem);
        LU.copy_to(mem);
    }
}

/// apply wfct
/// computes (X + Beta*P*Beta^H*X)
/// where P = -Q*(I + B*Q)⁻¹
template <class numeric_t>
void
InverseS_k<numeric_t>::apply(mdarray<numeric_t, 2>& Y, const mdarray<numeric_t, 2>& X)
{
    int nbnd = X.size(1);

    auto bp_gen = bp.make_generator();

    auto beta_coeffs = bp.prepare();

    int num_beta = bp.num_total_beta();

    sddk::mdarray<numeric_t, 2> bphi(num_beta, nbnd);
    // compute inner Beta^H X -> goes to host memory
    for (int ichunk = 0; ichunk < bp.num_chunks(); ++ichunk) {
        bp_gen.generate(beta_coeffs, ichunk);

        auto bphi_loc = inner<numeric_t>(ctx_.blas_linalg_t(), ctx_.processing_unit(), ctx_.preferred_memory_t(),
                                         ctx_.mem_pool(memory_t::host), beta_coeffs, X, 0, nbnd);

        // copy submatrix to bphi
        int beta_offset = beta_coeffs.beta_chunk.offset_;
#pragma omp parallel for
        for (int lbnd = 0; lbnd < nbnd; ++lbnd) {
            // issue copy operation
            sddk::copy(memory_t::host, bphi_loc.at(memory_t::host, 0, lbnd), memory_t::host,
                       bphi.at(memory_t::host, beta_offset, lbnd), bphi_loc.size(0));
        }
    }

    // compute bphi <- (I + B*Q)⁻¹ (B^H X)
    linalg(linalg_t::lapack)
        .getrs('N', num_beta, nbnd, LU.at(memory_t::host), LU.ld(), ipiv.at(memory_t::host), bphi.at(memory_t::host),
               bphi.ld());

    // compute R <- -Q * Z, where Z = (I + B*Q)⁻¹ (B^H X)
    sddk::matrix<numeric_t> R(q_op.size(0), bphi.size(1));

    // allocate bphi on gpu if needed
    if (ctx_.preferred_memory_t() == memory_t::device) {
        bphi.allocate(ctx_.mem_pool(memory_t::device));
        bphi.copy_to(memory_t::device);
        R.allocate(memory_t::device);
    }

    // compute -Q*bphi
    q_op.rmatmul(R, bphi, this->ispn, ctx_.preferred_memory_t(), -1);

    sddk::copy(Y, X);

    for (int ichunk = 0; ichunk < bp.num_chunks(); ++ichunk) {
        // std::cout << "* ichunk: " << ichunk << "\n";
        bp_gen.generate(beta_coeffs, ichunk);
        int m = Y.size(0);
        int n = Y.size(1);
        int k = beta_coeffs.pw_coeffs_a.size(1);

        memory_t mem{memory_t::none};
        linalg_t la{linalg_t::none};
        switch (ctx_.processing_unit()) {
            case device_t::CPU: {
                mem = memory_t::host;
                la  = linalg_t::blas;
                break;
            }
            case device_t::GPU: {
                mem = memory_t::device;
                la  = linalg_t::gpublas;
                break;
            }
        }

        linalg(la).gemm('N', 'N', m, n, k, &linalg_const<numeric_t>::one(),
                        beta_coeffs.pw_coeffs_a.at(mem), beta_coeffs.pw_coeffs_a.ld(),
                        R.at(mem, beta_coeffs.beta_chunk.offset_, 0), R.ld(),
                        &linalg_const<numeric_t>::one(), Y.at(mem), Y.ld());
    }
}

/// apply wfct
/// computes (X + Beta*P*Beta^H*X)
/// where P = -Q*(I + B*Q)⁻¹
template <class numeric_t>
mdarray<numeric_t, 2>
InverseS_k<numeric_t>::apply(const mdarray<numeric_t, 2>& X)
{
    auto Y = sddk::empty_like(X, ctx_.mem_pool(ctx_.preferred_memory_t()));
    this->apply(Y, X);
    return Y;
}

template <class numeric_t>
void
S_k<numeric_t>::apply(mdarray<numeric_t, 2>& Y, const mdarray<numeric_t, 2>& X)
{
    int nbnd = X.size(1);

    auto bp_gen = bp.make_generator();

    auto beta_coeffs = bp.prepare();

    int num_beta = bp.num_total_beta();

    sddk::mdarray<numeric_t, 2> bphi(num_beta, nbnd);
    // compute inner Beta^H X -> goes to host memory
    for (int ichunk = 0; ichunk < bp.num_chunks(); ++ichunk) {
        bp_gen.generate(beta_coeffs, ichunk);

        auto bphi_loc = inner<numeric_t>(ctx_.blas_linalg_t(), ctx_.processing_unit(), ctx_.preferred_memory_t(),
                                         ctx_.mem_pool(memory_t::host), beta_coeffs, X, 0, nbnd);

        // copy submatrix to bphi
        int beta_offset = beta_coeffs.beta_chunk.offset_;
        // std::printf("* apply_overlap: ichunk=%d,  beta_offset: %d\n", ichunk, beta_offset);
#pragma omp parallel for
        for (int lbnd = 0; lbnd < nbnd; ++lbnd) {
            // issue copy operation
            sddk::copy(memory_t::host, bphi_loc.at(memory_t::host, 0, lbnd), memory_t::host,
                       bphi.at(memory_t::host, beta_offset, lbnd), bphi_loc.size(0));
        }
    }

    sddk::matrix<numeric_t> R(q_op.size(0), bphi.size(1));
    // allocate bphi on gpu if needed
    if (ctx_.preferred_memory_t() == memory_t::device) {
        bphi.allocate(ctx_.mem_pool(memory_t::device));
        bphi.copy_to(memory_t::device);
        R.allocate(memory_t::device);
    }

    q_op.rmatmul(R, bphi, this->ispn, ctx_.preferred_memory_t(), 1, 0);

    sddk::copy(Y, X);

    for (int ichunk = 0; ichunk < bp.num_chunks(); ++ichunk) {
        // std::cout << "* ichunk: " << ichunk << "\n";
        bp_gen.generate(beta_coeffs, ichunk);
        int m = Y.size(0);
        int n = Y.size(1);
        int k = beta_coeffs.pw_coeffs_a.size(1);

        memory_t mem{memory_t::none};
        linalg_t la{linalg_t::none};
        switch (ctx_.processing_unit()) {
            case device_t::CPU: {
                mem = memory_t::host;
                la  = linalg_t::blas;
                break;
            }
            case device_t::GPU: {
                mem = memory_t::device;
                la  = linalg_t::gpublas;
                break;
            }
        }

        linalg(la).gemm('N', 'N', m, n, k, &linalg_const<numeric_t>::one(),
                                    beta_coeffs.pw_coeffs_a.at(mem), beta_coeffs.pw_coeffs_a.ld(),
                                    R.at(mem, beta_coeffs.beta_chunk.offset_, 0), R.ld(),
                                    &linalg_const<numeric_t>::one(), Y.at(mem), Y.ld());
    }
}

template <class numeric_t>
mdarray<numeric_t, 2>
S_k<numeric_t>::apply(const mdarray<numeric_t, 2>& X)
{
    auto Y = sddk::empty_like(X, ctx_.mem_pool(ctx_.preferred_memory_t()));
    this->apply(Y, X);
    return Y;
}

} // namespace sirius

#endif /* INVERSE_OVERLAP_H */
/** \file inverse_overlap.hpp
    \brief provides S⁻¹
 */
