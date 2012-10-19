// plstim/qexperiment.h – Manage a real experiment

#ifndef __PLSTIM_QEXPERIMENT_H
#define __PLSTIM_QEXPERIMENT_H

/*#include <cmath>
#include <map>
#include <string>
#include <vector>*/

// OpenGL ES 2.0 through EGL 2.0
#if 0
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#endif

#include <QApplication>
#include <QMainWindow>


#include "glwidget.h"
#include "utils.h"


namespace plstim
{
  class QExperiment : QObject
  {
  Q_OBJECT
  protected:
#if 0
    EGLNativeDisplayType nat_dpy;
    EGLNativeWindowType nat_win;

    EGLDisplay egl_dpy;
    EGLConfig config;
    EGLContext ctx;
    EGLSurface sur;
#endif

    /// Associated Qt application
    QApplication app;
    QMainWindow win;
    MyGLWidget* glwidget;

    /// Application settings
    QSettings* settings;

    /// Output display to be used
    std::string routput;

    /// Desired monitor refresh rate in Hertz
    float wanted_frequency;

    /// Trial duration in ms
    float dur_ms;

    /**
     * Minimal number of buffer swaps between frames.
     *
     * Set to 0 to disable synchronisation.
     */
    int swap_interval;

    /// Location of the current texture
    int texloc;

    /// Texture width (2ⁿ)
    int tex_width;

    /// Texture height (2ⁿ)
    int tex_height;

    /// Current trial number
    int current_trial;

  public:
    /// Number of trials in a session
    int ntrials;

    /// Number of frames per trial
    int nframes;

    /// Trial frames as OpenGL textures
    //GLuint* tframes;

    /// Special frames required by the protocol
    //std::map<std::string,GLuint> special_frames;

  protected:
#if 0
    bool make_native_window (EGLNativeDisplayType nat_dpy,
			     EGLDisplay egl_dpy,
			     int width, int height,
			     bool fullscreen,
			     const char* title,
			     EGLNativeWindowType *window,
			     EGLConfig *conf);
#endif

    void error (const std::string& msg);

    /**
     * Copy a pixel surface into an OpenGL texture.
     * The destination texture must have been already created
     * with gen_frame(), and its size should match the other
     * textures (tex_width×tex_height).  The source surface can be
     * destroyed after calling this function.
     */
    //bool copy_to_texture (const GLvoid* src, GLuint dest);

  public:

    QExperiment (int& argc, char** argv);
    virtual ~QExperiment ();

    virtual bool egl_init (int width, int height, bool fullscreen,
			   const std::string& title,
			   int texture_width, int texture_height);
 

    void egl_cleanup ();

    bool exec ();

    bool run_session ();

    /// Run a single trial
    virtual bool run_trial () = 0;

    /// Generate frames for a single trial
    virtual bool make_frames () = 0;

    /// Show the frames loaded in ‘tframes’
    bool show_frames ();

    /// Show a special frame
    bool show_frame (const std::string& frame_name);

    /// Define a special frame
    bool gen_frame (const std::string& frame_name);

    /// Clear the screen
    bool clear_screen ();

    /// Wait for any key to be pressed
    bool wait_any_key ();

    /// Wait for a key press in a given set
    /*bool wait_for_key (const std::vector<KeySym>& accepted_keys,
		       KeySym* pressed_key);*/

  protected slots:
    void setup_param_changed ();
    void update_setup ();
    void quit ();

  protected:
    bool save_setup;
    QComboBox* setup_cbox;
    QLineEdit* res_x_edit;
    QLineEdit* res_y_edit;
    QLineEdit* phy_width_edit;
    QLineEdit* phy_height_edit;
    QLineEdit* dst_edit;
    QLineEdit* lum_min_edit;
    QLineEdit* lum_max_edit;
    QLineEdit* refresh_edit;
  };
}

#endif

// vim:filetype=cpp: