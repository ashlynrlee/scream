#include "physics/p3/scream_p3_interface.hpp"
#include "physics/p3/atmosphere_microphysics.hpp"
#include "physics/p3/p3_inputs_initializer.hpp"
#include "physics/p3/p3_functions_f90.hpp"

#include "ekat/ekat_assert.hpp"
#include "p3_f90.hpp"

#include "ekat/kokkos/ekat_kokkos_utils.hpp"
#include "ekat/ekat_pack_kokkos.hpp"

#include <array>
#include "physics/p3/var_type.hpp"
#include <stdio.h>

namespace scream {
/*
 * P3 Microphysics routines
*/

// =========================================================================================
P3Microphysics::P3Microphysics (const ekat::Comm& comm, const ekat::ParameterList& params)
 : m_p3_comm (comm)
 , m_p3_params (params)
{
/* Anything that can be initialized without grid information can be initialized here.
 * Like universal constants, table lookups, p3 options.
*/
  if (!m_p3_params.isParameter("Grid")) {
    m_p3_params.set("Grid",std::string("SE Physics"));
  }

  m_initializer = create_field_initializer<P3InputsInitializer>();
}

// =========================================================================================
void P3Microphysics::set_grids(const std::shared_ptr<const GridsManager> grids_manager)
{
  using namespace ekat::units;

  // The units of mixing ratio Q are technically non-dimensional.
  // Nevertheless, for output reasons, we like to see 'kg/kg'.
  auto Q = kg/kg;
  Q.set_string("kg/kg");

  constexpr int NVL = SCREAM_NUM_VERTICAL_LEV;
  constexpr int QSZ =  35;  /* TODO THIS NEEDS TO BE CHANGED TO A CONFIGURABLE */

  const auto& grid_name = m_p3_params.get<std::string>("Grid");
  auto grid = grids_manager->get_grid(grid_name);
  const int num_dofs = grid->get_num_local_dofs();
  const int nc = num_dofs;

  using namespace ShortFieldTagsNames;

  FieldLayout scalar3d_layout_mid { {COL,VL}, {nc,NVL} }; // Note that C++ and Fortran read array dimensions in reverse
  FieldLayout scalar3d_layout_int { {COL,VL}, {nc,NVL+1} }; // Note that C++ and Fortran read array dimensions in reverse
  FieldLayout vector3d_layout_mid{ {COL,CMP,VL}, {nc,QSZ,NVL} };
  FieldLayout tracers_layout { {COL,VAR,VL}, {nc,QSZ,NVL} };

  // Inputs
  auto nondim = m/m;

  for (auto& i : p3_inputs){
    std :: cout << "here \n";
  }
  m_required_fields.emplace("ast",            scalar3d_layout_mid,   nondim, grid_name);
  m_required_fields.emplace("ni_activated",   scalar3d_layout_mid,   1/kg, grid_name);
  m_required_fields.emplace("nc_nuceat_tend", scalar3d_layout_mid,   1/(kg*s), grid_name);
  m_required_fields.emplace("pmid",           scalar3d_layout_mid,   Pa, grid_name);
  m_required_fields.emplace("dp",             scalar3d_layout_mid,   Pa, grid_name);
  m_required_fields.emplace("zi",             scalar3d_layout_int,   m, grid_name);

  m_required_fields.emplace("qc",             tracers_layout,   Pa, grid_name);
  m_required_fields.emplace("nc",             tracers_layout,   Pa, grid_name);
  m_required_fields.emplace("qr",             tracers_layout,   Pa, grid_name);
  m_required_fields.emplace("nr",             tracers_layout,   Pa, grid_name);
  m_required_fields.emplace("qi",             tracers_layout,   Pa, grid_name);
  m_required_fields.emplace("qm",             tracers_layout,   Pa, grid_name);
  m_required_fields.emplace("ni",             tracers_layout,   Pa, grid_name);
  m_required_fields.emplace("bm",             tracers_layout,   Pa, grid_name);
  m_required_fields.emplace("qv",             tracers_layout,   Pa, grid_name);
  m_required_fields.emplace("th",             tracers_layout,   Pa, grid_name);
  
//  m_required_fields.emplace("inv_qc_relvar",  scalar3d_layout_int,   Pa, grid_name);
//  m_required_fields.emplace("cld_frac_i",  scalar3d_layout_int,   Pa, grid_name);
//  m_required_fields.emplace("cld_frac_l",  scalar3d_layout_int,   Pa, grid_name);
//  m_required_fields.emplace("cld_frac_r",  scalar3d_layout_int,   Pa, grid_name);
//  m_required_fields.emplace("pres",  scalar3d_layout_int,   Pa, grid_name);
//  m_required_fields.emplace("dz",  scalar3d_layout_int,   Pa, grid_name);
//  m_required_fields.emplace("dpres",  scalar3d_layout_int,   Pa, grid_name);
//  m_required_fields.emplace("exner",  scalar3d_layout_int,   Pa, grid_name);
//  
  // Input-Outputs
  m_required_fields.emplace("FQ", tracers_layout,      Q, grid_name);
  m_required_fields.emplace("T",  scalar3d_layout_mid, K, grid_name);
  m_required_fields.emplace("q",  vector3d_layout_mid, Q, grid_name);

  m_computed_fields.emplace("FQ", tracers_layout,      Q, grid_name);
  m_computed_fields.emplace("T",  scalar3d_layout_mid, K, grid_name);
  m_computed_fields.emplace("q",  vector3d_layout_mid, Q, grid_name);
  
  m_computed_fields.emplace("qc",            tracers_layout,   Pa, grid_name);
  m_computed_fields.emplace("nc",            tracers_layout,   Pa, grid_name);
  m_computed_fields.emplace("qr",            tracers_layout,   Pa, grid_name);
  m_computed_fields.emplace("nr",            tracers_layout,   Pa, grid_name);
  m_computed_fields.emplace("qi",            tracers_layout,   Pa, grid_name);
  m_computed_fields.emplace("qm",            tracers_layout,   Pa, grid_name);
  m_computed_fields.emplace("ni",            tracers_layout,   Pa, grid_name);
  m_computed_fields.emplace("bm",            tracers_layout,   Pa, grid_name);
  m_computed_fields.emplace("qv",            tracers_layout,   Pa, grid_name);
  m_computed_fields.emplace("th",            tracers_layout,   Pa, grid_name);
  

//  m_computed_fields.emplace("inv_qc_relvar",  scalar3d_layout_int,   Pa, grid_name);
//  m_computed_fields.emplace("cld_frac_i",  scalar3d_layout_int,   Pa, grid_name);
//  m_computed_fields.emplace("cld_frac_l",  scalar3d_layout_int,   Pa, grid_name);
//  m_computed_fields.emplace("cld_frac_r",  scalar3d_layout_int,   Pa, grid_name);
//  m_computed_fields.emplace("pres",  scalar3d_layout_int,   Pa, grid_name);
//  m_computed_fields.emplace("dz",  scalar3d_layout_int,   Pa, grid_name);
//  m_computed_fields.emplace("dpres",  scalar3d_layout_int,   Pa, grid_name);
//  m_computed_fields.emplace("exner",  scalar3d_layout_int,   Pa, grid_name);
//
}

// =========================================================================================
void P3Microphysics::initialize (const util::TimeStamp& t0)
{
  m_current_ts = t0;

  // Call f90 routine
  p3_init_f90 ();

  // We may have to init some fields from within P3. This can be the case in a P3 standalone run.
  // Some options:
  //  - we can tell P3 it can init all inputs or specify which ones it can init. We call the
  //    resulting list of inputs the 'initializaable' (or initable) inputs. The default is
  //    that no inputs can be inited.
  //  - we can request that P3 either inits no inputs or all of the initable ones (as specified
  //    at the previous point). The default is that P3 must be in charge of init ing ALL or NONE
  //    of its initable inputs.
  // Recall that:
  //  - initable fields may not need initialization (e.g., some other atm proc that
  //    appears earlier in the atm dag might provide them).  
//  std::vector<std::string> p3_inputs = {"q","T","FQ","ast","ni_activated",
//				"nc_nuceat_tend","pmid","dp","zi", "qc", "nc", 
//				"qr" , "nr", "qi", "qm", "ni", "bm", "qv", "th"};
//			//,
//			//	"inv_qc_relvar", "cld_frac_i", "cld_frac_l", "cld_frac_r", 
//			//	"pres", "dz", "dpres", "exner"};
  using strvec = std::vector<std::string>;
  const strvec& allowed_to_init = m_p3_params.get<strvec>("Initializable Inputs",strvec(0));
  const bool can_init_all = m_p3_params.get<bool>("Can Initialize All Inputs", false);
  const bool init_all_or_none = m_p3_params.get<bool>("Must Init All Inputs Or None", true);
  
  const strvec& initable = can_init_all ? p3_inputs : allowed_to_init;
  if (initable.size()>0) {
    bool all_inited = true, all_uninited = true;
    for (const auto& name : initable) {
      const auto& f = m_p3_fields_in.at(name);
      const auto& track = f.get_header().get_tracking();
      if (track.get_init_type()==InitType::None) {
        // Nobody claimed to init this field. P3InputsInitializer will take care of it
        m_initializer->add_me_as_initializer(f);
        all_uninited &= true;
        all_inited &= false;
      } else {
        all_uninited &= false;
        all_inited &= true;
      }
    }

    // In order to gurantee some consistency between inputs, it is best if P3
    // initializes either none or all of the inputs.
    EKAT_REQUIRE_MSG (!init_all_or_none || all_inited || all_uninited,
                      "Error! Some p3 inputs were marked to be inited by P3, while others weren't.\n"
                      "       P3 was requested to init either all or none of the inputs.\n");
  }
}

// =========================================================================================
void P3Microphysics::run (const Real dt)
{
  using namespace p3;
  using namespace ekat::pack;
  using P3F = Functions<Real, DefaultDevice>;
  
//P3F::P3Infrastructure infrastructure{dt, it, its, ite, kts, kte,
//                                     do_predict_nc, col_location_d};
//P3F::P3HistoryOnly history_only{liq_ice_exchange_d, vap_liq_exchange_d,
//                                vap_ice_exchange_d};
//P3F::p3_main(prog_state, diag_inputs, diag_outputs, infrastructure,
//             history_only, nj, nk);
//
// std::array<const char*, num_views> view_names = {"q", "FQ", "T", "zi", "pmid", "dpres", "ast", "ni_activated", "nc_nuceat_tend"};

  std::vector<const Real*> in;
  std::vector<Real*> out;
  
  // Copy inputs to host. Copy also outputs, cause we might "update" them, rather than overwrite them.
  for (auto& it : m_p3_fields_in) {
    Kokkos::deep_copy(m_p3_host_views_in.at(it.first),it.second.get_view());
  }
  for (auto& it : m_p3_fields_out) {
    Kokkos::deep_copy(m_p3_host_views_out.at(it.first),it.second.get_view());
  }
  // Call f90 routine
  p3_main_f90 (dt, m_raw_ptrs_in["zi"], m_raw_ptrs_in["pmid"], m_raw_ptrs_in["dp"], m_raw_ptrs_in["ast"], m_raw_ptrs_in["ni_activated"], m_raw_ptrs_in["nc_nuceat_tend"], m_raw_ptrs_out["q"], m_raw_ptrs_out["FQ"], m_raw_ptrs_out["T"]);

// Copy outputs back to device
  for (auto& it : m_p3_fields_out) {
    Kokkos::deep_copy(it.second.get_view(),m_p3_host_views_out.at(it.first));
  }
  m_current_ts += dt;
  m_p3_fields_out.at("q").get_header().get_tracking().update_time_stamp(m_current_ts);
  m_p3_fields_out.at("FQ").get_header().get_tracking().update_time_stamp(m_current_ts);
  m_p3_fields_out.at("T").get_header().get_tracking().update_time_stamp(m_current_ts);



  
  auto qc_d = m_p3_fields_out.at("qc").get_reshaped_view<Pack<Real,4>**>();
  auto nc_d = m_p3_fields_out.at("nc").get_reshaped_view<Pack<Real,4>**>();
  auto qr_d = m_p3_fields_out.at("qr").get_reshaped_view<Pack<Real,4>**>();
  auto nr_d = m_p3_fields_out.at("nr").get_reshaped_view<Pack<Real,4>**>();
  auto qi_d = m_p3_fields_out.at("qi").get_reshaped_view<Pack<Real,4>**>();
  auto qm_d = m_p3_fields_out.at("qm").get_reshaped_view<Pack<Real,4>**>();
  auto ni_d = m_p3_fields_out.at("ni").get_reshaped_view<Pack<Real,4>**>();
  auto bm_d = m_p3_fields_out.at("bm").get_reshaped_view<Pack<Real,4>**>();
  auto qv_d = m_p3_fields_out.at("qv").get_reshaped_view<Pack<Real,4>**>();
  auto th_d = m_p3_fields_out.at("th").get_reshaped_view<Pack<Real,4>**>();

  std :: cout << "VAR TYPE : " << var_type(&qc_d) << "\n";


//  P3F :: P3PrognosticState prog_state{qc_d,nc_d, 
//				qr_d, nr_d,
//				qi_d, qm_d,
//                                ni_d, bm_d, 
//				qv_d, th_d};
//
//  auto nc_nuceat_tend_d = m_p3_fields_out.at("nc_nuceat_tend").get_reshaped_view<Pack<Real,16>**>();
//  auto ni_activated_d = m_p3_fields_out.at("ni_activated_tend").get_reshaped_view<Pack<Real,16>**>();
//  auto inv_qc_relvar_d = m_p3_fields_out.at("inv_qc_relvar").get_reshaped_view<Pack<Real,16>**>();
//  auto cld_frac_i_d = m_p3_fields_out.at("cld_frac_i").get_reshaped_view<Pack<Real,16>**>();
//  auto cld_frac_l_d = m_p3_fields_out.at("cld_frac_l").get_reshaped_view<Pack<Real,16>**>();
//  auto cld_frac_r_d = m_p3_fields_out.at("cld_frac_r").get_reshaped_view<Pack<Real,16>**>();
//  auto pres_d = m_p3_fields_out.at("pres").get_reshaped_view<Pack<Real,16>**>();
//  auto dz_d = m_p3_fields_out.at("dz").get_reshaped_view<Pack<Real,16>**>();
//  auto dpres_d = m_p3_fields_out.at("dpres").get_reshaped_view<Pack<Real,16>**>();
//  auto exner_d = m_p3_fields_out.at("exner").get_reshaped_view<Pack<Real,16>**>();
//  
//
//  P3F::P3DiagnosticInputs diag_inputs{nc_nuceat_tend_d, ni_activated_d, inv_qc_relvar_d, 
//				cld_frac_i_d, cld_frac_l_d, cld_frac_r_d, 
//				pres_d, dz_d, dpres_d, exner_d};
//
//
//



}

// =========================================================================================
void P3Microphysics::finalize()
{
  p3_finalize_f90 ();
}

// =========================================================================================
void P3Microphysics::register_fields (FieldRepository<Real, device_type>& field_repo) const {
  for (auto& fid : m_required_fields) {
    field_repo.register_field(fid);
  }
  for (auto& fid : m_computed_fields) {
    field_repo.register_field(fid);
  }
}

void P3Microphysics::set_required_field_impl (const Field<const Real, device_type>& f) {
  // Store a copy of the field. We need this in order to do some tracking checks
  // at the beginning of the run call. Other than that, there would be really
  // no need to store a scream field here; we could simply set the view ptr
  // in the Homme's view, and be done with it.
  const auto& name = f.get_header().get_identifier().name();
  m_p3_fields_in.emplace(name,f);
//  m_p3_dev_views_in[name] = f.get_view();  
  //m_p3_dev_views_in[name] = f.get_reshaped_view<const Real**>(); 
  m_p3_host_views_in[name] = Kokkos::create_mirror_view(f.get_view());
  m_raw_ptrs_in[name] = m_p3_host_views_in[name].data();
  // Add myself as customer to the field
  add_me_as_customer(f);
}

void P3Microphysics::set_computed_field_impl (const Field<      Real, device_type>& f) {
  using namespace ekat::pack;
  // Store a copy of the field. We need this in order to do some tracking updates
  // at the end of the run call. Other than that, there would be really
  // no need to store a scream field here; we could simply set the view ptr
  // in the Homme's view, and be done with it.
  const auto& name = f.get_header().get_identifier().name();
  m_p3_fields_out.emplace(name,f);
//  m_p3_dev_views_out[name] = f.get_view();  
 // m_p3_dev_views_out[name] = f.get_reshaped_view<const Real**>(); 
  m_p3_host_views_out[name] = Kokkos::create_mirror_view(f.get_view());
  m_raw_ptrs_out[name] = m_p3_host_views_out[name].data();

  // Add myself as provider for the field
  add_me_as_provider(f);
}
} // namespace scream
