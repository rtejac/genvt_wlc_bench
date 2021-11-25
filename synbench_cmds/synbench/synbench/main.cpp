#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "main.h"
#include "draw-digits.h"
#include "kpi.h"
#include <sys/time.h>
#include <cstdlib>
#include <iomanip>
#include <vector>
#include <iterator>

using namespace std;
using namespace std::chrono;

// globals
char version[] = "0.1";
struct config gConfig;
textRender g_TextRender;
std::ofstream g_metricsfile;
bool g_recordMetrics = false;
DrawCases g_draw_case = simpleDial;
static bool g_Initalized = false;
unsigned int g_FramesToRender = 0;
uint64_t global_frameid = 0;

char *config_filename = NULL;

//The window we'll be rendering to
SDL_Window *gWindow = NULL;

//OpenGL context
SDL_GLContext gContext;

//Render flag
bool gRenderQuad = true;

// texture ids
GLuint g_textureID = 0;
GLuint g_dialTexID = 0;
GLuint g_needleTexID = 0;
GLuint g_hmiTexID = 0;

// Default pyramid geometry parameters
int x_count = 5;
int y_count = 5;
int z_count = 5;
int g_batchSize = 1;

GLfloat *pyramid_positions = NULL;
GLfloat *pyramid_colors_single_draw = NULL;
GLfloat *pyramid_transforms = NULL;

// Default screen dimension constants
const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;

// Graphics program
GLuint gProgramID = 0;

struct actions table[] = {
        {"simpleDial",       initialize_simpleDial,       draw_simpleDial,       cleanup_simpleDial,       NULL},
        {"singleDrawArrays", initialize_singleDrawArrays, draw_singleDrawArrays, cleanup_singleDrawArrays, hk_singleDrawArrays},
        {"multiDrawArrays",  initialize_multiDrawArrays,  draw_multiDrawArrays,  cleanup_multiDrawArrays,  hk_multiDrawArrays},
        {"simpleTexture",    initialize_simpleTexture,    draw_simpleTexture,    cleanup_simpleTexture,    NULL},
        {"longShader",       initialize_longShader,       draw_longShader,       cleanup_longShader,       NULL},
        {"batchDrawArrays",  initialize_batchDrawArrays,  draw_batchDrawArrays,  cleanup_batchDrawArrays,  hk_multiDrawArrays},
        {"simple_egl",       initialize_simple_egl,       draw_simple_egl,       cleanup_simple_egl,       NULL},
        {"hmi",              initialize_hmi,              draw_hmi,              cleanup_hmi,              NULL},
};

int get_refresh_rate();

// parse out the digits of a string
int safeParse(std::string &line, const int max_digits, const int minvalue = 0)
{
    int value = 0;
    char *end = NULL;

    value = (int) strtol(line.substr(0, max_digits).c_str(), &end, 10);

    if (value < 0)
    {
        value = 0;
    }

    if (value < minvalue)
    {
        value = minvalue;
    }

    return value;
}

float safeParseF(std::string &line, const int max_digits, const float minvalue = 0.0f)
{
    float value = 0;
    char *end = NULL;

    value = strtof(line.substr(0, max_digits).c_str(), &end);

    if (value < minvalue)
    {
        value = minvalue;
    }

    return value;
}

