// egl-anim.cc – Stimulus presentation with OpenGL ES

#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef _WIN32
# include <windows.h>
#else
# include <X11/Xlib.h>
# include <X11/Xatom.h>
#endif

#include <EGL/egl.h>
#include <GLES2/gl2.h>


#include <png.h>


#include <iomanip>
#include <iostream>
#include <sstream>
using namespace std;

// Command line parsing
#include <tclap/CmdLine.h>


// If possible, use a clock not sensible to adjtime or ntp
// (Linux only)
#define CLOCK CLOCK_MONOTONIC_RAW

// Otherwise switch back to a clock at least not sensible to
// administrator clock settings
//#define CLOCK CLOCK_MONOTONIC


static bool
assert_gl_error (const std::string& msg)
{
  auto ret = glGetError ();
  if (ret != GL_NO_ERROR) {
    cerr << "could not " << msg
	 << " (" << hex << ret << dec << ')' << endl;
    exit(1);
  }
  return true;
}


static EGLNativeDisplayType
open_native_display (void* param)
{
#ifdef _WIN32
  return (EGLNativeDisplayType) 0;
#else
  return (EGLNativeDisplayType) XOpenDisplay ((char*) param);
#endif
}


static int
make_native_window (EGLNativeDisplayType nat_dpy, EGLDisplay egl_dpy,
		    int width, int height, const char* title,
		    EGLNativeWindowType *window, EGLConfig *conf)
{
  // Required characteristics
  EGLint egl_attr[] = {
    EGL_BUFFER_SIZE, 24,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };

  // Find a matching config
  EGLConfig config;
  EGLint num_egl_configs;
  if (! eglChooseConfig (egl_dpy, egl_attr, &config,
			 1, &num_egl_configs)
      || num_egl_configs != 1
      || config == 0) {
    fprintf (stderr, "could not choose an EGL config\n");
    return 0;
  }

#ifdef _WIN32
#else
  // Get the default screen and root window
  Display *dpy = (Display*) nat_dpy;
  Window root = DefaultRootWindow (dpy);
  Window scrnum = DefaultScreen (dpy);
#endif

  // Fetch visual information for the configuration
  EGLint vid;
  int num_visuals;
  if (! eglGetConfigAttrib (egl_dpy, config,
			    EGL_NATIVE_VISUAL_ID, &vid)) {
    fprintf (stderr, "could not get native visual id\n");
    return 0;
  }
#ifdef _WIN32
#else
  XVisualInfo xvis;
  XVisualInfo *xvinfo;
  xvis.visualid = vid;
  xvinfo = XGetVisualInfo (dpy, VisualIDMask, &xvis, &num_visuals);
  if (num_visuals == 0) {
    fprintf (stderr, "could not get the X visuals\n");
    return 0;
  }

  // Set the window attributes
  XSetWindowAttributes xattr;
  unsigned long mask;
  Window win;
  xattr.background_pixel = 0;
  xattr.border_pixel = 0;
  xattr.colormap = XCreateColormap (dpy, root,
				    xvinfo->visual, AllocNone);
  xattr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
  mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
  win = XCreateWindow (dpy, root, 0, 0, width, height, 0,
			xvinfo->depth, InputOutput, xvinfo->visual,
			mask, &xattr);
  *window = (EGLNativeWindowType) win;

  // Set X hints and properties
  XStoreName (dpy, win, title);

  XMapWindow (dpy, win);
  XFree (xvinfo);
#endif

  // Make the window fullscreen
#if 1
  bool fullscreen = true;
  if (fullscreen) {
    Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom fs_atm = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = fs_atm;
    xev.xclient.data.l[2] = 0;
    XSendEvent (dpy, DefaultRootWindow (dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
  }
#endif

  *conf = config;
  return 1;
}


int
main (int argc, char* argv[])
{
  int res;

  EGLNativeDisplayType nat_dpy;
  EGLNativeWindowType nat_win;

  EGLDisplay egl_dpy;
  EGLConfig config;
  EGLint egl_maj, egl_min;

  int i;
  unsigned int width, height;

  std::vector<std::string> images;
  EGLint swap_interval;

  try {
    // Setup the command line parser
    TCLAP::CmdLine cmd ("Display and record stimuli", ' ', "0.1");
    TCLAP::ValueArg<string> arg_geom ("g", "geometry",
	"Stimulus dimension in pixels", true, "", "WIDTHxHEIGHT");
    cmd.add (arg_geom);
    TCLAP::ValueArg<int> arg_swap ("s", "swap-interval",
	"Swap interval in frames", false, 1, "INTERVAL");
    cmd.add (arg_swap);
    TCLAP::UnlabeledMultiArg<string> arg_images ("images",
						 "list of images",
						 true, "IMAGE");
    cmd.add (arg_images);
    // Parser the command line arguments
    cmd.parse (argc, argv);

    // Parse the geometry
    auto geom = arg_geom.getValue ();
    size_t sep = geom.find ('x');
    if (sep == geom.npos) {
      cerr << "error: invalid stimulus dimension: " << geom << endl;
      return 1;
    }
    width = atof (geom.c_str ());
    height = atof (geom.c_str () + sep + 1);
    if (width == 0 || height == 0) {
      cerr << "error: invalid stimulus dimension: " << geom << endl;
      return 1;
    }

    images = arg_images.getValue ();
    swap_interval = arg_swap.getValue ();
  } catch (TCLAP::ArgException& e) {
    cerr << "error: could not parse argument "
         << e.argId () << ": " << e.error () << endl;
  }

#ifdef _WIN32
#else
  struct timespec tp_start, tp_stop;

  // Get the clock resolution
  res = clock_getres (CLOCK, &tp_start);
  if (res < 0) {
    perror ("clock_getres");
    return 1;
  }
  printf ("clock %d resolution: %lds %ldns\n",
	  CLOCK, tp_start.tv_sec, tp_start.tv_nsec);
#endif

  // Setup X11
  nat_dpy = open_native_display (NULL);
#if 0
  if (dpy == NULL) {
    fprintf (stderr, "could not open X display\n");
    return 1;
  }
#endif

  // Setup EGL
  egl_dpy = eglGetDisplay (nat_dpy);
  if (egl_dpy == EGL_NO_DISPLAY) {
    fprintf (stderr, "could not get an EGL display\n");
    return 1;
  }
  if (! eglInitialize (egl_dpy, &egl_maj, &egl_min)) {
    fprintf (stderr, "could not init the EGL display\n");
    return 1;
  }
  printf ("EGL version %d.%d\n", egl_maj, egl_min);

  // Create a system window
  int ret = make_native_window (nat_dpy, egl_dpy,
				width, height, argv[0],
				&nat_win, &config);
  if (ret == 0)
    return 1;

  eglBindAPI(EGL_OPENGL_ES_API);

  // Create an OpenGL ES 2 context
  static const EGLint ctx_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };
  EGLContext ctx = eglCreateContext (egl_dpy, config,
				     EGL_NO_CONTEXT, ctx_attribs);
  if (ctx == EGL_NO_CONTEXT) {
    fprintf (stderr, "could not create an EGL context\n");
    return 1;
  }
  {
    EGLint val;
    eglQueryContext(egl_dpy, ctx, EGL_CONTEXT_CLIENT_VERSION, &val);
    assert(val == 2);
  }

  // Create the EGL surface associated with the window
  EGLSurface sur = eglCreateWindowSurface (egl_dpy, config,
					   nat_win, NULL);
  if (sur == EGL_NO_SURFACE) {
    fprintf (stderr, "could not create an EGL surface\n");
    return 1;
  }

  // Set the current EGL context
  eglMakeCurrent (egl_dpy, sur, sur, ctx);

  // Display OpenGL ES information
  printf ("OpenGL ES version  %s by %s (%s) with: %s\n",
	  glGetString (GL_VERSION),
	  glGetString (GL_VENDOR),
	  glGetString (GL_RENDERER),
	  glGetString (GL_EXTENSIONS));

  //glClearColor (0.1, 0.1, 0.1, 1.0);
  glClearColor (0, 0, 0, 0);
  //glViewport (0, 0, width, height);

  // Load the frames as textures
  int nframes = images.size ();
  printf ("Creating %d texture frames\n", nframes);
  auto tframes = new GLuint[nframes];
  glGenTextures (nframes, tframes);
  assert_gl_error ("generate textures");

  int prev_twidth = -1, prev_theight;
  int twidth, theight;
  // ...
  for (int i = 0; i < nframes; i++) {
    // Load the image as a PNG file
    GLubyte* data;
    GLint gltype;
    if (! load_png_image (images[i].c_str (), &twidth, &theight,
			  &gltype, &data))
      return 1;

    // Make sure the texture sizes match
    if (prev_twidth >= 0
	&& (prev_twidth != twidth || prev_theight != theight)) {
      fprintf (stderr, "error: texture sizes do not match\n");
      return 1;
    }
    prev_twidth = twidth;
    prev_theight = theight;

    glBindTexture (GL_TEXTURE_2D, tframes[i]);
    assert_gl_error ("bind a texture");
    cout << "texture size: " << twidth << "×" << theight << endl
         << "texture type: " << hex << gltype << dec << endl;
    glTexImage2D (GL_TEXTURE_2D, 0, gltype, twidth, theight,
		  0, gltype, GL_UNSIGNED_BYTE, data);
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    assert_gl_error ("set texture data");

    // Erase the memory, OpenGL has its own copy
    delete [] data;
  }
  cout << nframes << " frames loaded" << endl;


  // Create a fragment shader for the texture
  stringstream ss;
  static const char *fshader_txt = 
    "varying mediump vec2 tex_coord;\n"
    "uniform sampler2D texture;\n"
    "void main() {\n"
    //"  gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "  gl_FragColor = texture2D(texture, tex_coord);\n"
    //"  gl_FragColor = vec4(tex_coord.x, tex_coord.y, 0, 1);\n"
    "}\n";
  GLuint fshader = glCreateShader (GL_FRAGMENT_SHADER);
  assert_gl_error ("create a fragment shader");
  if (fshader == 0) {
    fprintf (stderr, "could not create a fragment shader (0x%x)\n",
	     glGetError ());
    return 1;
  }
  glShaderSource (fshader, 1, (const char **) &fshader_txt, NULL);
  glCompileShader (fshader);
  GLint stat;
  glGetShaderiv (fshader, GL_COMPILE_STATUS, &stat);
  if (stat == 0) {
    GLsizei len;
    char log[1024];
    glGetShaderInfoLog (fshader, 1024, &len, log);
    cerr << "could not compile the fragment shader:" << endl
         << log << endl;
    return 1;
  }

  // Offset to center the stimulus
  GLfloat offx = (width-twidth)/2.0f;
  GLfloat offy = (height-theight)/2.0f;

  float tx_conv = twidth / (2.0f * width);
  float ty_conv = theight / (2.0f * height);

  tx_conv = 1/2.0f;
  ty_conv = 1/2.0f;

  // Create an identity vertex shader
  ss.str("");
  ss << fixed << setprecision(12)
     // This projection matrix maps OpenGL coordinates
     // to device coordinates (pixels)
     << "const mat4 proj_matrix = mat4("
     << (2.0/width) << ", 0.0, 0.0, -1.0," << endl
     << "0.0, " << -(2.0/height) << ", 0.0, 1.0," << endl
     << "0.0, 0.0, -1.0, 0.0," << endl
     << "0.0, 0.0, 0.0, 1.0);" << endl
     << "attribute vec2 ppos;" << endl
     << "varying vec2 tex_coord;" << endl
     << "void main () {" << endl
     //<< "  gl_Position = vec4(ppos.x, ppos.y, 0.0, 1.0) * proj_matrix;" << endl
     << "  gl_Position = vec4(ppos.x, ppos.y, 0.0, 1.0) * proj_matrix;" << endl
     //<< "  gl_Position = vec4(ppos.x, ppos.y, 0.0, 1.0);" << endl
     << "  tex_coord = vec2((ppos.x-" << offx << ")/" << (float) twidth 
     << ", (ppos.y-" << offy << ")/" << (float) theight << " );" << endl
     //<< "  tex_coord = vec2((gl_Position.x+1.0)*" << tx_conv << ", (-gl_Position.y+1.0)*" << ty_conv << ");" << endl
     //<< "  tex_coord = vec2((gl_Position.x+1.0)/2.0, (-gl_Position.y+1.0)/2.0);" << endl
     << "}" << endl;
  auto vshader_str = ss.str();
  cout << "vertex shader:" << endl << vshader_str << endl;
#if 0
  static const char *vshader_txt =
    "attribute vec2 ppos;\n"
    "void main() {\n"
    "  gl_Position = vec4(ppos.x, ppos.y, 0.0, 1.0);\n"
    "}\n";
#endif
  GLuint vshader = glCreateShader (GL_VERTEX_SHADER);
  if (vshader == 0) {
    fprintf (stderr, "could not create a vertex shader (0x%x)\n",
	     glGetError ());
    return 1;
  }
  assert_gl_error ("create a vertex shader");

  const char* vshader_txt = vshader_str.c_str();
  glShaderSource (vshader, 1, (const char **) &vshader_txt, NULL);
  glCompileShader (vshader);
  glGetShaderiv (vshader, GL_COMPILE_STATUS, &stat);
  if (stat == 0) {
    GLsizei len;
    char log[1024];
    glGetShaderInfoLog (vshader, 1024, &len, log);
    cerr << "could not compile the vertex shader:" << endl
         << log << endl;
    return 1;
  }

  // Add the fragment shader to the current program
  GLuint program = glCreateProgram ();
  glAttachShader (program, fshader);
  glAttachShader (program, vshader);
  glLinkProgram (program);
  glGetProgramiv (program, GL_LINK_STATUS, &stat);
  if (stat == 0) {
    GLsizei len;
    char log[1024];
    glGetProgramInfoLog (program, 1024, &len, log);
    fprintf (stderr,
	     "could not link the shaders:\n%s\n", log);
    return 1;
  }
  glUseProgram (program);
  assert_gl_error ("use the shaders program");

  int ppos = glGetAttribLocation (program, "ppos");
  if (ppos == -1) {
    fprintf (stderr, "could not get attribute ‘ppos’\n");
    return 1;
  }

  // Create the vertex buffer
#if 0
  static GLfloat vertices[] = {
    -1, -1,
    -1,  1,
     1,  1,

     1,  1,
    -1, -1,
     1, -1
  };
#else

  static GLfloat vertices[] = {
#if 1
#if 1
    offx, offy,
    offx, offy + theight,
    offx + twidth, offy + theight,

    offx + twidth, offy + theight,
    offx, offy,
    offx + twidth, offy
#else
    10.0f, 10.0f,
    10.0f, (GLfloat) height - 10.0f,
    (GLfloat) width - 10.0f, (GLfloat) height - 10.0f,

    (GLfloat) width - 10.0f, (GLfloat) height - 10.0f,
    10.0f, 10.0f,
    (GLfloat) width - 10.0f, 10.0f
#endif
#else
    4, 4,
    100, 4,
    4, 12
#endif
  };
#endif
  /*GLuint vbuf;
  glGenBuffers (1, &vbuf);
  glBindBuffer (GL_ARRAY_BUFFER, vbuf);
  glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);
*/
  glEnableVertexAttribArray (ppos);
  glVertexAttribPointer (ppos, 2, GL_FLOAT, GL_FALSE, 0, vertices);
  glClear (GL_COLOR_BUFFER_BIT);

  glActiveTexture (GL_TEXTURE0);

  int texloc = glGetUniformLocation (program, "texture");
  if (texloc == -1) {
    cerr << "could not get location of ‘texture’" << endl;
    return 1;
  }
  assert_gl_error ("get location of uniform ‘texture’");


  // Wait for a key press before running the trial
#ifdef _WIN32
#else
  bool pressed = false;
  XEvent evt;
  while (! pressed) {
    XNextEvent ((Display*) nat_dpy, &evt);
    if (evt.type == KeyPress)
      pressed = true;
  }
#endif

  //cout << "Starting" << endl;
#ifdef _WIN32
#else
  // Mark the beginning time
  res = clock_gettime (CLOCK, &tp_start);
#endif

  // Trial stimulus presentation
  for (i = 0; i < nframes; i++) {
    glBindTexture (GL_TEXTURE_2D, tframes[i%nframes]);
    glUniform1i (texloc, 0);

    glDrawArrays (GL_TRIANGLES, 0, 6);
    
    // Set the swap interval
    if (swap_interval != 1)
      eglSwapInterval (egl_dpy, swap_interval);
    eglSwapBuffers (egl_dpy, sur);

    //nanosleep (1);
  }

#ifdef _WIN32
#else
  // Mark the end time
  res = clock_gettime (CLOCK, &tp_stop);

  // Display the elapsed time
  printf ("start: %lds %ldns\n",
	  tp_start.tv_sec, tp_start.tv_nsec);
  printf ("stop:  %lds %ldns\n",
	  tp_stop.tv_sec, tp_stop.tv_nsec);
#endif

  // Wait for a key press at the end of the trial
  glClear (GL_COLOR_BUFFER_BIT);
  eglSwapBuffers (egl_dpy, sur);
#ifdef _WIN32
#else
  pressed = false;
  bool answer_up;
  KeySym keysym;
  XComposeStatus compose;
  const int buflen = 10;
  char buf[buflen];
  while (! pressed) {
    XNextEvent ((Display*) nat_dpy, &evt);
    if (evt.type == KeyPress) {
      int count = XLookupString (&evt.xkey, buf, buflen,
				 &keysym, &compose);
      //auto keycode = evt.xkey.keycode;
      if (keysym == XK_Up) {
	pressed = true;
	answer_up = true;
      }
      else if (keysym == XK_Down) {
	pressed = true;
	answer_up = false;
      }
    }
  }

  cout << "Answer: " << (answer_up ? "up" : "down") << endl;
#endif

  // Cleanup
  eglDestroyContext (egl_dpy, ctx);
  eglDestroySurface (egl_dpy, sur);
  eglTerminate (egl_dpy);

#ifdef _WIN32
#else
  Display *dpy = (Display*) nat_dpy;
  XDestroyWindow (dpy, (Window) nat_win);
  XCloseDisplay (dpy);
#endif

  return 0;
}
// vim:sw=2: