/*
    Copyright 2008 Brain Research Institute, Melbourne, Australia

    Written by J-Donald Tournier, 27/06/08.

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

#ifndef __dwi_tractography_properties_h__
#define __dwi_tractography_properties_h__

#include <map>
#include "timer.h"
#include "dwi/tractography/roi.h"
#include "dwi/tractography/seeding/list.h"


#define TRACTOGRAPHY_FILE_TIMESTAMP_PRECISION 20


namespace MR
{
  namespace DWI
  {
    namespace Tractography
    {


      class Properties : public std::map<std::string, std::string> {
        public:

          Properties () { 
            set_timestamp();
          }

          void set_timestamp () {
            (*this)["timestamp"] = str (Timer::current_time(), TRACTOGRAPHY_FILE_TIMESTAMP_PRECISION);
          }

          ROISet include, exclude, mask;
          Seeding::List seeds;
          std::vector<std::string> comments;
          std::multimap<std::string, std::string> roi;


          void  clear () { 
            std::map<std::string, std::string>::clear(); 
            seeds.clear();
            include.clear();
            exclude.clear();
            mask.clear();
            comments.clear(); 
            roi.clear();
          }

          template <typename T> void set (T& variable, const std::string& name) {
            if ((*this)[name].empty()) (*this)[name] = str (variable);
            else variable = to<T> ((*this)[name]);
          }

          void load_ROIs ();
      };


      inline void check_timestamps (const Properties& a, const Properties& b, const std::string& type) 
      {
        Properties::const_iterator stamp_a = a.find ("timestamp");
        Properties::const_iterator stamp_b = b.find ("timestamp");
        if (stamp_a == a.end() || stamp_b == b.end())
          throw Exception ("unable to verify " + type + " pair: missing timestamp");
        if (stamp_a->second != stamp_b->second)
          throw Exception ("invalid " + type + " combination - timestamps do not match");
      }




      inline void check_counts (const Properties& a, const Properties& b, const std::string& type, bool abort_on_fail) 
      {
        Properties::const_iterator count_a = a.find ("count");
        Properties::const_iterator count_b = b.find ("count");
        if ((count_a == a.end()) || (count_b == b.end())) {
          std::string mesg = "unable to validate " + type + " pair: missing count field";
          if (abort_on_fail) throw Exception (mesg);
          else WARN (mesg);
        }
        if (to<size_t>(count_a->second) != to<size_t>(count_b->second)) {
          std::string mesg = type + " files do not contain same number of elements";
          if (abort_on_fail) throw Exception (mesg);
          else WARN (mesg);
        }
      }





      inline std::ostream& operator<< (std::ostream& stream, const Properties& P)
      {
        stream << "seeds: " << P.seeds;
        stream << "include: " << P.include << ", exclude: " << P.exclude << ", mask: " << P.mask << ", dict: ";
        for (std::map<std::string, std::string>::const_iterator i = P.begin(); i != P.end(); ++i)
          stream << "[ " << i->first << ": " << i->second << " ], ";
        stream << "comments: ";
        for (std::vector<std::string>::const_iterator i = P.comments.begin(); i != P.comments.end(); ++i)
          stream << "\"" << *i << "\", ";
        return (stream);
      }




    }
  }
}

#endif