// calculate the per-frame fps
float calculate_fps(struct config *conf)
{
    static const uint32_t benchmark_interval = 1000;
    struct timeval tv;
    static float fps = 0.0f;

    // saving to the a file
    static uint32_t *frametimes = NULL;
    static uint32_t *frameids = NULL;
    static uint32_t frametime_window_index = 0;

    // timer related
    static uint64_t global_frameid = 0;
    static uint64_t prev_frame_timestamp = 0;
    static uint64_t now_timestamp = 0;
    static uint64_t now = 0;
    static uint64_t last = 0;
    static uint64_t prev = 0;

    //static uint64_t kprev = 0;
    //static int kdelta = 0;
    static int seconds = 0;

    // get time now
    gettimeofday(&tv, NULL);
    now = tv.tv_sec * 1000000 + tv.tv_usec;

    // first frame setup
    if (0 == global_frameid)
    {
        //kprev = prev = now;
        prev = now;
    }

    // one-time setup for first frame
    if ((NULL == frametimes) && g_recordMetrics)
    {
        // allocate space for circulare frame time buffer
        frametimes = new uint32_t[FRAME_TIME_RECORDING_WINDOW_SIZE];
        frameids = new uint32_t[FRAME_TIME_RECORDING_WINDOW_SIZE];
        frametime_window_index = 0;

        gettimeofday(&tv, NULL);
        prev_frame_timestamp = tv.tv_sec * 1000000 + tv.tv_usec;

        // create unique metrics file name
        std::string filename = "metrics_";

        time_t currDateTime = time(NULL);
        struct tm *tm = localtime(&currDateTime);

        // build filename for log using current date/time
        if (tm)
        {
            std::ostringstream year, mon, mday, hour, min, sec;
            year << tm->tm_year + 1900;
            mon << tm->tm_mon + 1;
            mday << tm->tm_mday;
            hour << tm->tm_hour;
            min << tm->tm_min;
            sec << tm->tm_sec;

            filename +=
                    year.str() + "-" + mon.str() + "-" + mday.str() + "__" + hour.str() +
                    "-" + min.str() + "-" + sec.str() + ".csv";
        } else
        {
            filename += "log";
        }

        // open new file
        printf("Saving metrics in file: %s\n", filename.c_str());
        g_metricsfile.open(filename.c_str(), std::ofstream::out);

        // possibly store scene/configuration settings here

        // add column headers
        g_metricsfile << "frame,\tmicroseconds (1e-6)\n";
    }

    // if the frame metrics buffer is full, append it to the file
    if ((frametime_window_index >= FRAME_TIME_RECORDING_WINDOW_SIZE - 1) && g_recordMetrics)
    {
        printf("dumping metrics...\n");

        // dump to file
        if (FRAME_TIME_RECORDING_WINDOW_SIZE < global_frameid)
        {
            g_metricsfile << ",\n";
        }

        for (int i = 0; i < FRAME_TIME_RECORDING_WINDOW_SIZE - 2; i++)
        {
            g_metricsfile << frameids[i] << ",\t" << frametimes[i] << ",\n";
        }
        g_metricsfile << frameids[FRAME_TIME_RECORDING_WINDOW_SIZE -
                                  2] << ",\t" << frametimes[FRAME_TIME_RECORDING_WINDOW_SIZE
                                                            - 2];

        frametime_window_index = 0;
    }

    // store the time and frame id if we are recording metrics
    if (g_recordMetrics)
    {
        now_timestamp = tv.tv_sec * 1000000 + tv.tv_usec;

        frametimes[frametime_window_index] = (now_timestamp - prev_frame_timestamp);
        frameids[frametime_window_index] = global_frameid;

        prev_frame_timestamp = now_timestamp;
        frametime_window_index++;
    }

    if (0 == last) {
        last = now;
    }

    //
    // Call code to implement KPI model, each frame, but only for workloads
    // that are vsync locked.
    //
    if ((conf->frame_sync) && (1 == gConfig.kpi_state)) {
        evaluate_kpis(&gConfig, ((double)(now - last)) / 1000.0f);
        last = now;
    }

    // calculate delta since interval start
    uint64_t timeDelta = now - prev;

    // calculate fps if we have exceeded the benchmark interval
    if (timeDelta > (benchmark_interval * 1000))
    {
        fps = (float) ((double) conf->frames / ((double) timeDelta / 1000000.0));
        //printf("%s: %d frames in %.4f seconds: %.4f fps. \n",
        //       test_name, conf->frames, timeDelta / 1000000.0, fps);
        printf("%d frames in %.4f seconds: %.4f fps. \n",
               conf->frames, timeDelta / 1000000.0, fps);
        //fflush(stdout);
        prev = now;
        conf->frames = 0;
        conf->benchmark_time = now;
        seconds++;
    }

    //time_now = now / 1000.0;

    // increment the global frame id
    // the global frame id never resets, starts from first app frame
    global_frameid++;

    return fps;

}

