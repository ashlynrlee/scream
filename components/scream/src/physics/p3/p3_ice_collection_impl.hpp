#ifndef P3_ICE_COLLECTION_IMPL_HPP
#define P3_ICE_COLLECTION_IMPL_HPP

#include "p3_functions.hpp" // for ETI only but harmless for GPU

namespace scream {
namespace p3 {

template<typename S, typename D>
KOKKOS_FUNCTION
void Functions<S,D>
::ice_cldliq_collection(
  const Spack& rho, const Spack& temp,
  const Spack& rhofaci, const Spack& table_val_qc2qi_collect,
  const Spack& qi_incld, const Spack& qc_incld,
  const Spack& ni_incld, const Spack& nc_incld,
  Spack& qc2qi_collect_tend, Spack& nc_collect_tend, Spack& qc2qr_ice_shed_tend, Spack& ncshdc,
  const Smask& range_mask, const Smask& context)
{
  constexpr Scalar qsmall = C::QSMALL;
  constexpr Scalar tmelt  = C::Tmelt;

  // set up masks
  const auto t_is_negative        = temp < tmelt;
  const auto qi_incld_gt_small = qi_incld > qsmall;
  const auto qc_incld_gt_small    = qc_incld > qsmall;
  const auto both_gt_small        = qi_incld_gt_small && qc_incld_gt_small && context;
  const auto both_gt_small_pos_t  = both_gt_small && !t_is_negative;

  constexpr auto eci = C::eci;
  constexpr auto inv_dropmass = C::ONE/C::dropmass;

  qc2qi_collect_tend.set(both_gt_small && t_is_negative,
            rhofaci*table_val_qc2qi_collect*qc_incld*eci*rho*ni_incld);
  nc_collect_tend.set(both_gt_small, rhofaci*table_val_qc2qi_collect*nc_incld*eci*rho*ni_incld);

  // for T_atm > 273.15, assume cloud water is collected and shed as rain drops
  // sink for cloud water mass and number, note qcshed is source for rain mass
  qc2qr_ice_shed_tend.set(both_gt_small_pos_t, rhofaci*table_val_qc2qi_collect*qc_incld*eci*rho*ni_incld);
  nc_collect_tend.set(both_gt_small_pos_t, rhofaci*table_val_qc2qi_collect*nc_incld*eci*rho*ni_incld);
  // source for rain number, assume 1 mm drops are shed
  ncshdc.set(both_gt_small_pos_t, qc2qr_ice_shed_tend*inv_dropmass);
}

template<typename S, typename D>
KOKKOS_FUNCTION
void Functions<S,D>
::ice_rain_collection(
  const Spack& rho, const Spack& temp,
  const Spack& rhofaci, const Spack& logn0r,
  const Spack& table_val_nr_collect, const Spack& table_val_qr2qi_collect,
  const Spack& qi_incld, const Spack& ni_incld,
  const Spack& qr_incld,
  Spack& qr2qi_collect_tend, Spack& nr_collect_tend,
  const Smask& context)
{
  constexpr Scalar qsmall = C::QSMALL;
  constexpr Scalar tmelt  = C::Tmelt;

  // Set up masks
  const auto t_is_negative        = temp <= tmelt;
  const auto qi_incld_ge_small = qi_incld >= qsmall;
  const auto qr_incld_ge_small    = qr_incld >= qsmall;
  const auto both_gt_small        = qi_incld_ge_small && qr_incld_ge_small && context;
  const auto both_gt_small_neg_t  = both_gt_small && t_is_negative;

  constexpr Scalar ten = 10.0;
  constexpr auto eri = C::eri;

  // note: table_val_qr2qi_collect and logn0r are already calculated as log_10
  qr2qi_collect_tend.set(both_gt_small_neg_t, pow(ten, table_val_qr2qi_collect+logn0r)*rho*rhofaci*eri*ni_incld);
  nr_collect_tend.set(both_gt_small_neg_t, pow(ten, table_val_nr_collect+logn0r)*rho*rhofaci*eri*ni_incld);

  // rain number sink due to collection
  // for T_atm > 273.15, assume collected rain number is shed as
  // 1 mm drops
  // note that melting of ice number is scaled to the loss
  // rate of ice mass due to melting
  // collection of rain above freezing does not impact total rain mass
  nr_collect_tend.set(both_gt_small && !t_is_negative,
            pow(ten, table_val_nr_collect + logn0r)*rho*rhofaci*eri*ni_incld);
  // for now neglect shedding of ice collecting rain above freezing, since snow is
  // not expected to shed in these conditions (though more hevaily rimed ice would be
  // expected to lead to shedding)
}

template<typename S, typename D>
KOKKOS_FUNCTION
void Functions<S,D>
::ice_self_collection(
  const Spack& rho, const Spack& rhofaci,
  const Spack& table_val_ni_self_collect, const Spack& eii,
  const Spack& qm_incld, const Spack& qi_incld,
  const Spack& ni_incld, Spack& ni_selfcollect_tend,
  const Smask& context)
{
  constexpr Scalar qsmall = C::QSMALL;
  constexpr Scalar zero = C::ZERO;

  // Set up masks
  const auto qm_incld_positive = qm_incld > zero && context;
  const auto qi_incld_gt_small = qi_incld > qsmall && context;

  Spack tmp1{0.0};
  Spack Eii_fact{0.0};
  Smask tmp1_lt_six{0};
  Smask tmp1_ge_six{0};
  Smask tmp1_lt_nine{0};
  Smask tmp1_ge_nine{0};

  if (qi_incld_gt_small.any()) {
    // Determine additional collection efficiency factor to be applied to ice-ice collection.
    // The computed values of qicol and nicol are multipiled by Eii_fact to gradually shut off collection
    // if ice is highly rimed.
    tmp1.set(qi_incld_gt_small && qm_incld_positive,
             qm_incld/qi_incld);   //rime mass fraction
    tmp1_lt_six  = tmp1 < sp(0.6);
    tmp1_ge_six  = tmp1 >= sp(0.6);
    tmp1_lt_nine = tmp1 < sp(0.9);
    tmp1_ge_nine = tmp1 >= sp(0.9);

    Eii_fact.set(tmp1_lt_six && qm_incld_positive, 1);

    Eii_fact.set(tmp1_ge_six && tmp1_lt_nine && qm_incld_positive,
                 1 - (tmp1-sp(0.6))/sp(0.3));

    Eii_fact.set(tmp1_ge_nine && qm_incld_positive, 0);

    Eii_fact.set(!qm_incld_positive && context, 1);

    ni_selfcollect_tend.set(qi_incld_gt_small,
              table_val_ni_self_collect*rho*eii*Eii_fact*rhofaci*ni_incld);
  }
}

} // namespace p3
} // namespace scream

#endif
