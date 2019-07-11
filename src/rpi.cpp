#include "rpi.h"

#include <dirent.h>
#include <stdio.h>

#include <execinfo.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

//GBM
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#define gettid() syscall(SYS_gettid)

//cbxx FIXME debugging
#define DEBUG_GLES true
#define DEBUG_RENDER true
#define DEBUG_INPUT false
#define DEBUG_HDMI true

#define AMINO_EGL_SAMPLES 4
#define test_bit(bit, array) (array[bit / 8] & (1 << (bit % 8)))

//
// AminoGfxRPi
//

AminoGfxRPi::AminoGfxRPi(): AminoGfx(getFactory()->name) {
    //empty
}

AminoGfxRPi::~AminoGfxRPi() {
    if (!destroyed) {
        destroyAminoGfxRPi();
    }
}

/**
 * Get factory instance.
 */
AminoGfxRPiFactory* AminoGfxRPi::getFactory() {
    static AminoGfxRPiFactory *instance = NULL;

    if (!instance) {
        instance = new AminoGfxRPiFactory(New);
    }

    return instance;
}

/**
 * Add class template to module exports.
 */
NAN_MODULE_INIT(AminoGfxRPi::Init) {
    AminoGfxRPiFactory *factory = getFactory();

    AminoGfx::Init(target, factory);
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoGfxRPi::New) {
    AminoJSObject::createInstance(info, getFactory());
}

/**
 * Setup JS instance.
 */
void AminoGfxRPi::setup() {
    if (DEBUG_GLES) {
        printf("AminoGfxRPi.setup()\n");
    }

    //init OpenGL ES
    if (!glESInitialized) {
        if (DEBUG_GLES) {
            printf("-> initializing VideoCore\n");
        }

        //VideoCore IV
        bcm_host_init();

        //Note: tvservice and others are already initialized by bcm_host_init() call!
        //      see https://github.com/raspberrypi/userland/blob/master/host_applications/linux/libs/bcm_host/bcm_host.c

        if (DEBUG_GLES) {
            printf("-> ready\n");
        }

        /*
         * register callback
         *
         * Note: never called with "hdmi_force_hotplug=1".
         */
        vc_tv_register_callback(tvservice_cb, NULL);

        //show info screen (Note: seems not to work!)
        //vc_tv_show_info(1);

        //handle preferred resolution
        if (!createParams.IsEmpty()) {
            v8::Local<v8::Object> obj = Nan::New(createParams);

            //resolution
            Nan::MaybeLocal<v8::Value> resolutionMaybe = Nan::Get(obj, Nan::New<v8::String>("resolution").ToLocalChecked());

            if (!resolutionMaybe.IsEmpty()) {
                v8::Local<v8::Value> resolutionValue = resolutionMaybe.ToLocalChecked();

                if (resolutionValue->IsString()) {
                    std::string prefRes = AminoJSObject::toString(resolutionValue);

                    //change resolution

                    if (prefRes == "720p@24") {
                        forceHdmiMode(HDMI_CEA_720p24);
                    } else if (prefRes == "720p@25") {
                        forceHdmiMode(HDMI_CEA_720p25);
                    } else if (prefRes == "720p@30") {
                        forceHdmiMode(HDMI_CEA_720p30);
                    } else if (prefRes == "720p@50") {
                        forceHdmiMode(HDMI_CEA_720p50);
                    } else if (prefRes == "720p@60") {
                        forceHdmiMode(HDMI_CEA_720p60);
                    } else if (prefRes == "1080p@24") {
                        forceHdmiMode(HDMI_CEA_1080p24);
                    } else if (prefRes == "1080p@25") {
                        forceHdmiMode(HDMI_CEA_1080p25);
                    } else if (prefRes == "1080p@30") {
                        forceHdmiMode(HDMI_CEA_1080p30);
                    } else if (prefRes == "1080p@50") {
                        forceHdmiMode(HDMI_CEA_1080p50);
                    } else if (prefRes == "1080p@60") {
                        forceHdmiMode(HDMI_CEA_1080p60);
                    } else {
                        printf("unknown resolution: %s\n", prefRes.c_str());
                    }
                }
            }
        }

        glESInitialized = true;
    }

    //instance
    addInstance();

    //basic EGL to get screen size
    initEGL();

    //base class
    AminoGfx::setup();
}