// read the default_params.txt file to get all our running parameters
int read_config_file(char *config_filename)
{
    char filename[] = "params/default_params.txt";
    struct stat statbuff;

    std::ifstream infile;

    printf("Reading config from: ");
    if (config_filename)
    {
        printf("%s\n", config_filename);
        int err = stat(config_filename, &statbuff);
        if (err)
        {
            printf("Error opening %s parameters file\n", config_filename);
            exit_synbench(1);
        }

        infile.open(config_filename);
    } else
    {
        char *dest_dir = std::getenv("DESTDIR");

        if (dest_dir == nullptr)
        {
            cerr
                    << "ERROR : ****** Set environment variable DESTDIR to ensure correct paths of textures/params.txt get picked up."
                    << "Example: export DESTDIR=$PWD if local testing or  export DESTDIR=<path to synbench> ******\n";
            exit_synbench(1);
        }

        string full_path = string(dest_dir) + "/" + filename;

        int err = stat(full_path.c_str(), &statbuff);
        if (err)
        {
            printf("Error opening the default_params.txt parameters file\n");
            exit_synbench(1);
        }
        infile.open(full_path.c_str());
    }

    // read window/buffer dimensions
    std::string line;
    const int max_digits = 10;
    std::getline(infile, line);
    gConfig.width = safeParse(line, max_digits);
    std::getline(infile, line);
    gConfig.height = safeParse(line, max_digits);
    printf("Window dimensions = (%d,%d)\n", gConfig.width, gConfig.height);

    // run fullscreen?
    std::getline(infile, line);
    gConfig.fullscreen = safeParse(line, 1);
    printf("Window running fullscreen = %s\n", (gConfig.fullscreen) ? "true" : "false");

    // record metrics to a file?
    std::getline(infile, line);
    g_recordMetrics = safeParse(line, 1);
    printf("Record metrics = %s\n", (g_recordMetrics) ? "true" : "false");

    // run offscreen
    std::getline(infile, line);
    gConfig.offscreen = safeParse(line, 1);
    printf("draw offscreen = %s\n", (gConfig.offscreen) ? "true" : "false");

    // Enable vsync
    std::getline(infile, line);
    gConfig.frame_sync = safeParse(line, 1);
    printf("vsync = %d\n", gConfig.frame_sync);

    // Do not call swap buffers?
    std::getline(infile, line);
    gConfig.no_swapbuffer_call = safeParse(line, 1);
    printf("no_swapbuffer_call = %d\n", gConfig.no_swapbuffer_call);

    // which scene to draw
    std::getline(infile, line);
    g_draw_case = (DrawCases) safeParse(line, 1);

    if (g_draw_case >= simpleDial && g_draw_case < total_cases)
    {
        printf("Scene: %s\n", table[g_draw_case].title);
    } else
    {
        printf("Scene not supported, defaulting to dials\n");
        g_draw_case = simpleDial;
    }

    // use the flat grey shader for the texture scene?
    std::getline(infile, line);
    gConfig.texParam.texture_flat_no_rotate = safeParse(line, 1);
    gConfig.texBParam.texture_flat_no_rotate = gConfig.texParam.texture_flat_no_rotate;
    printf("Texture scene using simple shader = %s\n",
           (gConfig.texParam.texture_flat_no_rotate) ? "true" : "false");

    // if not using the flat gray shader on the texture scene, what is the
    // blur kernel radius
    std::getline(infile, line);
    gConfig.texBParam.texture_fetch_radius = (float) safeParse(line, max_digits);
    if (gConfig.texBParam.texture_fetch_radius < 1)
    {
        gConfig.texBParam.texture_fetch_radius = 1;
    }

    printf("Texture scene using fetch radius of = %.0f\n",
           gConfig.texBParam.texture_fetch_radius);

    // pyramid scene counts
    std::getline(infile, line);
    x_count = safeParse(line, max_digits, 1);
    std::getline(infile, line);
    y_count = safeParse(line, max_digits, 1);
    std::getline(infile, line);
    z_count = safeParse(line, max_digits, 1);
    std::getline(infile, line);
    g_batchSize = safeParse(line, max_digits, 1);

    printf("pyramid scenes dimensions: %dx%dx%d ", x_count, y_count, z_count);
    if (batchDrawArrays != g_draw_case)
    {
        printf("\n");
    } else
    {
        printf("in %d batches\n", g_batchSize);
    }

    // pyramid shader loop count
    std::getline(infile, line);
    gConfig.shortShader_loop_count = (float) safeParse(line, max_digits);
    printf("pyramid shader loop iterations: %.0f\n", gConfig.shortShader_loop_count);

    // dials shader loop count
    std::getline(infile, line);
    gConfig.dialsShader_loop_count = (float) safeParse(line, max_digits);
    printf("dials scene shader loop iterations: %.0f\n", gConfig.dialsShader_loop_count);

    // read the long/fullscreen quad shader loop count
    std::getline(infile, line);
    gConfig.longShader_loop_count = (float) safeParse(line, max_digits);
    printf("long shader loop iterations: %.0f\n", gConfig.longShader_loop_count);

    // read number of frames to render
    std::getline(infile, line);
    g_FramesToRender = (unsigned int) safeParse(line, max_digits);
    if (0 == g_FramesToRender)
    {
        printf("Number of frames to render: <infinite>\n");
        g_FramesToRender = -1;
    } else
    {
        printf("Number of frames to render: %d\n", g_FramesToRender);
    }

    // GL or GLES
    std::getline(infile, line);
    gConfig.gl = safeParse(line, 1);
    printf("Using OpenGL%s\n", gConfig.gl ? "" : "ES");

    // Fixed FPS
    std::getline(infile, line);
    gConfig.fixed_fps = safeParse(line, max_digits);
    if (gConfig.fixed_fps)
    {
        printf("Fixed FPS set to %d\n", gConfig.fixed_fps);
    } else
    {
        printf("Fixed FPS not set\n");
    }

    std::getline(infile, line);
    gConfig.hmi_overdraw_ratio = safeParseF(line, max_digits, 1.0f);
    printf("hmi overdraw ratio: %0.2f\n", gConfig.hmi_overdraw_ratio);

    std::getline(infile, line);
    gConfig.hmi_alpha_blending = (unsigned int) safeParse(line, max_digits);
    printf("hmi alpha blending: %d\n", gConfig.hmi_alpha_blending);

    std::getline(infile, line);
    gConfig.hmi_texturing = (unsigned int) safeParse(line, max_digits);
    printf("hmi texturing: %d\n", gConfig.hmi_texturing);

    std::getline(infile, line);
    gConfig.fps_limiter = safeParse(line, max_digits);
    if (gConfig.fps_limiter)
    {
        printf("FPS limiter (sleep) set to %d\n", gConfig.fps_limiter);
    } else
    {
        printf("FPS limiter (sleep) not set\n");
    }

    std::getline(infile, line);
    gConfig.debug_mode = safeParse(line, max_digits);
    printf("synbench debug mode: %d\n", gConfig.debug_mode);

    std::getline(infile, line);
    gConfig.hmi_3d_render = safeParse(line, max_digits);
    printf("Render 3D model on HMI: %d\n", gConfig.hmi_3d_render);
    
    std::getline(infile, line);
    gConfig.kpi_state = safeParse(line, max_digits);
    printf("kpi measurement state: %d\n", gConfig.kpi_state);

    std::getline(infile, line);
    gConfig.kpi_settling_time_seconds = safeParse(line, max_digits);
    printf("kpi settling time: %d\n", gConfig.kpi_settling_time_seconds);

    std::getline(infile, line);
    gConfig.kpi_number_nines = safeParse(line, max_digits);
    printf("kpi number nines: %d\n", gConfig.kpi_number_nines);

    std::getline(infile, line);
    gConfig.kpi_exit_on_failure = safeParse(line, max_digits);
    printf("kpi exit on failure: %d\n", gConfig.kpi_exit_on_failure);

    std::getline(infile, line);
    gConfig.kpi_max_frametime_milliseconds = safeParseF(line, max_digits, 0.0f);
    printf("kpi max frametime milliseconds: %0.2f\n", gConfig.kpi_max_frametime_milliseconds);

    std::getline(infile, line);
    gConfig.kpi_window_milliseconds = safeParseF(line, max_digits, 0.0f);
    printf("kpi window milliseconds: %0.2f\n", gConfig.kpi_window_milliseconds);

    std::getline(infile, line);
    gConfig.kpi_target_fps = safeParse(line, max_digits);
    printf("kpi target fps: %d\n", gConfig.kpi_target_fps);

    std::getline(infile, line);
    gConfig.kpi_min_fps = safeParse(line, max_digits);
    printf("kpi min fps: %d\n", gConfig.kpi_min_fps);

    return 0;
}

