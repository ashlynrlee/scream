#ifndef SCREAM_P3_MICROPHYSICS_HPP
#define SCREAM_P3_MICROPHYSICS_HPP

#include "share/atm_process/atmosphere_process.hpp"
#include "ekat/ekat_parameter_list.hpp"
#include "physics/p3/p3_main_impl.hpp"
#include "physics/p3/p3_functions.hpp"
#include "share/util/scream_common_physics_functions.hpp"

#include <string>

namespace scream
{

/*
 * The class responsible to handle the atmosphere microphysics
 *
 * The AD should store exactly ONE instance of this class stored
 * in its list of subcomponents (the AD should make sure of this).
 *
 *  Note: for now, scream is only going to accommodate P3 as microphysics
*/

  using namespace p3;
  using P3F          = Functions<Real, DefaultDevice>;
  using Spack        = typename P3F::Spack;
  using Smask        = typename P3F::Smask;
  using Pack         = ekat::Pack<Real,Spack::n>;
  using PF           = scream::PhysicsFunctions<DefaultDevice>;
  using KT           = ekat::KokkosTypes<DefaultDevice>;
  using WSM          = ekat::WorkspaceManager<Spack, KT::Device>;

  using view_1d  = typename P3F::view_1d<Real>;
  using view_2d  = typename P3F::view_2d<Spack>;
  using view_2d_const  = typename P3F::view_2d<const Spack>;
  using sview_2d = typename KokkosTypes<DefaultDevice>::template view_2d<Real>;

  using uview_1d  = Unmanaged<view_1d>;
  using uview_2d  = Unmanaged<view_2d>;
  using suview_2d = Unmanaged<sview_2d>;

class P3Microphysics : public AtmosphereProcess
{
public:
  using field_type       = Field<      Real>;
  using const_field_type = Field<const Real>;

  // Constructors
  P3Microphysics (const ekat::Comm& comm, const ekat::ParameterList& params);

  // The type of subcomponent
  AtmosphereProcessType type () const { return AtmosphereProcessType::Physics; }

  // The name of the subcomponent
  std::string name () const { return "Microphysics"; }

  // The communicator used by subcomponent
  const ekat::Comm& get_comm () const { return m_p3_comm; }

  // Get the required grid for subcomponent
  std::set<std::string> get_required_grids () const {
    static std::set<std::string> s;
    s.insert(m_p3_params.get<std::string>("Grid"));
    return s;
  }

  // Set the grid
  void set_grids (const std::shared_ptr<const GridsManager> grids_manager);

  // Get the set of required/computed fields