/**
 * Initialize EGL and get display size.
 */
void AminoGfxRPi::initEGL() {
    if (DEBUG_GLES) {
        printf("AminoGfxRPi::initEGL()\n");
    }
//cbxx hangs somewhere here on RPi 4 (Dispmanx)
    //get an EGL display connection
    display = eglGetDisplay(displayType);

    assert(display != EGL_NO_DISPLAY);

    //initialize the EGL display connection
    EGLBoolean res = eglInitialize(display, NULL, NULL);

    assert(res != EGL_FALSE);

    //get an appropriate EGL frame buffer configuration
    //this uses a BRCM extension that gets the closest match, rather than standard which returns anything that matches
    static const EGLint attribute_list[] = {
        //RGBA
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,

        //OpenGL ES 2.0
        EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,

        //buffers
        EGL_STENCIL_SIZE, 8,
        EGL_DEPTH_SIZE, 16,

        //sampling (quality)
        EGL_SAMPLE_BUFFERS, 1,
        EGL_SAMPLES, AMINO_EGL_SAMPLES, //4: 4x MSAA

        //window
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,

        EGL_NONE
    };

    EGLint num_config;

    res = eglSaneChooseConfigBRCM(display, attribute_list, &config, 1, &num_config);

    assert(EGL_FALSE != res);

    //choose OpenGL ES 2
    res = eglBindAPI(EGL_OPENGL_ES_API);

    assert(EGL_FALSE != res);

    //create an EGL rendering context
    static const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);

    assert(context != EGL_NO_CONTEXT);

    //get display size (see http://elinux.org/Raspberry_Pi_VideoCore_APIs#graphics_get_display_size)
    int32_t success = graphics_get_display_size(0 /* LCD */, &screenW, &screenH);

    assert(success >= 0); //Note: check display resolution (force if not connected to display)
    assert(screenW > 0);
    assert(screenH > 0);

    if (DEBUG_GLES) {
        printf("RPI: display size = %d x %d\n", screenW, screenH);
    }
}

/**
 * Get the current display state.
 *
 * Returns NULL if no display is connected.
 */
TV_DISPLAY_STATE_T* AminoGfxRPi::getDisplayState() {
    /*
     * Get TV state.
     *
     * - https://github.com/bmx-ng/sdl.mod/blob/master/sdlgraphics.mod/rpi_glue.c
     * - https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_hdmi.h
     */
    TV_DISPLAY_STATE_T *tvstate = (TV_DISPLAY_STATE_T *)malloc(sizeof(TV_DISPLAY_STATE_T));

    assert(tvstate);

    if (vc_tv_get_display_state(tvstate) != 0) {
        free(tvstate);
        tvstate = NULL;
    } else {
        /*
         * Notes:
         *
         * - Raspberry Pi 3 default:
         *    - hdmi_force_hotplug=1 is being used in /boot/config.txt
         *    - 1920x1080@60Hz on HDMI (mode=16, group=1)
         *    - monitor data gets cached, reported as connected even if the HDMI cable was unplugged!
         */

        //debug
        if (DEBUG_HDMI) {
            printf("State: %i\n", tvstate->state);
        }

        //check HDMI
        if ((tvstate->state & VC_HDMI_UNPLUGGED) == VC_HDMI_UNPLUGGED) {
            //unplugged (so notes above)
            if (DEBUG_HDMI) {
                printf("-> unplugged\n");
            }

            free(tvstate);
            tvstate = NULL;
        } else if ((tvstate->state & (VC_HDMI_HDMI | VC_HDMI_DVI)) == 0) {
            //no HDMI (or DVI) output
            if (DEBUG_HDMI) {
                printf("-> no HDMI display\n");
            }

            free(tvstate);
            tvstate = NULL;
        }

        if (tvstate && DEBUG_HDMI) {
            printf("Currently outputting %ix%i@%iHz on HDMI (mode=%i, group=%i).\n", tvstate->display.hdmi.width, tvstate->display.hdmi.height, tvstate->display.hdmi.frame_rate, tvstate->display.hdmi.mode, tvstate->display.hdmi.group);
        }
    }

    return tvstate;
}