int get_refresh_rate()
{
    Display *dpy = XOpenDisplay(NULL);
    Window win = RootWindow(dpy, 0);
    XRRScreenConfiguration *conf = XRRGetScreenInfo(dpy, win);
    int refresh_rate = XRRConfigCurrentRate(conf);

    return refresh_rate;
}

void initGL()
{
    table[g_draw_case].init(&gConfig);
    g_TextRender.InitializeDigits(&gConfig);

    // create offscreen buffer if we params indicated to draw offscreen
    if (gConfig.offscreen)
    {
        // create a framebuffer object
        GLuint fboId = 0;

        GLuint renderBufferWidth = gConfig.width;
        GLuint renderBufferHeight = gConfig.height;
        glGenFramebuffers(1, &fboId);
        glBindFramebuffer(GL_FRAMEBUFFER, fboId);

        printf("glBindFramebuffer() = '0x%08x'\n", glGetError());
        GLuint renderBuffer = 0;
        glGenRenderbuffers(1, &renderBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);
        printf("glBindRenderbuffer() = '0x%08x'\n", glGetError());
        glRenderbufferStorage(GL_RENDERBUFFER,
                              GL_RGB565,
                              renderBufferWidth,
                              renderBufferHeight);
        printf("glRenderbufferStorage() = '0x%08x'\n", glGetError());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                  GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER,
                                  renderBuffer);

        printf("glFramebufferRenderbuffer() = '0x%08x'\n", glGetError());
        GLuint depthRenderbuffer;
        glGenRenderbuffers(1, &depthRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, renderBufferWidth, renderBufferHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);

        // check FBO status
        GLenum fbstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fbstatus != GL_FRAMEBUFFER_COMPLETE)
        {
            printf("Problem with OpenGL framebuffer after specifying color render buffer: \n%x\n", fbstatus);
        } else
        {
            printf("FBO creation succedded\n");
        }

        // we must query for the format of the offscreen buffer
        // opengl es has very limited offscreen buffer formats
        GLint format = 0, type = 0;
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);
        printf("Offscreen buffer - Format = 0x%x ", format);
        if (GL_RGB == format)
        {
            printf("GL_RGB");
        }
        printf("\nType = 0x%x", type);    // GL_RGB = 0x1907 in /usr/include/GLES2/gl2.h
        if (GL_UNSIGNED_SHORT_5_6_5 == type)
        {
            // GL_UNSIGNED_SHORT_5_6_5 = 0x8363
            printf("GL_UNSIGNED_SHORT_5_6_5");
        }
        printf("\n");

        glClearColor(1.0, 0.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        if (gConfig.debug_mode == 1)
        {
            // unbind from FBO to enable window rendering from FBO to screen
            printf("Rendering back from FBO to OpenGL window to verify offscreen rendering working..\n");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        } else
        {
            // hide openGL window if offscreen mode
            //SDL_HideWindow(gWindow);
            SDL_MinimizeWindow(gWindow);
        }
    }

    // set cross-scene defaults
    glClearColor(0.0, 0.0, 0.0, 1.0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, gConfig.width, gConfig.height);

    g_Initalized = true;
}

