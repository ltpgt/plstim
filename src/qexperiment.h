// plstim/qexperiment.h – Manage a real experiment

#ifndef __PLSTIM_QEXPERIMENT_H
#define __PLSTIM_QEXPERIMENT_H

#include <random>
#include <set>

#include <QApplication>
#include <QMainWindow>

#include <QtQml>

// Lua interpreter
#include <lua.hpp>

// HDF5 C++ library
#include <H5Cpp.h>

#ifdef HAVE_EYELINK
#include <core_expt.h>
#include "elcalibration.h"
#endif

#ifdef HAVE_POWERMATE
#include "powermate.h"
#endif // HAVE_POWERMATE

#include "qtypes.h"
#include "stimwindow.h"
#include "messagebox.h"
#include "utils.h"


namespace plstim
{
  class Page : public QObject
  {
    Q_OBJECT
    Q_PROPERTY (int frameCount READ frameCount WRITE setFrameCount)
    Q_PROPERTY (PaintTime paintTime READ paintTime WRITE setPaintTime)
  public:
    enum Type {
      SINGLE,
      FRAMES
    };
    Q_ENUMS (Type)
    enum PaintTime {
      EXPERIMENT,
      TRIAL
    };
    Q_ENUMS (PaintTime)
  public:
    Page::Type type;
    QString title;
    PaintTime paint_time;
    bool wait_for_key;
    std::set<int> accepted_keys;

    /// Duration of the animation in milliseconds
    float duration;

#ifdef HAVE_EYELINK
    /// Duration of the required fixation
    float fixation;
#endif // HAVE_EYELINK

#ifdef HAVE_POWERMATE
    /// Wait for a rotation answer
    bool waitRotation;
#endif // HAVE_POWERMATE

    /// Actual number of frames in the page
    int nframes;

  public:
    Page (Page::Type t, const QString& title);
    virtual ~Page ();
    void accept_key (int key);
  public:
    void make_active ();
    void emit_key_pressed (QKeyEvent* evt);
    int frameCount () const { return nframes; }
    void setFrameCount (int count) { nframes = count; }
    PaintTime paintTime () const { return paint_time; }
    void setPaintTime (PaintTime time) { paint_time = time; }
  signals:
    /**
     * Called when the page is the one currently displayed.
     */
    void page_active ();
  };

  class QExperiment : public QObject
  {
    Q_OBJECT
    Q_PROPERTY (float monitor_rate READ monitor_rate)
    Q_PROPERTY (int textureSize READ textureSize WRITE setTextureSize)
  public:
    /// Associated Qt application
    QApplication app;
  protected:
    QMainWindow* win;

    /// Stimulus OpenGL window
    StimWindow* stim;

    /// Application settings
    QSettings* settings;

    QDesktopWidget dsk;

    MyMessageBox* msgbox;
    //Message* res_msg;
    //Message* match_res_msg;

    /// Pages composing a trial
    std::vector<Page*> pages;

    /// Current page in a trial
    int current_page;

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
    int tex_size;

    /// Current trial number
    int current_trial;

    /// Accepted keys to switch to next page
    QSet<int> nextPageKeys;

  public:
    /// Number of trials in a session
    int ntrials;

    /// Trial frames as OpenGL textures
    //GLuint* tframes;

    /// Special frames required by the protocol
    //std::map<std::string,GLuint> special_frames;

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

    bool load_experiment (const QString& path);

  public slots:
    void unloadExperiment ();

    void abortExperiment ();

    void nextPage ();

  public:
    bool exec ();

    /// Add a page to the composition of a trial
    void add_page (Page* page);

    void run_trial ();

    void show_page (int index);

    /// Define a special frame
    bool gen_frame (const std::string& frame_name);

    /// Clear the screen
    bool clear_screen ();

    int textureSize () const { return tex_size; }
    void setTextureSize (int size) { tex_size = size; }

    //void set_glformat (QGLFormat glformat);

    float monitor_rate () const;

    //void set_swap_interval (int swap_interval);

  public slots:

    void new_setup ();
    void new_subject ();

    /**
     * Run a session of multiple trials.
     * Sessions may run asynchronously, so this method may return
     * before the session being finished.
     */
    void run_session ();
    void runSessionInline ();

    void set_trial_count (int ntrials);

    void set_subject_datafile (const QString& path);

  protected:
    void init_session ();

    void setup_updated ();