/**
 * HDMI tvservice callback.
 */
void AminoGfxRPi::tvservice_cb(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2) {
    //http://www.m2x.nl/videolan/vlc/blob/1d2b56c68bbc3287e17f6140bdf8c8c3efe08fdc/modules/hw/mmal/vout.c

    if (DEBUG_HDMI) {
        //reasons seen: VC_HDMI_HDMI
        printf("-> tvservice state has changed: %s\n", vc_tv_notification_name((VC_HDMI_NOTIFY_T)reason));
    }

    //check resolution
    TV_DISPLAY_STATE_T *tvState = getDisplayState();

    if (tvState) {
        //Note: new resolution used by DispmanX calls
        free(tvState);
    }

    //check sem
    if (resSemValid) {
        sem_post(&resSem);
    }
}

/**
 * Destroy GLFW instance.
 */
void AminoGfxRPi::destroy() {
    if (destroyed) {
        return;
    }

    //instance
    destroyAminoGfxRPi();

    //destroy basic instance (activates context)
    AminoGfx::destroy();
}

/**
 * Destroy GLFW instance.
 */
void AminoGfxRPi::destroyAminoGfxRPi() {
    //OpenGL ES
    if (display != EGL_NO_DISPLAY) {
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
            context = EGL_NO_CONTEXT;
        }

        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
            surface = EGL_NO_SURFACE;
        }

        eglTerminate(display);
        display = EGL_NO_DISPLAY;
    }

    removeInstance();

    if (DEBUG_GLES) {
        printf("Destroyed OpenGL ES/EGL instance. Left=%i\n", instanceCount);
    }

    if (instanceCount == 0) {
        //tvservice
        vc_tv_unregister_callback(tvservice_cb);

        //VideoCore IV
        bcm_host_deinit();
        glESInitialized = false;
    }
}

/**
 * Get default monitor resolution.
 */
bool AminoGfxRPi::getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) {
    if (DEBUG_GLES) {
        printf("getScreenInfo\n");
    }

    TV_DISPLAY_STATE_T *tvState = getDisplayState();

    //get display properties
    w = screenW;
    h = screenH;
    refreshRate = 0; //unknown
    fullscreen = true;

    if (tvState) {
        //depends on attached screen
        refreshRate = tvState->display.hdmi.frame_rate;
    }

    //free
    if (tvState) {
        free(tvState);
    }

    return true;
}

/**
 * Get runtime statistics.
 */
