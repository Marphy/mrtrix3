/*
    Copyright 2011 Brain Research Institute, Melbourne, Australia

    Written by Robert E. Smith and J-Donald Tournier, 2011.

    This file is part of MRtrix.

    MRtrix is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MRtrix is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MRtrix.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __dwi_tractography_tracking_exec_h__
#define __dwi_tractography_tracking_exec_h__


#include "thread_queue.h"

#include "dwi/directions/set.h"

#include "dwi/tractography/streamline.h"

#include "dwi/tractography/tracking/generated_track.h"
#include "dwi/tractography/tracking/method.h"
#include "dwi/tractography/tracking/shared.h"
#include "dwi/tractography/tracking/write_kernel.h"

#include "dwi/tractography/mapping/mapper.h"
#include "dwi/tractography/mapping/mapping.h"
#include "dwi/tractography/mapping/voxel.h"

#include "dwi/tractography/seeding/dynamic.h"


#define MAX_NUM_SEED_ATTEMPTS 100000



namespace MR
{
  namespace DWI
  {
    namespace Tractography
    {
      namespace Tracking
      {


        // TODO Try having ACT as a template boolean; allow compiler to optimise out branch statements

        template <class Method> class Exec {

          public:

            static void run (const std::string& diff_path, const std::string& destination, DWI::Tractography::Properties& properties)
            {

              if (properties.find ("seed_dynamic") == properties.end()) {

                typename Method::Shared shared (diff_path, properties);
                WriteKernel writer (shared, destination, properties);
                Exec<Method> tracker (shared);
                Thread::run_queue (Thread::multi (tracker), GeneratedTrack(), writer);

              } else {

                const std::string& fod_path (properties["seed_dynamic"]);

                typedef Mapping::SetDixel SetDixel;
                typedef Mapping::TrackMapperBase TckMapper;
                typedef Seeding::WriteKernelDynamic Writer;

                DWI::Directions::FastLookupSet dirs (1281);
                Image::Buffer<float> fod_data (fod_path);
                Math::SH::check (fod_data);
                Seeding::Dynamic* seeder = new Seeding::Dynamic (fod_path, fod_data, properties.seeds.get_rng(), dirs);
                properties.seeds.add (seeder); // List is responsible for deleting this from memory

                typename Method::Shared shared (diff_path, properties);

                Writer       writer  (shared, destination, properties);
                Exec<Method> tracker (shared);

                TckMapper mapper (fod_data, dirs);
                mapper.set_upsample_ratio (Mapping::determine_upsample_ratio (fod_data, properties, 0.25));
                mapper.set_use_precise_mapping (true);

                Thread::run_queue (
                    Thread::multi (tracker), 
                    GeneratedTrack(), 
                    writer, 
                    Streamline<value_type>(), 
                    Thread::multi (mapper), 
                    SetDixel(), 
                    *seeder);

              }

            }



            Exec (const typename Method::Shared& shared) :
              S (shared),
              method (shared),
              track_excluded (false),
              track_included (S.properties.include.size(), false) { }


            bool operator() (GeneratedTrack& item) {
              if (!gen_track (item))
                return false;
              if (track_rejected (item))
                item.clear();
              S.downsampler (item);
              return true;
            }


          private:

            const typename Method::Shared& S;
            Method method;
            bool track_excluded;
            std::vector<bool> track_included;


            term_t iterate ()
            {

              const term_t method_term = (S.rk4 ? next_rk4() : method.next());

              if (method_term)
                return (S.is_act() && method.act().sgm_depth) ? TERM_IN_SGM : method_term;

              if (S.is_act()) {
                const term_t structural_term = method.act().check_structural (method.pos);
                if (structural_term)
                  return structural_term;
              }

              if (S.properties.mask.size() && !S.properties.mask.contains (method.pos))
                return EXIT_MASK;

              if (S.properties.exclude.contains (method.pos))
                return ENTER_EXCLUDE;

              // If backtracking is not enabled, add streamline to include regions as it is generated
              // If it is enabled, this check can only be performed after the streamline is completed
              if (!(S.is_act() && S.act().backtrack()))
                S.properties.include.contains (method.pos, track_included);

              if (S.stop_on_all_include && traversed_all_include_regions())
                return TRAVERSE_ALL_INCLUDE;

              return CONTINUE;

            };


            bool gen_track (GeneratedTrack& tck)
            {
              tck.clear();
              track_excluded = false;
              track_included.assign (track_included.size(), false);
              method.dir.invalidate();

              bool unidirectional = S.unidirectional;

              if (S.properties.seeds.is_finite()) {

                if (!S.properties.seeds.get_seed (method.pos, method.dir))
                  return false;
                if (!method.check_seed() || !method.init()) {
                  track_excluded = true;
                  return true;
                }

              } else {

                for (size_t num_attempts = 0; num_attempts != MAX_NUM_SEED_ATTEMPTS; ++num_attempts) {
                  if (S.properties.seeds.get_seed (method.pos, method.dir) && method.check_seed() && method.init())
                    break;
                }
                if (!method.pos.valid()) {
                  FAIL ("Failed to find suitable seed point after " + str (MAX_NUM_SEED_ATTEMPTS) + " attempts - aborting");
                  return false;
                }

              }

              if (S.is_act() && !unidirectional)
                unidirectional = method.act().seed_is_unidirectional (method.pos, method.dir);

              S.properties.include.contains (method.pos, track_included);

              const Point<value_type> seed_dir (method.dir);
              tck.push_back (method.pos);

              gen_track_unidir (tck);

              if (!track_excluded && !unidirectional) {
                tck.reverse();
                method.pos = tck.back();
                method.dir = -seed_dir;
                method.reverse_track ();
                gen_track_unidir (tck);
              }

              return true;

            }




            void gen_track_unidir (GeneratedTrack& tck)
            {

              if (S.is_act())
                method.act().sgm_depth = 0;

              term_t termination = CONTINUE;

              if (S.is_act() && S.act().backtrack()) {

                size_t revert_step = 0;

                do {
                  termination = iterate();
                  if (term_add_to_tck[termination])
                    tck.push_back (method.pos);
                  if (termination) {
                    apply_priors (termination);
                    if (track_excluded && termination != ENTER_EXCLUDE) {
                      method.truncate_track (tck, ++revert_step);
                      if (tck.size() > tck.get_seed_index() + 1) {
                        track_excluded = false;
                        termination = CONTINUE;
                        method.pos = tck.back();
                        method.dir = (tck.back() - tck[tck.size() - 2]).normalise();
                      }
                    }
                  } else if (tck.size() >= S.max_num_points) {
                    termination = LENGTH_EXCEED;
                  }
                } while (!termination);

              } else {

                do {
                  termination = iterate();
                  if (term_add_to_tck[termination])
                    tck.push_back (method.pos);
                  if (!termination && tck.size() >= S.max_num_points)
                    termination = LENGTH_EXCEED;
                } while (!termination);

              }

              apply_priors (termination);

              if (termination == EXIT_SGM) {
                truncate_exit_sgm (tck);
                method.pos = tck.back();
              }

              if (track_excluded) {
                switch (termination) {
                  case CALIBRATE_FAIL: case ENTER_CSF: case BAD_SIGNAL: case HIGH_CURVATURE:
                    S.add_rejection (ACT_POOR_TERMINATION);
                    break;
                  case LENGTH_EXCEED:
                    S.add_rejection (TRACK_TOO_LONG);
                    break;
                  case ENTER_EXCLUDE:
                    S.add_rejection (ENTER_EXCLUDE_REGION);
                    break;
                  default:
                    throw Exception ("\nFIXME: Unidirectional track excluded but termination is good!\n");
                }
              }

              if (S.is_act() && (termination == ENTER_CGM) && S.act().crop_at_gmwmi())
                S.act().crop_at_gmwmi (tck);

#ifdef DEBUG_TERMINATIONS
              S.add_termination (termination, method.pos);
#else
              S.add_termination (termination);
#endif

            }



            void apply_priors (term_t& termination)
            {

              if (S.is_act()) {

                switch (termination) {

                  case CONTINUE:
                    throw Exception ("\nFIXME: undefined termination of track in apply_priors()\n");

                  case ENTER_CGM: case EXIT_IMAGE: case EXIT_MASK: case EXIT_SGM: case TERM_IN_SGM: case TRAVERSE_ALL_INCLUDE:
                    break;

                  case ENTER_CSF: case LENGTH_EXCEED: case ENTER_EXCLUDE:
                    track_excluded = true;
                    break;

                  case CALIBRATE_FAIL: case BAD_SIGNAL: case HIGH_CURVATURE:
                    if (method.act().sgm_depth)
                      termination = TERM_IN_SGM;
                    else if (!method.act().in_pathology())
                      track_excluded = true;
                    break;

                }

              } else {

                switch (termination) {

                  case CONTINUE:
                    throw Exception ("\nFIXME: undefined termination of track in apply_priors()\n");

                  case ENTER_CGM: case ENTER_CSF: case EXIT_SGM: case TERM_IN_SGM:
                    throw Exception ("\nFIXME: Have received ACT-based termination for non-ACT tracking in apply_priors()\n");

                  case EXIT_IMAGE: case EXIT_MASK: case LENGTH_EXCEED: case CALIBRATE_FAIL: case BAD_SIGNAL: case HIGH_CURVATURE: case TRAVERSE_ALL_INCLUDE:
                    break;

                  case ENTER_EXCLUDE:
                    track_excluded = true;
                    break;

                }

              }

            }



            bool track_rejected (const std::vector< Point<float> >& tck)
            {

              if (track_excluded)
                return true;

              if (tck.size() < S.min_num_points) {
                S.add_rejection (TRACK_TOO_SHORT);
                return true;
              }

              if (S.is_act()) {

                if (!satisfy_wm_requirement (tck)) {
                  S.add_rejection (ACT_FAILED_WM_REQUIREMENT);
                  return true;
                }

                if (S.act().backtrack()) {
                  for (std::vector< Point<float> >::const_iterator i = tck.begin(); i != tck.end(); ++i)
                    S.properties.include.contains (*i, track_included);
                }

              }

              if (!traversed_all_include_regions()) {
                S.add_rejection (MISSED_INCLUDE_REGION);
                return true;
              }

              return false;

            }



            bool traversed_all_include_regions ()
            {
              for (size_t n = 0; n < track_included.size(); ++n)
                if (!track_included[n])
                  return false;
              return true;
            }



            bool satisfy_wm_requirement (const std::vector< Point<float> >& tck)
            {
              // If using the Seed_test algorithm (indicated by max_num_points == 2), don't want to execute this check
              if (S.max_num_points == 2)
                return true;
              // Used these in the ACT paper, but wasn't entirely happy with the method; can change these #defines to re-enable
              // ACT instead now defaults to a 2-voxel minimum length
              if (!ACT_WM_INT_REQ && !ACT_WM_ABS_REQ)
                return true;
              float integral = 0.0, max_value = 0.0;
              for (std::vector< Point<float> >::const_iterator i = tck.begin(); i != tck.end(); ++i) {
                if (method.act().fetch_tissue_data (*i)) {
                  const float wm = method.act().tissues().get_wm();
                  max_value = MAX (max_value, wm);
                  if (((integral += (Math::pow2 (wm) * S.internal_step_size())) > ACT_WM_INT_REQ) && (max_value > ACT_WM_ABS_REQ))
                    return true;
                }
              }
              return false;
            }



            void truncate_exit_sgm (std::vector< Point<float> >& tck)
            {

              Interpolator<SourceBufferType::voxel_type>::type source (S.source_voxel);

              const size_t sgm_start = tck.size() - method.act().sgm_depth;
              assert (sgm_start >= 0 && sgm_start < tck.size());
              size_t best_termination = tck.size() - 1;
              float min_value = INFINITY;
              for (size_t i = sgm_start; i != tck.size(); ++i) {
                method.pos = tck[i];
                method.get_data (source);
                method.dir = (tck[i] - tck[i-1]).normalise();
                const float this_value = method.get_metric();
                if (this_value < min_value) {
                  min_value = this_value;
                  best_termination = i;
                }
              }
              tck.erase (tck.begin() + best_termination + 1, tck.end());

            }



            term_t next_rk4()
            {
              term_t termination = CONTINUE;
              const Point<value_type> init_pos (method.pos);
              const Point<value_type> init_dir (method.dir);
              if ((termination = method.next()))
                return termination;
              const Point<value_type> dir_rk1 (method.dir);
              method.pos = init_pos + (dir_rk1 * (0.5 * S.step_size));
              method.dir = init_dir;
              if ((termination = method.next()))
                return termination;
              const Point<value_type> dir_rk2 (method.dir);
              method.pos = init_pos + (dir_rk2 * (0.5 * S.step_size));
              method.dir = init_dir;
              if ((termination = method.next()))
                return termination;
              const Point<value_type> dir_rk3 (method.dir);
              method.pos = init_pos + (dir_rk3 * S.step_size);
              method.dir = (dir_rk2 + dir_rk3).normalise();
              if ((termination = method.next()))
                return termination;
              const Point<value_type> dir_rk4 (method.dir);
              method.dir = (dir_rk1 + (dir_rk2 * 2.0) + (dir_rk3 * 2.0) + dir_rk4).normalise();
              method.pos = init_pos + (method.dir * S.step_size);
              const Point<value_type> final_pos (method.pos);
              const Point<value_type> final_dir (method.dir);
              if ((termination = method.next()))
                return termination;
              if (dir_rk1.dot (method.dir) < S.cos_max_angle_rk4)
                return HIGH_CURVATURE;
              method.pos = final_pos;
              method.dir = final_dir;
              return CONTINUE;
            }



        };









      }
    }
  }
}

#endif


