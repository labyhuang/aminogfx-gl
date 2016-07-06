#ifndef _AMINOGFX_H
#define _AMINOGFX_H

//FIXME
#define DEBUG_RESOURCES true

#include "freetype-gl.h"
#include "vertex-buffer.h"
#include <nan.h>

#ifdef MAC
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <sys/time.h>

//return the current time in msec
static double getTime(void) {
    timeval time;

    gettimeofday(&time, NULL);

    double millis = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);

    return millis;
}

#endif

#ifdef KLAATU
#include <ui/DisplayInfo.h>
#include <ui/FramebufferNativeWindow.h>
#include <gui/SurfaceComposerClient.h>

//return the current time in msec
static double getTime(void) {
    struct timespec res;
    clock_gettime(CLOCK_REALTIME, &res);
    return 1000.0 * res.tv_sec + ((double) res.tv_nsec / 1e6);
}
#endif

#ifdef LINUX
#include <GL/glfw.h>
#include <GL/glext.h>
#include <sys/time.h>
//return the current time in msec
static double getTime(void) {
    timeval time;

    gettimeofday(&time, NULL);

    double millis = (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);

    return millis;
}
#endif

#ifdef RPI

#include "GLES/gl.h"
#include "GLES2/gl2.h"
#include "GLES2/gl2ext.h"

//return the current time in msec
static double getTime(void) {
    struct timespec res;
    clock_gettime(CLOCK_REALTIME, &res);
    return 1000.0 * res.tv_sec + ((double) res.tv_nsec / 1e6);
}

#endif

#include <map>

/**
 * Font class.
 */
class AminoFont {
public:
    int id;

    //font
    texture_atlas_t *atlas;
    std::map<int, texture_font_t *> fonts; //by font size
    const char *filename;

    //shader
    GLuint shader;
    GLint texuni;
    GLint mvpuni;
    GLint transuni;
    GLint coloruni;

    AminoFont() {
        texuni = -1;
    }

    virtual ~AminoFont() {
        //destroy (if not called before)
        destroy();
    }

    /**
     * Free all resources.
     */
    virtual void destroy() {
        //textures
        for (std::map<int, texture_font_t *>::iterator it = fonts.begin(); it != fonts.end(); it++) {
            if (DEBUG_RESOURCES) {
                printf("freeing font texture\n");
            }

            texture_font_delete(it->second);
        }

        fonts.clear();

        //atlas
        if (atlas) {
            if (DEBUG_RESOURCES) {
                printf("freeing font\n");
            }

            texture_atlas_delete(atlas);
            atlas = NULL;
        }
    }
};

#endif