void AminoGfxRPi::getStats(v8::Local<v8::Object> &obj) {
    AminoGfx::getStats(obj);

    //HDMI (see https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_hdmi.h)
    TV_DISPLAY_STATE_T *tvState = getDisplayState();

    if (!tvState) {
        //Note: does not occur on detached cable (see getDisplayState())

        return;
    }

    v8::Local<v8::Object> hdmiObj = Nan::New<v8::Object>();

    Nan::Set(obj, Nan::New("hdmi").ToLocalChecked(), hdmiObj);

    // HDMI_DISPLAY_STATE_T
    Nan::Set(hdmiObj, Nan::New("state").ToLocalChecked(), Nan::New(tvState->display.hdmi.state));
    Nan::Set(hdmiObj, Nan::New("width").ToLocalChecked(), Nan::New(tvState->display.hdmi.width));
    Nan::Set(hdmiObj, Nan::New("height").ToLocalChecked(), Nan::New(tvState->display.hdmi.height));
    Nan::Set(hdmiObj, Nan::New("frameRate").ToLocalChecked(), Nan::New(tvState->display.hdmi.frame_rate));
    Nan::Set(hdmiObj, Nan::New("scanMode").ToLocalChecked(), Nan::New(tvState->display.hdmi.scan_mode));
    Nan::Set(hdmiObj, Nan::New("group").ToLocalChecked(), Nan::New(tvState->display.hdmi.group));
    Nan::Set(hdmiObj, Nan::New("mode").ToLocalChecked(), Nan::New(tvState->display.hdmi.mode));
    Nan::Set(hdmiObj, Nan::New("pixelRep").ToLocalChecked(), Nan::New(tvState->display.hdmi.pixel_rep));
    Nan::Set(hdmiObj, Nan::New("aspectRatio").ToLocalChecked(), Nan::New(tvState->display.hdmi.aspect_ratio));
    Nan::Set(hdmiObj, Nan::New("pixelEncoding").ToLocalChecked(), Nan::New(tvState->display.hdmi.pixel_encoding));
    Nan::Set(hdmiObj, Nan::New("format3d").ToLocalChecked(), Nan::New(tvState->display.hdmi.format_3d));

    //display options (HDMI_DISPLAY_OPTIONS_T)
    v8::Local<v8::Object> displayObj = Nan::New<v8::Object>();

    Nan::Set(hdmiObj, Nan::New("displayOptions").ToLocalChecked(), displayObj);

    Nan::Set(displayObj, Nan::New("aspect").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.aspect));
    Nan::Set(displayObj, Nan::New("verticalBarPresent").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.vertical_bar_present));
    Nan::Set(displayObj, Nan::New("leftBarWidth").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.left_bar_width));
    Nan::Set(displayObj, Nan::New("rightBarWidth").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.right_bar_width));
    Nan::Set(displayObj, Nan::New("horizontalBarPresent").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.horizontal_bar_present));
    Nan::Set(displayObj, Nan::New("topBarHeight").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.top_bar_height));
    Nan::Set(displayObj, Nan::New("bottomBarHeight").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.bottom_bar_height));
    Nan::Set(displayObj, Nan::New("overscanFlags").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.overscan_flags));

    //device
    TV_DEVICE_ID_T id;

    memset(&id, 0, sizeof(id));

    if (vc_tv_get_device_id(&id) == 0 && id.vendor[0] != '\0' && id.monitor_name[0] != '\0') {
        v8::Local<v8::Object> deviceObj = Nan::New<v8::Object>();

        //add monitor property
        Nan::Set(hdmiObj, Nan::New("device").ToLocalChecked(), deviceObj);

        //properties
        Nan::Set(deviceObj, Nan::New("vendor").ToLocalChecked(), Nan::New(id.vendor).ToLocalChecked());
        Nan::Set(deviceObj, Nan::New("monitorName").ToLocalChecked(), Nan::New(id.monitor_name).ToLocalChecked());
        Nan::Set(deviceObj, Nan::New("serialNum").ToLocalChecked(), Nan::New(id.serial_num));
    }
}

/**
 * Switch to HDMI mode.
 */
void AminoGfxRPi::forceHdmiMode(uint32_t code) {
    if (DEBUG_HDMI) {
        printf("Changing resolution to CEA code %i\n", (int)code);
    }

    //Note: mode change takes some time (is asynchronous)
    //      see https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_hdmi.h
    sem_init(&resSem, 0, 0);
    resSemValid = true;

    if (vc_tv_hdmi_power_on_explicit(HDMI_MODE_HDMI, HDMI_RES_GROUP_CEA, code) != 0) {
        printf("-> failed\n");
        return;
    }

    //wait for change to occur before DispmanX is initialized
    sem_wait(&resSem);
    sem_destroy(&resSem);
    resSemValid = false;
}

/**
 * Switch HDMI off.
 */
void AminoGfxRPi::switchHdmiOff() {
    if (DEBUG_HDMI) {
        printf("Switching HDMI off.\n");
    }

    vc_tv_power_off();
}

/**
 * Add VideoCore IV properties.
 */
void AminoGfxRPi::populateRuntimeProperties(v8::Local<v8::Object> &obj) {
    if (DEBUG_GLES) {
        printf("populateRuntimeProperties()\n");
    }
//cbxx FIXME stops here
    AminoGfx::populateRuntimeProperties(obj);

    //GLES
    Nan::Set(obj, Nan::New("eglVendor").ToLocalChecked(), Nan::New(std::string(eglQueryString(display, EGL_VENDOR))).ToLocalChecked());
    Nan::Set(obj, Nan::New("eglVersion").ToLocalChecked(), Nan::New(std::string(eglQueryString(display, EGL_VERSION))).ToLocalChecked());

    //VC
    char resp[80] = "";

    if (vc_gencmd(resp, sizeof resp, "get_mem gpu") == 0) {
        //GPU memory in MB
        int gpuMem = 0;

        vc_gencmd_number_property(resp, "gpu", &gpuMem);

        if (gpuMem > 0) {
            Nan::Set(obj, Nan::New("gpu_mem").ToLocalChecked(), Nan::New(gpuMem));

            //debug
            //printf("gpu_mem: %i\n", gpuMem);
        }
    }
}