  /*--------------------------------------------------------------------------------------------*/
  // Most individual processes have a pre-processing step that constructs needed variables from
  // the set of fields stored in the field manager.  A structure like this defines those operations,
  // which can then be called during run_imple in the main .cpp code.
  // NOTE: the use of the ekat command "set" to copy local values into the variables of interest.
  // This is an important step to avoid having those variables just share pointers to memory.
  // Structure to handle the local generation of data needed by p3_main in run_impl
  struct p3_preamble {
    p3_preamble() = default;
    // Functor for Kokkos loop to pre-process every run step
    KOKKOS_INLINE_FUNCTION
    void operator()(const int icol) const {
      for (int ipack=0;ipack<m_npack;ipack++) {
        // The ipack slice of input variables used more than once
        const Spack& pmid_pack(pmid(icol,ipack));
        const Spack& T_atm_pack(T_atm(icol,ipack));
        const Spack& cld_frac_t_pack(cld_frac_t(icol,ipack));
        // Exner
        const auto& exner = PF::exner_function(pmid_pack);
        inv_exner(icol,ipack) = 1.0/exner;
        // Potential temperature
        th_atm(icol,ipack) = PF::calculate_theta_from_T(T_atm_pack,pmid_pack);
        // DZ
        dz(icol,ipack) = PF::calculate_dz(pseudo_density(icol,ipack), pmid_pack, T_atm_pack, qv(icol,ipack));
        // Cloud fraction
        // Set minimum cloud fraction - avoids division by zero
        cld_frac_l(icol,ipack) = ekat::max(cld_frac_t_pack,mincld);
        cld_frac_i(icol,ipack) = ekat::max(cld_frac_t_pack,mincld);
        cld_frac_r(icol,ipack) = ekat::max(cld_frac_t_pack,mincld);
        // update rain cloud fraction given neighboring levels using max-overlap approach.
        for (int ivec=0;ivec<Spack::n;ivec++)
        {
          // Hard-coded max-overlap cloud fraction calculation.  Cycle through the layers from top to bottom and determine if the rain fraction needs to
          // be updated to match the cloud fraction in the layer above.  It is necessary to calculate the location of the layer directly above this one,
          // labeled ipack_m1 and ivec_m1 respectively.  Note, the top layer has no layer above it, which is why we have the kstr index in the loop.
          Int lev = ipack*Spack::n + ivec;  // Determine the level at this pack/vec location.
          Int ipack_m1 = (lev - 1) / Spack::n;
          Int ivec_m1  = (lev - 1) % Spack::n;
          if (lev != 0) { /* Not applicable at the very top layer */
            cld_frac_r(icol,ipack)[ivec] = cld_frac_t(icol,ipack_m1)[ivec_m1]>cld_frac_r(icol,ipack)[ivec] ?
                                              cld_frac_t(icol,ipack_m1)[ivec_m1] :
                                              cld_frac_r(icol,ipack)[ivec];
          }
        }
        //
      }
    } // operator
    // Local variables
    int m_ncol, m_npack;
    Real mincld = 0.0001;  // TODO: These should be stored somewhere as more universal constants.  Or maybe in the P3 class hpp
    view_2d_const pmid;
    view_2d_const pseudo_density;
    view_2d       T_atm;
    view_2d_const cld_frac_t;
    view_2d       qv;
    view_2d       inv_exner;
    view_2d       th_atm;
    view_2d       cld_frac_l;
    view_2d       cld_frac_i;
    view_2d       cld_frac_r;
    view_2d       dz;
    // Assigning local variables
    void set_variables(const int ncol, const int npack,
           const view_2d_const& pmid_, const view_2d_const& pseudo_density_, const view_2d& T_atm_, const view_2d_const& cld_frac_t_, const view_2d& qv_,
           const view_2d& inv_exner_, const view_2d& th_atm_, const view_2d& cld_frac_l_, const view_2d& cld_frac_i_, const view_2d& cld_frac_r_, const view_2d& dz_
           )
    {
      m_ncol = ncol;
      m_npack = npack;
      // IN
      pmid = pmid_;
      pseudo_density = pseudo_density_;
      T_atm = T_atm_;
      cld_frac_t = cld_frac_t_;
      qv = qv_;
      // OUT
      inv_exner = inv_exner_;
      th_atm = th_atm_;
      cld_frac_l = cld_frac_l_;
      cld_frac_i = cld_frac_i_;
      cld_frac_r = cld_frac_r_;
      dz = dz_;
    } // set_variables
  }; // p3_preamble
  /* --------------------------------------------------------------------------------------------*/
  // Most individual processes have a post-processing step that derives variables needed by the rest
  // of the model, using outputs from this process.
  // Structure to handle the generation of data needed by the rest of the model based on output from
  // p3_main.
  struct p3_postamble {
    p3_postamble() = default;
    // Functor for Kokkos loop to pre-process every run step
    KOKKOS_INLINE_FUNCTION
    void operator()(const int icol) const {
      for (int ipack=0;ipack<m_npack;ipack++) {
        // Update the atmospheric temperature and the previous temperature.
        T_atm(icol,ipack)  = PF::calculate_T_from_theta(th_atm(icol,ipack),pmid(icol,ipack));
        T_prev(icol,ipack) = T_atm(icol,ipack);
        // Update qv_prev
        qv_prev(icol,ipack) = qv(icol,ipack);
        // Rescale effective radius' into microns
        diag_eff_radius_qc(icol,ipack) *= 1e6;
        diag_eff_radius_qi(icol,ipack) *= 1e6;
      } // for ipack
    } // operator
    // Local variables
    int m_ncol, m_npack;
    view_2d       T_atm;
    view_2d_const pmid;
    view_2d       th_atm;
    view_2d       T_prev;
    view_2d       qv;
    view_2d       qv_prev;
    view_2d       diag_eff_radius_qc;
    view_2d       diag_eff_radius_qi;
    // Assigning local values
    void set_variables(const int ncol, const int npack,
                    const view_2d& th_atm_, const view_2d_const& pmid_, const view_2d& T_atm_, const view_2d& T_prev_,
                    const view_2d& qv_, const view_2d& qv_prev_, const view_2d& diag_eff_radius_qc_, const view_2d& diag_eff_radius_qi_)
    {
      m_ncol  = ncol;
      m_npack = npack;
      // IN
      th_atm      = th_atm_;
      pmid        = pmid_;
      qv          = qv_;
      // OUT
      T_atm              = T_atm_;
      T_prev             = T_prev_;
      qv_prev            = qv_prev_;
      diag_eff_radius_qc = diag_eff_radius_qc_;
      diag_eff_radius_qi = diag_eff_radius_qi_;
      // TODO: This is a list of variables not yet defined for post-processing, but are
      // defined in the F90 p3 interface code.  So this list will need to be checked as
      // new processes come online to make sure their requirements from p3 are being met.
      // qme, vap_liq_exchange
      // ENERGY Conservation: prec_str, snow_str
      // RAD Vars: icinc, icwnc, icimrst, icwmrst
      // COSP Vars: flxprc, flxsnw, flxprc, flxsnw, cvreffliq, cvreffice, reffrain, reffsnow
    } // set_variables
  }; // p3_postamble
  /* --------------------------------------------------------------------------------------------*/

