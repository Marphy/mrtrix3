/*
    Copyright 2008 Brain Research Institute, Melbourne, Australia

    Written by J-Donald Tournier, 27/11/09.

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

#include <list>

#include "app.h"
#include "progressbar.h"
#include "image/voxel.h"
#include "dataset/loop.h"
#include "dataset/min_max.h"
#include "dataset/histogram.h"

using namespace MR; 

SET_VERSION_DEFAULT;
SET_AUTHOR (NULL);
SET_COPYRIGHT (NULL);

DESCRIPTION = {
 "create bitwise image by thresholding image intensity.",
 "By default, the threshold level is determined using a histogram analysis to cut out the background. Otherwise, the threshold intensity can be specified using command line options. Note that only the first study is used for thresholding.",
  NULL
};

ARGUMENTS = {
  Argument ("input", "input image", "the input image to be thresholded.").type_image_in (),
  Argument ("output", "output image", "the output binary image mask.").type_image_out (),
  Argument::End
};


OPTIONS = { 
  Option ("abs", "absolute threshold", "specify threshold value as absolute intensity.")
    .append (Argument ("value", "value", "the absolute threshold to use.").type_float (NAN, NAN, 0.0)),

  Option ("percentile", "threshold ith percentile", "threshold the image at the ith percentile.")
    .append (Argument ("value", "value", "the percentile at which to threshold.").type_float (0.0, 100.0, 95.0)),

  Option ("top", "top N voxels", "provide a mask of the N top-valued voxels")
    .append (Argument ("N", "N", "the number of voxels.").type_integer (0, INT_MAX, 100)),

  Option ("bottom", "bottom N voxels", "provide a mask of the N bottom-valued voxels")
    .append (Argument ("N", "N", "the number of voxels.").type_integer (0, INT_MAX, 100)),

  Option ("invert", "invert mask.", "invert output binary mask."),

  Option ("nan", "use NaN.", "replace all zero values with NaN."),

  Option::End 
};







EXECUTE {
  float val (NAN), percentile (NAN);
  size_t topN (0), bottomN (0), nopt (0);

  OptionList opt = get_options ("abs");
  if (opt.size()) {
    val = opt[0][0].get_float();
    ++nopt;
  }

  opt = get_options ("percentile"); 
  if (opt.size()) {
    percentile = opt[0][0].get_float();
    ++nopt;
  }

  opt = get_options ("top");
  if (opt.size()) {
    topN = opt[0][0].get_int();
    ++nopt;
  }

  opt = get_options ("bottom");
  if (opt.size()) {
    bottomN = opt[0][0].get_int();
    ++nopt;
  }

  if (nopt > 1) throw Exception ("too many conflicting options");


  bool invert = get_options("invert").size();
  bool use_NaN = get_options("nan").size();

  const Image::Header header_in (argument[0].get_image());
  assert (!header_in.is_complex());

  if (DataSet::voxel_count (header_in) < topN || DataSet::voxel_count (header_in) < bottomN)
    throw Exception ("number of voxels at which to threshold exceeds number of voxels in image");

  if (finite (percentile)) {
    percentile /= 100.0;
    if (percentile < 0.5) {
      bottomN = Math::round (DataSet::voxel_count (header_in) * percentile);
      invert = !invert;
    }
    else topN = Math::round (DataSet::voxel_count (header_in) * (1.0 - percentile));
  }

  Image::Header header (header_in);
  if (use_NaN) header.datatype() = DataType::Float32;
  else header.datatype() = DataType::Bit;

  const Image::Header header_out (argument[1].get_image (header));

  Image::Voxel<float> in (header_in);
  Image::Voxel<float> out (header_out);

  float zero = use_NaN ? NAN : 0.0;
  float one  = 1.0;
  if (invert) std::swap (zero, one);

  if (topN || bottomN) {
    std::multimap<float,std::vector<ssize_t> > list;

    {
      DataSet::Loop loop ("thresholding \"" + shorten (in.name()) + "\" at " + (
            isnan (percentile) ? 
            ( str (topN ? topN : bottomN) + "th " + (topN ? "top" : "bottom" ) + " voxel" ) : 
            (str (percentile*100.0) + "\% percentile") 
            ) + "...");

      if (topN) {
        for (loop.start (in); loop.ok(); loop.next (in)) { 
          float val = in.value();
          if (list.size() == topN) {
            if (val < list.begin()->first) continue;
            list.erase (list.begin());
          }
          std::vector<ssize_t> pos (in.ndim());
          for (size_t n = 0; n < in.ndim(); ++n) pos[n] = in[n];
          list.insert (std::pair<float,std::vector<ssize_t> > (val, pos));
        }
      }
      else {
        for (loop.start (in); loop.ok(); loop.next (in)) { 
          float val = in.value();
          if (list.size() == bottomN) {
            std::multimap<float,std::vector<ssize_t> >::iterator i = list.end();
            --i;
            if (val > i->first) continue;
            list.erase (i);
          }
          std::vector<ssize_t> pos (in.ndim());
          for (size_t n = 0; n < in.ndim(); ++n) pos[n] = in[n];
          list.insert (std::pair<float,std::vector<ssize_t> > (val, pos));
        }
      }
    }

    DataSet::Loop loop;
    for (loop.start (out); loop.ok(); loop.next (out)) {
      out.value() = zero;
    }

    for (std::multimap<float,std::vector<ssize_t> >::const_iterator i = list.begin(); i != list.end(); ++i) {
      for (size_t n = 0; n < out.ndim(); ++n)
        out[n] = i->second[n];
      out.value() = one;
    }
  }
  else {
    if (isnan (val)) {
      DataSet::Histogram<Image::Voxel<float> > hist (in);
      val = hist.first_min();
    }

    DataSet::Loop loop ("thresholding \"" + shorten (in.name()) + "\" at intensity " + str(val) + "...");
    for (loop.start (out, in); loop.ok(); loop.next (out, in)) {
      out.value() = in.value() < val ? zero : one;
    }
  }
}