/**
 * Initialize OpenGL ES.
 */
void AminoGfxRPi::initRenderer() {
    if (DEBUG_GLES) {
        printf("initRenderer()\n");
    }

    //base
    AminoGfx::initRenderer();

    //set display size & viewport
    updateSize(screenW, screenH);
    updatePosition(0, 0);

    viewportW = screenW;
    viewportH = screenH;
    viewportChanged = true;

    if (DEBUG_HDMI) {
        printf("-> init Dispmanx\n");
    }

#ifdef EGL_DISPMANX
    //Dispmanx
    initDispmanxSurface();
#endif

#ifdef EGL_GBM
    //GBM
    initGbmSurface();
#endif

    //activate context (needed by JS code to create shaders)
    EGLBoolean res = eglMakeCurrent(display, surface, surface, context);

    assert(EGL_FALSE != res);

    //swap interval
    if (swapInterval != 0) {
        res = eglSwapInterval(display, swapInterval);

        assert(res == EGL_TRUE);
    }

    //input
    initInput();
}

/**
 * Get EGL surface from Dispmanx (RPi 3 and lower).
 */
void AminoGfxRPi::initDispmanxSurface() {
    //Dispmanx init
    DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open(0); //LCD
    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);

    int LAYER = 0;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = screenW;
    dst_rect.height = screenH;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = screenW << 16;
    src_rect.height = screenH << 16;

    VC_DISPMANX_ALPHA_T dispman_alpha;

    //Notes: works but seeing issues with font shader which uses transparent pixels! The background is visible at the border pixels of the font!
    /*
    dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE;
    dispman_alpha.opacity = 255;
    dispman_alpha.mask = 0;
    */

    dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
    dispman_alpha.opacity = 0xFF;
    dispman_alpha.mask = 0;

    DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add(
        dispman_update,
        dispman_display,
        LAYER, //layer
        &dst_rect,
        0, //src
        &src_rect,
        DISPMANX_PROTECTION_NONE,
        &dispman_alpha, //alpha
        0, //clamp,
        (DISPMANX_TRANSFORM_T)0 //transform
    );

    vc_dispmanx_update_submit_sync(dispman_update);

    //create EGL surface
    static EGL_DISPMANX_WINDOW_T native_window;

    native_window.element = dispman_element;
    native_window.width = screenW;
    native_window.height = screenH;

    surface = eglCreateWindowSurface(display, config, &native_window, NULL);

    //Note: happens for instance if there is a resource leak (restart the RPi in this case)
    assert(surface != EGL_NO_SURFACE);
}

/**
 * Get EGL surface from GBM.
 */
void AminoGfxRPi::initGbmSurface() {
    //access OpenGL driver (available if OpenGL driver is loaded)
    //cbxx TODO switch dynamically between Dispmanx and GBM
    int device = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);

    assert(device > 0);

    //get display resolutions
    drmModeRes *resources = drmModeGetResources(device);

    assert(resources);

    //find connector
    drmModeConnector *connector = NULL;
    uint32_t connector_id = 0;

	for (int i = 0; i < resources->count_connectors; i++) {
		drmModeConnector *connector2 = drmModeGetConnector(device, resources->connectors[i]);

		// pick the first connected connector
		if (connector2->connection == DRM_MODE_CONNECTED) {
            //Note: have to free instance later
			connector = connector2;
            break;
		}

		drmModeFreeConnector(connector2);
	}

    assert(connector);

	connector_id = connector->connector_id;

	//show mode
	drmModeModeInfo mode_info = connector->modes[0];

    if (DEBUG_GLES) {
	    printf ("resolution: %ix%i\n", mode_info.hdisplay, mode_info.vdisplay);
    }

	//find an encoder
    assert(connector->encoder_id);

    drmModeEncoder *encoder = drmModeGetEncoder(device, connector->encoder_id);

    assert(encoder);

	//find a CRTC
    drmModeCrtc *crtc = NULL;

	if (encoder->crtc_id) {
		crtc = drmModeGetCrtc(device, encoder->crtc_id);
	}

	drmModeFreeEncoder(encoder);
	drmModeFreeConnector(connector);
	drmModeFreeResources(resources);

    //create GBM device
    struct gbm_device *gbm_device = gbm_create_device(device);

    assert(gbm_device);

    displayType = gbm_device;

    //create surface
    gbm_surface *gbm_surface = gbm_surface_create(gbm_device, mode_info.hdisplay, mode_info.vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    assert(gbm_surface);

	surface = eglCreateWindowSurface(display, config, gbm_surface, NULL);

    assert(surface != EGL_NO_SURFACE);
}

bool AminoGfxRPi::startsWith(const char *pre, const char *str) {
    size_t lenpre = strlen(pre);
    size_t lenstr = strlen(str);

    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

const char* INPUT_DIR = "/dev/input";

void AminoGfxRPi::initInput() {
    if ((getuid()) != 0) {
        printf("you are not root. this might not work\n");
    }

    DIR *dir;
    struct dirent *file;

    dir = opendir(INPUT_DIR);

    if (dir) {
        while ((file = readdir(dir)) != NULL) {
            if (!startsWith("event", file->d_name)) {
                continue;
            }

            if (DEBUG_INPUT) {
                printf("file = %s\n", file->d_name);
                printf("initing a device\n");
            }

            char str[256];

            strcpy(str, INPUT_DIR);
            strcat(str, "/");
            strcat(str, file->d_name);

            int fd = -1;
            if ((fd = open(str, O_RDONLY | O_NONBLOCK)) == -1) {
                printf("this is not a valid device %s\n", str);
                continue;
            }

            char name[256] = "Unknown";

            ioctl(fd, EVIOCGNAME(sizeof name), name);

            printf("Reading from: %s (%s)\n", str,name);

            ioctl(fd, EVIOCGPHYS(sizeof name), name);

            printf("Location %s (%s)\n", str,name);

            struct input_id device_info;

            ioctl(fd, EVIOCGID, &device_info);

            u_int8_t evtype_b[(EV_MAX+7)/8];

            memset(evtype_b, 0, sizeof evtype_b);

            if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evtype_b) < 0) {
                printf("error reading device info\n");
                continue;
            }

            for (int i = 0; i < EV_MAX; i++) {
                if (test_bit(i, evtype_b)) {
                    printf("event type 0x%02x ", i);

                    switch (i) {
                        case EV_SYN:
                            printf("sync events\n");
                            break;

                        case EV_KEY:
                            printf("key events\n");
                            break;

                        case EV_REL:
                            printf("rel events\n");
                            break;

                        case EV_ABS:
                            printf("abs events\n");
                            break;

                        case EV_MSC:
                            printf("msc events\n");
                            break;

                        case EV_SW:
                            printf("sw events\n");
                            break;

                        case EV_LED:
                            printf("led events\n");
                            break;

                        case EV_SND:
                            printf("snd events\n");
                            break;

                        case EV_REP:
                            printf("rep events\n");
                            break;

                        case EV_FF:
                            printf("ff events\n");
                            break;
                    }
                }
            }

            fds.push_back(fd);
        }

        closedir(dir);
    }
}