    void paintPage (QPage* page, QImage& img, QPainter& painter);

    bool check_lua (int retcode);

    void savePageParameter (const QString& pageTitle,
	                    const QString& paramName,
			    int paramValue);

  protected:
    bool creating_subject;

  protected slots:
    //void screen_param_changed ();
    void setup_param_changed ();
    void update_setup ();
    void updateRecents ();
    void about_to_quit ();
    void open ();
    void quit ();
    void stimKeyPressed (QKeyEvent* evt);
    //void stimScreenChanged (QScreen* screen);
#ifdef HAVE_POWERMATE
    void powerMateRotation (PowerMateEvent* evt);
    void powerMateButtonPressed (PowerMateEvent* evt);
#endif // HAVE_POWERMATE
    void open_recent ();
    void xp_param_changed (double);
    void new_setup_validated ();
    void new_setup_cancelled ();
    void new_subject_validated ();
    void new_subject_cancelled ();
    void select_subject_datafile ();
    void subject_changed (const QString& subject);

  protected:
    bool save_setup;
    QToolBar* toolbar;
    QToolBox* tbox;

    QWidget* setup_item;

    QWidget* xp_item;
    QSpinBox* ntrials_spin;
    std::map<QString,QDoubleSpinBox*> param_spins;

    QWidget* subject_item;
    QComboBox* subject_cbox;
    QPushButton* subject_databutton;

    QComboBox* setup_cbox;
    QSpinBox* screen_sbox;
  public:
    QSpinBox* off_x_edit;
    QSpinBox* off_y_edit;
    QSpinBox* res_x_edit;
    QSpinBox* res_y_edit;
  protected:
    QSpinBox* phy_width_edit;
    QSpinBox* phy_height_edit;
    QSpinBox* dst_edit;
    QSpinBox* lum_min_edit;
    QSpinBox* lum_max_edit;
    QSpinBox* refresh_edit;
    QLabel* xp_label;

    QSplitter* hsplitter;
    QTabWidget* logtab;

    bool session_running;

    QString xp_name;
    int session_number;

    QMenu* xp_menu;
    int max_recents;
    QAction** recent_actions;

    QAction* close_action;

    /// QML scripting engine to load the experiments
    QQmlEngine m_engine;
    /// QML component for the current experiment
    QQmlComponent* m_component;
    /// Currently loaded experiment
    plstim::Experiment* m_experiment;

    lua_State* lstate;

    /// Last opened directory in file dialog
    QString last_dialog_dir;

    QString last_datafile_dir;

  public:
  
    /// Random number generator
    std::mt19937 twister;

    /// Binary choice distribution
    std::uniform_int_distribution<int> bin_dist;

    /// Uniform double distribution in [0,1]
    std::uniform_real_distribution<double> real_dist;

    std::map<QString,int> key_mapping;

  public:

    /// Memory for the trial record
    void* trial_record;

    /// Memory size required by a trial record
    size_t record_size;

    /// Trial record offsets
    std::map<QString,size_t> trial_offsets;

    /// Page record offsets
    std::map<QString,std::map<QString,size_t>> record_offsets;

    /// Trial record types
    std::map<QString,H5::DataType> trial_types;

    // List of key accepted across the experiment
    std::set<QString> xp_keys;

    H5::CompType* record_type;

    H5::H5File* hf;
    H5::DataSet dset;

    QElapsedTimer timer;

  protected:

    QSpinBox* make_setup_spin (int min, int max, const char* suffix);

  public:
    void error (const QString& msg, const QString& desc="");

    QColor get_colour (const char* name) const;

#ifdef HAVE_EYELINK
  protected:
    bool eyelink_connected;
    bool eyelink_dummy;
    bool waiting_fixation;
  public:
    EyeLinkCalibrator* calibrator;
  public:
    void load_eyelink ();
    void calibrate_eyelink ();
    bool check_eyelink (INT16 errcode, const QString& func_name);
#endif
  };

  class MyLineEdit : public QLineEdit
  {
    Q_OBJECT
  public:
    MyLineEdit ()
      : QLineEdit ()
    {
    }
  signals: 
    void escapePressed ();
  protected:
    virtual void keyPressEvent (QKeyEvent* evt)
    {
      if (evt->key () == Qt::Key_Escape)
	emit escapePressed ();
      else
	QLineEdit::keyPressEvent (evt);
    }
  };
}

#endif

// vim:filetype=cpp:
