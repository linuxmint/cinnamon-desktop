#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

int main (int argc, char **argv)
{
  Display *dpy = XOpenDisplay (NULL);
  
  Window win;
  int attribSingle[] = {
    GLX_RGBA,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    None };
  
  int attribDouble[] = {
    GLX_RGBA,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_DOUBLEBUFFER,
    None };
  
  XSetWindowAttributes attr;
  unsigned long mask;
  GLXContext ctx = NULL;
  XVisualInfo *visinfo;
  
  int exit_status = EXIT_SUCCESS;
  
  GLint max_texture_size = 0;

  if (!dpy) {
    /* We have, for some reason, been unable to connect to X
     * Bail cleanly, and leave a little note */
    fprintf (stderr, "check_gl_texture_size: Unable to open display %s", getenv("DISPLAY"));
    exit (EXIT_FAILURE);
  }
  
  visinfo = glXChooseVisual (dpy, DefaultScreen (dpy), attribSingle);
  if (!visinfo)
    visinfo = glXChooseVisual (dpy, DefaultScreen (dpy), attribDouble);
  
  if (visinfo)
    ctx = glXCreateContext (dpy, visinfo, NULL, GL_TRUE);
  
  if (!visinfo) {
    exit_status = EXIT_FAILURE;
    goto child_out;
  }
  
  if (!ctx) {
    XFree (visinfo);
    exit_status = EXIT_FAILURE;
    goto child_out;
  }
  
  attr.background_pixel = 0;
  attr.border_pixel = 0;
  attr.colormap = XCreateColormap (dpy, DefaultRootWindow (dpy),
				   visinfo->visual, AllocNone);
  attr.event_mask = StructureNotifyMask | ExposureMask;
  mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
  win = XCreateWindow (dpy, DefaultRootWindow (dpy), 0, 0, 100, 100,
		       0, visinfo->depth, InputOutput,
		       visinfo->visual, mask, &attr);
  
  if (!glXMakeCurrent (dpy, win, ctx)) {
    exit_status = EXIT_FAILURE;
    goto child_out;
  }

  glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_texture_size);
  
  printf ("%u", max_texture_size);

child_out:
  XCloseDisplay (dpy);
  exit (exit_status);
}