void AminoGfxRPi::start() {
    if (DEBUG_GLES) {
        printf("start()\n");
    }

    //ready to get control back to JS code
    ready();

    //detach context from main thread
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool AminoGfxRPi::bindContext() {
    //bind OpenGL context
    if (surface == EGL_NO_SURFACE) {
        return false;
    }

    EGLBoolean res = eglMakeCurrent(display, surface, surface, context);

    assert(res == EGL_TRUE);

    return true;
}

void AminoGfxRPi::renderingDone() {
    if (DEBUG_GLES) {
        printf("renderingDone()\n");
    }
//cbxx hangs here after reboot on RPi 4
    //swap buffer
    EGLBoolean res = eglSwapBuffers(display, surface);

    assert(res == EGL_TRUE);

    if (DEBUG_GLES) {
        printf("-> EGL buffers swapped\n");
    }
}

void AminoGfxRPi::handleSystemEvents() {
    //handle events
    processInputs();
}

void AminoGfxRPi::processInputs() {
    if (DEBUG_GLES) {
        printf("processInputs()\n");
    }

    int size = sizeof(struct input_event);
    struct input_event ev[64];

    for (unsigned int i = 0; i < fds.size(); i++) {
        int fd = fds[i];
        int rd = read(fd, ev, size*64);

        if (rd == -1) {
            continue;
        }

        if (rd < size) {
            printf("read too little!!!  %d\n", rd);
        }

        for (int i = 0; i < (int)(rd / size); i++) {
            //dump_event(&(ev[i]));
            handleEvent(ev[i]);
        }
    }

    if (DEBUG_GLES) {
        printf("-> done\n");
    }
}

void AminoGfxRPi::handleEvent(input_event ev) {
    //relative event. probably mouse
    if (ev.type == EV_REL) {
        if (ev.code == 0) {
            //x axis
            mouse_x += ev.value;
        }

        if (ev.code == 1) {
            mouse_y += ev.value;
        }

        if (mouse_x < 0) {
            mouse_x = 0;
        }

        if (mouse_y < 0) {
            mouse_y = 0;
        }

        if (mouse_x >= (int)screenW)  {
            mouse_x = (int)screenW - 1;
        }

        if (mouse_y >= (int)screenH) {
            mouse_y = (int)screenH - 1;
        }

        //TODO GLFW_MOUSE_POS_CALLBACK_FUNCTION(mouse_x, mouse_y);

        return;
    }

    //mouse wheel
    if (ev.type == EV_REL && ev.code == 8) {
        //TODO GLFW_MOUSE_WHEEL_CALLBACK_FUNCTION(ev.value);
        return;
    }

    if (ev.type == EV_KEY) {
        if (DEBUG_GLES) {
            printf("key or button pressed code = %d, state = %d\n", ev.code, ev.value);
        }

        if (ev.code == BTN_LEFT) {
            //TODO GLFW_MOUSE_BUTTON_CALLBACK_FUNCTION(ev.code, ev.value);
            return;
        }

        //TODO GLFW_KEY_CALLBACK_FUNCTION(ev.code, ev.value);

        return;
    }
}

void AminoGfxRPi::dump_event(struct input_event *event) {
    switch(event->type) {
        case EV_SYN:
            printf("EV_SYN  event separator\n");
            break;

        case EV_KEY:
            printf("EV_KEY  keyboard or button \n");

            if (event ->code == KEY_A) {
                printf("  A key\n");
            }

            if (event ->code == KEY_B) {
                printf("  B key\n");
            }
            break;

        case EV_REL:
            printf("EV_REL  relative axis\n");
            break;

        case EV_ABS:
            printf("EV_ABS  absolute axis\n");
            break;

        case EV_MSC:
            printf("EV_MSC  misc\n");
            if (event->code == MSC_SERIAL) {
                printf("  serial\n");
            }

            if (event->code == MSC_PULSELED) {
                printf("  pulse led\n");
            }

            if (event->code == MSC_GESTURE) {
                printf("  gesture\n");
            }

            if (event->code == MSC_RAW) {
                printf("  raw\n");
            }

            if (event->code == MSC_SCAN) {
                printf("  scan\n");
            }

            if (event->code == MSC_MAX) {
                printf("  max\n");
            }
            break;

        case EV_LED:
            printf("EV_LED  led\n");
            break;

        case EV_SND:
            printf("EV_SND  sound\n");
            break;

        case EV_REP:
            printf("EV_REP  autorepeating\n");
            break;

        case EV_FF:
            printf("EV_FF   force feedback send\n");
            break;

        case EV_PWR:
            printf("EV_PWR  power button\n");
            break;

        case EV_FF_STATUS:
            printf("EV_FF_STATUS force feedback receive\n");
            break;

        case EV_MAX:
            printf("EV_MAX  max value\n");
            break;
    }

    printf("type = %d code = %d value = %d\n",event->type,event->code,event->value);
}

/**
 * Update the window size.
 *
 * Note: has to be called on main thread
 */
void AminoGfxRPi::updateWindowSize() {
    //not supported

    //reset to screen values
    propW->setValue(screenW);
    propH->setValue(screenH);
}

/**
 * Update the window position.
 *
 * Note: has to be called on main thread
 */
void AminoGfxRPi::updateWindowPosition() {
    //not supported
    propX->setValue(0);
    propY->setValue(0);
}

/**
 * Update the title.
 *
 * Note: has to be called on main thread
 */
void AminoGfxRPi::updateWindowTitle() {
    //not supported
}

/**
 * Shared atlas texture has changed.
 */
void AminoGfxRPi::atlasTextureHasChanged(texture_atlas_t *atlas) {
    //check single instance case
    if (instanceCount == 1) {
        return;
    }

    //run on main thread
    enqueueJSCallbackUpdate(static_cast<jsUpdateCallback>(&AminoGfxRPi::atlasTextureHasChangedHandler), NULL, atlas);
}

/**
 * Handle on main thread.
 */
void AminoGfxRPi::atlasTextureHasChangedHandler(JSCallbackUpdate *update) {
    AminoGfx *gfx = static_cast<AminoGfx *>(update->obj);
    texture_atlas_t *atlas = (texture_atlas_t *)update->data;

    for (auto const &item : instances) {
        if (gfx == item) {
            continue;
        }

        static_cast<AminoGfxRPi *>(item)->updateAtlasTexture(atlas);
    }
}

/**
 * Create video player.
 */
AminoVideoPlayer* AminoGfxRPi::createVideoPlayer(AminoTexture *texture, AminoVideo *video) {
    return new AminoOmxVideoPlayer(texture, video);
}

/**
 * Create EGL Image.
 */
EGLImageKHR AminoGfxRPi::createEGLImage(GLuint textureId) {
    /*
     * Notes:
     *
     * - In case of failure check gpu_mem is high enough and only one process is using OMX.
     *
     * Sample error:
     *
     *   eglCreateImageKHR:  failed to create image for buffer 0x4 target 12465 error 0x300c
     */

    return eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)textureId, 0);
}