  // Structure for storing local variables initialized using the ATMBufferManager
  struct Buffer {
    // 1d view scalar, size (ncol)
    static constexpr int num_1d_scalar = 1;
    // 2d view packed, size (ncol, nlev_packs)
    static constexpr int num_2d_vector = 8;
    static constexpr int num_2dp1_vector = 2;

    uview_1d precip_ice_surf;
    uview_2d inv_exner;
    uview_2d th_atm;
    uview_2d cld_frac_l;
    uview_2d cld_frac_i;
    uview_2d cld_frac_r;
    uview_2d dz;
    uview_2d qv2qi_depos_tend;
    uview_2d rho_qi;
    uview_2d precip_liq_flux; //nlev+1
    uview_2d precip_ice_flux; //nlev+1

    suview_2d col_location;

    Spack* wsm_data;
  };

protected:

  // The three main overrides for the subcomponent
  void initialize_impl (const util::TimeStamp& t0);
  void run_impl        (const Real dt);
  void finalize_impl   ();

  // Setting the fields in the atmospheric process
  void set_required_field_impl (const Field<const Real>& f);
  void set_computed_field_impl (const Field<      Real>& f);

  // Computes total number of bytes needed for local variables
  int requested_buffer_size_in_bytes() const;

  // Set local variables using memory provided by
  // the ATMBufferManager
  void init_buffers(const ATMBufferManager &buffer_manager);

  std::map<std::string,const_field_type>  m_p3_fields_in;
  std::map<std::string,field_type>        m_p3_fields_out;

  template<typename T>
  using view_type = field_type::view_type<T*>;

  template<typename T>
  using host_view_type = field_type::get_view_type<view_type<T>,Host>;

  using host_view_in_type   = host_view_type<const_field_type::RT>;
  using host_view_out_type  = host_view_type<      field_type::RT>;

  std::map<std::string,host_view_in_type>   m_p3_host_views_in;
  std::map<std::string,host_view_out_type>  m_p3_host_views_out;

  std::map<std::string,const Real*>  m_raw_ptrs_in;
  std::map<std::string,Real*>        m_raw_ptrs_out;

  util::TimeStamp     m_current_ts;
  ekat::Comm          m_p3_comm;
  ekat::ParameterList m_p3_params;

  // Keep track of field dimensions and the iteration count
  Int m_num_cols;
  Int m_num_levs;
  Int m_nk_pack;

  // Struct which contains local variables
  Buffer m_buffer;

  // Store the structures for each arguement to p3_main;
  P3F::P3PrognosticState   prog_state;
  P3F::P3DiagnosticInputs  diag_inputs;
  P3F::P3DiagnosticOutputs diag_outputs;
  P3F::P3HistoryOnly       history_only;
  P3F::P3Infrastructure    infrastructure;
  p3_preamble              p3_preproc;
  p3_postamble             p3_postproc;
  // Iteration count is internal to P3 and keeps track of the number of times p3_main has been called.
  // infrastructure.it is passed as an arguement to p3_main and is used for identifying which iteration an error occurs.

}; // class P3Microphysics

} // namespace scream

#endif // SCREAM_P3_MICROPHYSICS_HPP
