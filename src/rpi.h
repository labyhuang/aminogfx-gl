#ifndef _AMINO_RPI_H
#define _AMINO_RPI_H

#include "base.h"
#include "renderer.h"
#include "rpi_video.h"

//VideoCore
#include "bcm_host.h"

//Dispmanx
#ifdef EGL_DISPMANX
#include "interface/vchiq_arm/vchiq_if.h"
#endif

//GBM
#ifdef EGL_GBM
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#endif

#include <semaphore.h>
#include <linux/input.h>

class AminoGfxRPiFactory : public AminoJSObjectFactory {
public:
    AminoGfxRPiFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Raspberry Pi AminoGfx implementation.
 */
class AminoGfxRPi : public AminoGfx {
public:
    AminoGfxRPi();
    ~AminoGfxRPi();

    static AminoGfxRPiFactory* getFactory();
    static NAN_MODULE_INIT(Init);

    EGLImageKHR createEGLImage(GLuint textureId);
    void destroyEGLImage(EGLImageKHR eglImage);

private:
    static bool glESInitialized;

    //OpenGL ES
    NativeDisplayType displayType = EGL_DEFAULT_DISPLAY;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLConfig config;
    uint32_t screenW = 0;
    uint32_t screenH = 0;

    //DRM/GBM
#ifdef EGL_GBM
    int driDevice = 0;
    uint32_t connector_id = 0;
    drmModeModeInfo mode_info;
    gbm_surface *gbmSurface = NULL;
    drmModeCrtc *crtc = NULL;
    gbm_bo *previous_bo = NULL;
    uint32_t previous_fb = 0;
#endif
    //resolution
    static sem_t resSem;
    static bool resSemValid;

    //input
    std::vector<int> fds;
    int mouse_x = 0;
    int mouse_y = 0;

    static NAN_METHOD(New);

    void setup() override;
    void initEGL();

    static TV_DISPLAY_STATE_T* getDisplayState();
    static void tvservice_cb(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2);

    void destroy() override;
    void destroyAminoGfxRPi();

    bool getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) override;
    void getStats(v8::Local<v8::Object> &obj) override;

//cbxx use again
//#ifdef EGL_DISPMANX
    void forceHdmiMode(uint32_t code);
    void switchHdmiOff();
//#endif

    void populateRuntimeProperties(v8::Local<v8::Object> &obj) override;
    void initRenderer() override;

#ifdef EGL_DISPMANX
    EGLSurface createDispmanxSurface();
#endif

#ifdef EGL_GBM
    EGLSurface createGbmSurface();
#endif

    bool startsWith(const char *pre, const char *str);
    void initInput();

    void start() override;
    bool bindContext() override;
    void renderingDone() override;
    void handleSystemEvents() override;

    void processInputs();
    void handleEvent(input_event ev);
    void dump_event(struct input_event *event);

    void updateWindowSize() override;
    void updateWindowPosition() override;
    void updateWindowTitle() override;

    void atlasTextureHasChanged(texture_atlas_t *atlas) override;
    void atlasTextureHasChangedHandler(JSCallbackUpdate *update);

    AminoVideoPlayer *createVideoPlayer(AminoTexture *texture, AminoVideo *video) override;

    void destroyEGLImageHandler(AsyncValueUpdate *update, int state);
};

#endif
