#ifndef ULTRASOFT_PRECOND_H
#define ULTRASOFT_PRECOND_H

#include "adaptor.hpp"
#include "k_point/k_point_set.hpp"
#include "context/simulation_context.hpp"
#include <nlcglib/interface.hpp>
#include "hamiltonian/non_local_operator.hpp"
#include "preconditioner/ultrasoft_precond.hpp"
#include "adaptor.hpp"
#include <stdexcept>
#include "SDDK/memory.hpp"
#include <memory>
#include <complex>

#ifdef SIRIUS_NLCGLIB

namespace sirius {

class UltrasoftPrecond : public nlcglib::UltrasoftPrecondBase
{
  private:
    using key_t = std::pair<int, int>;
    using numeric_t = std::complex<double>;
    using op_t = Ultrasoft_preconditioner<numeric_t>;

  public:
    using buffer_t = nlcglib::MatrixBaseZ::buffer_t;

  public:
    UltrasoftPrecond(const K_point_set& kset, Simulation_context& ctx, const Q_operator& q_op);

    virtual void apply(const key_t& key, buffer_t& out, buffer_t& in) const override;
    virtual std::vector<std::pair<int, int>> get_keys() const override;

  private:
    std::map<key_t, std::shared_ptr<op_t>> data;
};

UltrasoftPrecond::UltrasoftPrecond(const K_point_set& kset, Simulation_context& ctx, const Q_operator& q_op)
{
    int nk = kset.spl_num_kpoints().local_size();
    for (int ik_loc = 0; ik_loc < nk; ++ik_loc) {
        int ik   = kset.spl_num_kpoints(ik_loc);
        auto& kp = *kset[ik];
        for (int ispn = 0; ispn < ctx.num_spins(); ++ispn) {
            key_t key{ik, ispn};
            data[key] = std::make_shared<op_t>(ctx, q_op, ispn, kp.beta_projectors(), kp.gkvec());
        }
    }
}

void UltrasoftPrecond::apply(const key_t& key, buffer_t& out, buffer_t& in) const
{
    auto& op    = data.at(key);
    auto array_out = make_matrix_view(out);
    auto array_in  = make_matrix_view(in);
    op->apply(array_out, array_in);
}

std::vector<std::pair<int,int>> UltrasoftPrecond::get_keys() const
{
    std::vector<key_t> keys;
    for (auto& elem : data) {
        keys.push_back(elem.first);
    }
    return keys;
}

}  // sirius

#endif /* __NLCGLIB */
#endif /* ULTRASOFT_PRECOND_H */