bool init()
{
    //Initialization flag
    bool success = true;

    int ret = 0;

    if (1 == gConfig.kpi_state) {
        ret = init_kpi_model(&gConfig);
    }

    if (0 == ret) {

        //Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
            success = false;
        } else
        {
            //Use OpenGLES 3.1 core
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
            SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
            if (gConfig.gl)
            {
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            } else
            {
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            }

            char workload_title[140];
            string temp;
            if (config_filename == nullptr)
            {
                temp = "params/default_params.txt";
            } else
            {
                temp = config_filename;
            }
            int index = temp.find_last_of("/\\");
            std::string mode = temp.substr(index + 1);
            snprintf(workload_title, 140, "Intel Synbench Beta v%s: %s using: %s ", version, table[g_draw_case].title,
                     mode.c_str());

            //Create window
            gWindow = SDL_CreateWindow(workload_title,
                                       SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                       gConfig.width, gConfig.height,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
                                       | (gConfig.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
            if (gWindow == NULL)
            {
                printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
                success = false;
            } else
            {
                //Create context
                gContext = SDL_GL_CreateContext(gWindow);
                if (gContext == NULL)
                {
                    printf("OpenGL context could not be created! SDL Error: %s\n", SDL_GetError());
                    success = false;
                } else
                {
                    if (SDL_GL_SetSwapInterval(gConfig.frame_sync) < 0)
                    {
                        printf("Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError());
                    }

                    //for X11
                    //Initialize GLEW
                    glewExperimental = GL_TRUE;
                    GLenum glewError = glewInit();
                    if (glewError != GLEW_OK)
                    {
                        printf("Error initializing GLEW! %s\n", glewGetErrorString(glewError));
                    }

                    //Initialize OpenGL
                    initGL();
                }
            }
        }
    } else {
        printf("ERROR: MQTT client/connection could not be created\n");
        exit_synbench(-1);
    }


    return success;
}

void close()
{
    table[g_draw_case].cleanup(&gConfig);
    g_TextRender.Cleanup();

    //Deallocate program
    glDeleteProgram(gProgramID);

    //Destroy window
    SDL_DestroyWindow(gWindow);
    gWindow = NULL;

    //Quit SDL subsystems
    SDL_Quit();

    if (1 == gConfig.kpi_state) {
        cleanup_kpi_model();
    }
}

void track_desired_fps()
{
    static const uint32_t benchmark_interval = 5000;
    static const int desired_delta = 3;
    int need_to_change = 1;
    struct timeval tv;
    float percent;
    static float prev_percent = 0;
    unsigned char key = '+';
    static int close = 0;

    // timer related
    static uint64_t now = 0;
    static uint64_t prev = 0;

    // get time now
    gettimeofday(&tv, NULL);

    now = (uint64_t) (tv.tv_sec) * 1000000 + tv.tv_usec;

    // first frame setup
    if (0 == global_frameid)
    {
        prev = now;
    }

    // calculate delta since interval start
    uint64_t timeDelta = now - prev;

    // calculate fps if we have exceeded the benchmark interval
    if (timeDelta > (benchmark_interval * 1000))
    {
        percent = ((float) ((gConfig.current_fps - gConfig.fixed_fps) / gConfig.fixed_fps)) * 100;

        if (percent > (desired_delta * -1) && percent < desired_delta)
        {
            need_to_change = 0;
        } else if (prev_percent < 0 && percent > 0)
        {
            key = ')';
            close = 1;
        } else if (prev_percent > 0 && percent < 0)
        {
            key = '(';
            close = 1;
        } else if (percent > desired_delta)
        {
            key = close ? ')' : '+';
        } else if (percent < (desired_delta * -1))
        {
            key = close ? '(' : '-';
        }

        prev_percent = percent;
        prev = now;

        if (need_to_change)
        {
            table[g_draw_case].handle_keys(key, 0, 0);
        }
    }
}

#undef main

int main(int argc, char *argv[])
{
    memset(&gConfig, 0, sizeof(gConfig));

    gConfig.width = SCREEN_WIDTH;    // default window dim
    gConfig.height = SCREEN_HEIGHT;    // default window dim
    gConfig.frame_sync = 0;
    gConfig.fullscreen = 0;
    g_recordMetrics = false;

    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;
    using std::chrono::microseconds;

    // read the config file

    if (argc)
    {
        config_filename = argv[1];
    }
    read_config_file(config_filename);

    //Start up SDL and create window
    if (!init())
    {
        printf("Failed to initialize!\n");
    } else
    {

        //Main loop flag
        bool quit = false;

        //Event handler
        SDL_Event e;

        //Enable text input
        SDL_StartTextInput();

        ofstream out;
        vector<vector<double>> vect;
        int counter = 0;

        if (g_recordMetrics && !gConfig.fps_limiter)
        {
            system_clock::time_point timePoint = system_clock::now();
            time_t t = system_clock::to_time_t(timePoint);
            std::stringstream stream;
            stream << put_time(std::localtime(&t), "%Y-%m-%d_%I-%M-%S");

            string filename;
            filename += "./frame_logs_" + stream.str() + ".csv";
            out.open(filename.c_str());
            out << "frame_id,total_frame_time,render_time,swap_time,\n";
        }

        //While application is running
        while (!quit && (g_FramesToRender != 0))
        {
            auto t1 = high_resolution_clock::now();
            //Handle events on queue
            while (SDL_PollEvent(&e) != 0)
            {
                //User requests quit
                if ((e.type == SDL_QUIT)
                    || (e.key.keysym.sym == SDLK_ESCAPE)
                    || (e.key.keysym.sym == SDLK_q))
                {
                    quit = true;
                }
                    //Handle keypress with current mouse position
                else if (e.type == SDL_TEXTINPUT)
                {
                    int x = 0, y = 0;
                    SDL_GetMouseState(&x, &y);
                    if (table[g_draw_case].handle_keys)
                    {
                        table[g_draw_case].handle_keys(e.text.text[0], x, y);
                    }
                }
            }

            /*
             * If the user wants a fixed fps to be automatically reached, we will try
             * to increase or decrease the pyramid count. We will do so by automatically
             * sending the key strokes that the user would've sent (ex: +, -). However,
             * in order to do so, we need this draw case to handle keys so check if it
             * does here or not
             */
            if (gConfig.fixed_fps && table[g_draw_case].handle_keys)
            {
                track_desired_fps();
            }

            gConfig.current_fps = calculate_fps(&gConfig);
            
            //Render workload
            table[g_draw_case].render(&gConfig);
            g_TextRender.DrawDigits(gConfig.current_fps, &gConfig);

            auto t2 = high_resolution_clock::now();

            //Update screen
            SDL_GL_SwapWindow(gWindow);
            auto t3 = high_resolution_clock::now();

            if (g_recordMetrics && !gConfig.fps_limiter)
            {
                duration<double, std::milli> frame = t3 - t1;
                duration<double, std::milli> render = t2 - t1;
                duration<double, std::milli> swap = t3 - t2;
                // cout << frame.count() << "ms\t" << render.count() << "ms\t" << swap.count() << "ms\n";

                vector<double> log;
                log.push_back(counter);
                log.push_back(frame.count());
                log.push_back(render.count());
                log.push_back(swap.count());
                vect.push_back(log);

                if (vect.size() % FRAME_TIME_RECORDING_WINDOW_SIZE == 0)
                {
                    for (const auto &row : vect)
                    {
                        copy(row.cbegin(), row.cend(), ostream_iterator<float>(out, ","));
                        out << '\n';
                    }
                    vect.clear();
                    vect.back();
                }
                counter++;
            }

            if (gConfig.fps_limiter)
            {
                struct timeval tv{};
                gettimeofday(&tv, nullptr);

                long now = (long) tv.tv_sec * 1e6 + tv.tv_usec;
                static long prev = 0;

                long frameDelta = now - prev; // time passed between frames - from previous to current call of this line
                static long limitDelta = 1e6 / gConfig.fps_limiter; // time that should pass to respect the frame limit

                static long lastSleep = 0; // time to sleep to reach the frame limit
                long delta = limitDelta - frameDelta; // difference between the frame limit delay and actual delay
                if (delta > 0)
                {
                    lastSleep = delta + lastSleep;
                    usleep(lastSleep);
                } else if (lastSleep > 0)
                {
                    lastSleep = delta + lastSleep;
                    if (lastSleep > 0)
                    { // if the value is negative skip sleeping anymore and reset the last sleep value
                        usleep(lastSleep);
                    } else
                    {
                        lastSleep = 0;
                    }
                }
                prev = now;
            }

            if (g_FramesToRender > 0)
            {
                g_FramesToRender--;
            }
        }
        //Disable text input
        SDL_StopTextInput();
    }

    //Free resources and close SDL
    close();

    return 0;
}

void exit_synbench(int val) {
    if (1 == gConfig.kpi_state) {
        cleanup_kpi_model();
    }
    exit(val);
}