/**
 * Destroy EGL Image.
 */
void AminoGfxRPi::destroyEGLImage(EGLImageKHR eglImage) {
    //switch to rendering thread
    AminoJSObject::enqueueValueUpdate(0, eglImage, static_cast<asyncValueCallback>(&AminoGfxRPi::destroyEGLImageHandler));
}

/**
 * Destroy EGL Image texture on OpenGL thread.
 */
void AminoGfxRPi::destroyEGLImageHandler(AsyncValueUpdate *update, int state) {
    if (state != AsyncValueUpdate::STATE_APPLY) {
        return;
    }

    assert(update->data);

    if (DEBUG_VIDEOS) {
        printf("destroying EGL image\n");
    }

    EGLBoolean res = eglDestroyImageKHR(display, (EGLImageKHR)update->data);

    assert(res == EGL_TRUE);
}

//static initializers
bool AminoGfxRPi::glESInitialized = false;
sem_t AminoGfxRPi::resSem;
bool AminoGfxRPi::resSemValid = false;

//
// AminoGfxRPiFactory
//

/**
 * Create AminoGfx factory.
 */
AminoGfxRPiFactory::AminoGfxRPiFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoGfx", callback) {
    //empty
}

/**
 * Create AminoGfx instance.
 */
AminoJSObject* AminoGfxRPiFactory::create() {
    return new AminoGfxRPi();
}

void crashHandler(int sig) {
    void *array[10];
    size_t size;

    //process & thread
    pid_t pid = getpid();
    pid_t tid = gettid();
    uv_thread_t threadId = uv_thread_self();

    //get void*'s for all entries on the stack
    size = backtrace(array, 10);

    //print out all the frames to stderr
    fprintf(stderr, "Error: signal %d (process=%d, thread=%d, uvThread=%lu):\n", sig, pid, tid, (unsigned long)threadId);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// ========== Event Callbacks ===========

NAN_MODULE_INIT(InitAll) {
    //crash handler
    signal(SIGSEGV, crashHandler);

    //main class
    AminoGfxRPi::Init(target);

    //amino classes
    AminoGfx::InitClasses(target);
}

//entry point
NODE_MODULE(aminonative, InitAll)
