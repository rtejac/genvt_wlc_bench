// Copyright 2017 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Please see the readme.txt for further license information.
#ifndef __MAIN_H__
#define __MAIN_H__

#define FRAME_TIME_RECORDING_WINDOW_SIZE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <GL/glew.h>

// structs/forward declaretions
enum DrawCases
{
    simpleDial = 0,
    singleDrawArrays = 1,
    multiDrawArrays = 2,
    simpleTexture = 3,
    longShader = 4,
    batchDrawArrays = 5,
    simpleEgl = 6,    // future case
    hmi = 7,
    total_cases,
};

struct gl_param
{
    GLuint program;
    GLuint rotation_uniform;
    GLuint projection_uniform;
    GLuint view_uniform;
    GLuint pos;
    GLuint col;
    GLuint trans;
    GLuint tex1;
    GLuint blur_radius;
    GLfloat texture_fetch_radius;
    GLint loop_count;
    bool texture_flat_no_rotate;
};

struct config
{
    int width, height;

    // GL parameters related to scene
    struct gl_param singleDParam;
    struct gl_param multiDParam;
    struct gl_param texParam;
    struct gl_param texBParam;
    struct gl_param flatParam;
    struct gl_param longSParam;
    struct gl_param simpleEParam;
    struct gl_param hmiParam;

    // global scene parameters
    int fullscreen;
    int opaque;
    int frame_sync;
    uint64_t benchmark_time;
    uint32_t frames;
    int offscreen;
    bool no_swapbuffer_call;
    float shortShader_loop_count;
    float dialsShader_loop_count;
    float longShader_loop_count;
    int gl;
    int fixed_fps;
    float current_fps;
    float hmi_overdraw_ratio;
    int hmi_alpha_blending;
    int hmi_texturing;
    int fps_limiter;
    int debug_mode;
    int hmi_3d_render;

    // KPI model parameters
    unsigned int kpi_state;
    unsigned int kpi_settling_time_seconds;
    unsigned int kpi_number_nines;
    unsigned int kpi_exit_on_failure;
    double kpi_max_frametime_milliseconds;
    double kpi_window_milliseconds;
    unsigned int kpi_target_fps;
    unsigned int kpi_min_fps;
};

// forward declarations
class textRender;

// globals
extern DrawCases g_draw_case;
extern bool g_demo_mode;

// pyramid geometry parameters
extern int x_count;
extern int y_count;
extern int z_count;
extern int g_batchSize;
extern textRender g_TextRender;
extern bool g_recordMetrics;

//digits
extern GLuint g_digitTextureID;

//dials
extern GLuint g_dialTexID;
extern GLuint g_needleTexID;

// hmi
extern GLuint g_hmiTexID;

//pyramids
extern GLfloat *pyramid_positions;
extern GLfloat *pyramid_colors_single_draw;
extern GLfloat *pyramid_transforms;

// texture
extern GLuint g_textureID;


// helpers
float calculate_fps(struct config *conf);

// simple dial
void initialize_simpleDial(struct config *conf);

void draw_simpleDial(struct config *conf);

void cleanup_simpleDial(struct config *conf);


// single draw arrays
void initialize_singleDrawArrays(struct config *conf);

void draw_singleDrawArrays(struct config *conf);

void cleanup_singleDrawArrays(struct config *conf);

bool hk_singleDrawArrays(unsigned char key, int x, int y);

// multi draw arrays
void initialize_multiDrawArrays(struct config *conf);

void draw_multiDrawArrays(struct config *conf);

void cleanup_multiDrawArrays(struct config *conf);

bool hk_multiDrawArrays(unsigned char key, int x, int y);

// simple texture
void initialize_simpleTexture(struct config *conf);

void draw_simpleTexture(struct config *conf);

void cleanup_simpleTexture(struct config *conf);

// long shader
void initialize_longShader(struct config *conf);

void draw_longShader(struct config *conf);

void cleanup_longShader(struct config *conf);

// batch draw arrays
void initialize_batchDrawArrays(struct config *conf);

void draw_batchDrawArrays(struct config *conf);

void cleanup_batchDrawArrays(struct config *conf);

// simple egl
void initialize_simple_egl(struct config *conf);

void draw_simple_egl(struct config *conf);

void cleanup_simple_egl(struct config *conf);

// iotg HMI
void initialize_hmi(struct config *conf);

void draw_hmi(struct config *conf);

void cleanup_hmi(struct config *conf);

typedef void ( *render_func )(struct config *conf);

typedef void ( *init_func )(struct config *conf);

typedef void ( *cleanup_func )(struct config *conf);

typedef bool ( *hk_func )(unsigned char key, int x, int y);

void exit_synbench(int val);

struct actions
{
    char title[100];
    init_func init;
    render_func render;
    cleanup_func cleanup;
    hk_func handle_keys;
};

#endif // __MAIN_H__
