/*
   Copyright 2013 Brain Research Institute, Melbourne, Australia

   Written by David Raffelt 10/04/2013

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

#include "mrtrix.h"
#include "gui/mrview/window.h"
#include "gui/mrview/mode/base.h"
#include "gui/mrview/tool/screen_capture.h"


namespace MR
{
  namespace GUI
  {
    namespace MRView
    {
      namespace Tool
      {


        ScreenCapture::ScreenCapture (Window& main_window, Dock* parent) :
          Base (main_window, parent)
        {
          VBoxLayout* main_box = new VBoxLayout (this);

          QGroupBox* rotate_group_box = new QGroupBox ("Rotate");
          GridLayout* rotate_layout = new GridLayout;
          rotate_layout->setContentsMargins (5, 5, 5, 5);
          rotate_layout->setSpacing (5);
          main_box->addWidget (rotate_group_box);
          rotate_group_box->setLayout (rotate_layout);

          rotate_layout->addWidget (new QLabel ("Axis X"), 0, 0);
          rotation_axis_x = new AdjustButton (this);
          rotate_layout->addWidget (rotation_axis_x, 0, 1);
          rotation_axis_x->setValue (0.0);
          rotation_axis_x->setRate (0.1);

          rotate_layout->addWidget (new QLabel ("Axis Y"), 1, 0);
          rotation_axis_y = new AdjustButton (this);
          rotate_layout->addWidget (rotation_axis_y, 1, 1);
          rotation_axis_y->setValue (0.0);
          rotation_axis_y->setRate (0.1);

          rotate_layout->addWidget (new QLabel ("Axis Z"), 2, 0);
          rotation_axis_z = new AdjustButton (this);
          rotate_layout->addWidget (rotation_axis_z, 2, 1);
          rotation_axis_z->setValue (1.0);
          rotation_axis_z->setRate (0.1);

          rotate_layout->addWidget (new QLabel ("Angle"), 3, 0);
          degrees_button = new AdjustButton (this);
          rotate_layout->addWidget (degrees_button, 3, 1);
          degrees_button->setValue (0.0);
          degrees_button->setRate (0.1);

          QGroupBox* translate_group_box = new QGroupBox ("Translate");
          GridLayout* translate_layout = new GridLayout;
          translate_layout->setContentsMargins (5, 5, 5, 5);
          translate_layout->setSpacing (5);
          main_box->addWidget (translate_group_box);
          translate_group_box->setLayout (translate_layout);

          translate_layout->addWidget (new QLabel ("Axis X"), 0, 0);
          translate_x = new AdjustButton (this);
          translate_layout->addWidget (translate_x, 0, 1);
          translate_x->setValue (0.0);
          translate_x->setRate (0.1);

          translate_layout->addWidget (new QLabel ("Axis Y"), 1, 0);
          translate_y = new AdjustButton (this);
          translate_layout->addWidget (translate_y, 1, 1);
          translate_y->setValue (0.0);
          translate_y->setRate (0.1);

          translate_layout->addWidget (new QLabel ("Axis Z"), 2, 0);
          translate_z = new AdjustButton (this);
          translate_layout->addWidget (translate_z, 2, 1);
          translate_z->setValue (0.0);
          translate_z->setRate (0.1);

          QGroupBox* volume_group_box = new QGroupBox ("Volume");
          GridLayout* volume_layout = new GridLayout;
          volume_layout->setContentsMargins (5, 5, 5, 5);
          volume_layout->setSpacing (5);
          main_box->addWidget (volume_group_box);
          volume_group_box->setLayout (volume_layout);

          volume_layout->addWidget (new QLabel ("Axis"), 0, 0);
          volume_axis = new QSpinBox (this);
          volume_axis->setMinimum (3);
          volume_axis->setValue (3);
          volume_layout->addWidget (volume_axis, 0, 1);

          volume_layout->addWidget (new QLabel ("Target"), 1, 0);
          target_volume = new AdjustButton (this);
          volume_layout->addWidget (target_volume, 1, 1);
          target_volume->setValue (0.0);
          target_volume->setRate (0.1);

          QGroupBox* FOV_group_box = new QGroupBox ("FOV");
          GridLayout* FOV_layout = new GridLayout;
          FOV_layout->setContentsMargins (5, 5, 5, 5);
          FOV_layout->setSpacing (5);
          main_box->addWidget (FOV_group_box);
          FOV_group_box->setLayout (FOV_layout);

          FOV_layout->addWidget (new QLabel ("Multiplier"), 0, 0);
          FOV_multipler = new AdjustButton (this);
          FOV_layout->addWidget (FOV_multipler, 0, 1);
          FOV_multipler->setValue (1.0);
          FOV_multipler->setRate (0.01);


          QGroupBox* output_group_box = new QGroupBox ("Output");
          main_box->addWidget (output_group_box);
          GridLayout* output_grid_layout = new GridLayout;
          output_group_box->setLayout (output_grid_layout);

          output_grid_layout->addWidget (new QLabel ("Prefix"), 0, 0);
          prefix_textbox = new QLineEdit ("screenshot", this);
          output_grid_layout->addWidget (prefix_textbox, 0, 1);
          connect (prefix_textbox, SIGNAL (editingFinished()), this, SLOT (on_output_update()));

          folder_button = new QPushButton ("Select output folder", this);
          folder_button->setToolTip (tr ("Output Folder"));
          connect (folder_button, SIGNAL (clicked()), this, SLOT (select_output_folder_slot ()));
          output_grid_layout->addWidget (folder_button, 1, 0, 1, 2);

          QGroupBox* capture_group_box = new QGroupBox ("Capture");
          main_box->addWidget (capture_group_box);
          GridLayout* capture_grid_layout = new GridLayout;
          capture_group_box->setLayout (capture_grid_layout);

          capture_grid_layout->addWidget (new QLabel ("Start Index"), 0, 0);
          start_index = new QSpinBox (this);
          start_index->setMinimum (0);
          start_index->setMaximum (std::numeric_limits<int>::max());
          start_index->setValue (0);
          capture_grid_layout->addWidget (start_index, 0, 1);

          capture_grid_layout->addWidget (new QLabel ("Frames"), 1, 0);
          frames = new QSpinBox (this);
          frames->setMinimum (0);
          frames->setMaximum (std::numeric_limits<int>::max());
          frames->setValue (1);
          capture_grid_layout->addWidget (frames, 1, 1);

          QPushButton* preview = new QPushButton ("Preview", this);
          connect (preview, SIGNAL (clicked()), this, SLOT (on_screen_preview()));
          capture_grid_layout->addWidget (preview, 2, 0, 1, 2);

          QPushButton* capture = new QPushButton ("Record", this);
          connect (capture, SIGNAL (clicked()), this, SLOT (on_screen_capture()));
          capture_grid_layout->addWidget (capture, 3, 0, 1, 2);

          main_box->addStretch ();

          directory = new QDir();
        }





        void ScreenCapture::on_screen_preview () { run (false); }

        void ScreenCapture::on_screen_capture () { run (true); }


        void ScreenCapture::run (bool with_capture) 
        {
          if (!window.image())
            return;

          Image::VoxelType& vox (window.image()->interp);

          if (std::isnan (rotation_axis_x->value()))
            rotation_axis_x->setValue (0.0);
          if (std::isnan (rotation_axis_y->value()))
            rotation_axis_y->setValue (0.0);
          if (std::isnan (rotation_axis_z->value()))
            rotation_axis_z->setValue (0.0);
          if (std::isnan (degrees_button->value()))
            degrees_button->setValue (0.0);

          if (std::isnan (translate_x->value()))
            translate_x->setValue(0.0);
          if (std::isnan (translate_y->value()))
            translate_y->setValue(0.0);
          if (std::isnan (translate_z->value()))
            translate_z->setValue(0.0);

          if (std::isnan (target_volume->value()))
            target_volume->setValue(0.0);

          if (volume_axis->value() >= ssize_t (vox.ndim()))
            volume_axis->setValue (vox.ndim()-1);

          if (target_volume->value() >= vox.dim(volume_axis->value()))
            target_volume->setValue (vox.dim(volume_axis->value())-1);

          if (std::isnan (FOV_multipler->value()))
            FOV_multipler->setValue(1.0);

          if (window.snap_to_image () && degrees_button->value() > 0.0)
            window.set_snap_to_image (false);
          float radians = degrees_button->value() * (Math::pi / 180.0) / frames->value();
          float volume = vox[volume_axis->value()];
          float volume_inc = (target_volume->value() - volume) / frames->value();
          std::string folder (directory->path().toUtf8().constData());
          std::string prefix (prefix_textbox->text().toUtf8().constData());
          int first_index = start_index->value();
          int i = first_index;


          for (; i < first_index + frames->value(); ++i) {
            if (with_capture) 
              this->window.captureGL (folder + "/" + prefix + printf ("%04d.png", i));

            // Rotation
            Math::Versor<float> orientation (this->window.orientation());
            Math::Vector<float> axis (3);
            axis[0] = rotation_axis_x->value();
            axis[1] = rotation_axis_y->value();
            axis[2] = rotation_axis_z->value();
            Math::Versor<float> rotation (radians, axis.ptr());
            orientation *= rotation;
            this->window.set_orientation (orientation);

            // Translation
            Point<float> focus (this->window.focus());
            focus[0] += translate_x->value() / frames->value();
            focus[1] += translate_y->value() / frames->value();
            focus[2] += translate_z->value() / frames->value();
            window.set_focus (focus);
            Point<float> target (this->window.target());
            target[0] += translate_x->value() / frames->value();
            target[1] += translate_y->value() / frames->value();
            target[2] += translate_z->value() / frames->value();
            window.set_target (target);

            // Volume
            volume += volume_inc;
            window.set_image_volume (volume_axis->value(), std::round (volume));

            // FOV
            window.set_FOV (window.FOV() * (std::pow (FOV_multipler->value(), (float) 1.0 / frames->value())));

            start_index->setValue (i + 1);
            this->window.updateGL();
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
          }
        }






        void ScreenCapture::select_output_folder_slot ()
        {
          directory->setPath(QFileDialog::getExistingDirectory (this, tr("Directory"), directory->path()));
          QString path (shorten(directory->path().toUtf8().constData(), 20, 0).c_str());
          if (!path.size()) return;
          folder_button->setText(path);
          on_output_update ();
        }






        void ScreenCapture::on_output_update () {
          start_index->setValue (0);
        }






        bool ScreenCapture::process_batch_command (const std::string& cmd, const std::string& args)
        {
          // BATCH_COMMAND capture.folder path # Set the output folder for the screen capture tool
          if (cmd == "capture.folder") {
            directory->setPath (args.c_str());
            QString path (shorten(directory->path().toUtf8().constData(), 20, 0).c_str());
            folder_button->setText(path);
            on_output_update ();
            return true;
          }

          // BATCH_COMMAND capture.prefix path # Set the output file prefix for the screen capture tool
          if (cmd == "capture.prefix") {
            prefix_textbox->setText (args.c_str());
            on_output_update ();
            return true;
          }

          // BATCH_COMMAND capture.grab # Start the screen capture process
          if (cmd == "capture.grab") {
            on_screen_capture();
            return true;
          }

          return false;
        }


      }
    }
  }
}






