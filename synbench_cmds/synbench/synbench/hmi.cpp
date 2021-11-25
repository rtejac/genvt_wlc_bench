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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <cmath>
#include <cstdio> // fopen
#include <fstream>
#include <algorithm>    // std::max

// glm math library
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shaders.h" // quad_verts/etc
#include "draw-digits.h"
#include "os.h"

// textures
#include "main.h"

using namespace std;

static GLuint frag, vert;

void initialize_hmi(struct config *conf)
{
    GLint status;

    //
    // texParam -> needed for FPS display
    //
    if (0 == conf->texParam.program)
    {
        // textured shader
        vert = create_shader(vert_shader_textured, GL_VERTEX_SHADER);
        frag = create_shader(frag_shader_textured, GL_FRAGMENT_SHADER);

        conf->texParam.program = glCreateProgram();
        glAttachShader(conf->texParam.program, frag);
        glAttachShader(conf->texParam.program, vert);
        glLinkProgram(conf->texParam.program);

        glGetProgramiv(conf->texParam.program, GL_LINK_STATUS, &status);
        if (!status)
        {
            char log[1000];
            GLsizei len;
            glGetProgramInfoLog(conf->texParam.program, 1000, &len, log);
            fprintf(stderr, "Error: linking:\n%*s\n", len, log);
            exit_synbench(1);
        }

        glUseProgram(conf->texParam.program);

        // get shader attribute bind locations
        conf->texParam.pos = glGetAttribLocation(conf->texParam.program, "pos");
        conf->texParam.col = glGetAttribLocation(conf->texParam.program, "color");
        conf->texParam.tex1 = glGetAttribLocation(conf->texParam.program, "texcoord1");

        conf->texParam.rotation_uniform =
                glGetUniformLocation(conf->texParam.program, "model_matrix");
        conf->texParam.view_uniform =
                glGetUniformLocation(conf->texParam.program, "view");
        conf->texParam.projection_uniform =
                glGetUniformLocation(conf->texParam.program, "projection");
        conf->texParam.loop_count =
                glGetUniformLocation(conf->texParam.program, "loop_count");
    }

    //
    // needed for hmi rendering
    //
    vert = create_shader(vert_shader_textured, GL_VERTEX_SHADER);
    if (conf->hmi_texturing)
    {
        frag = create_shader(frag_shader_textured, GL_FRAGMENT_SHADER);
    } else
    {
        frag = create_shader(frag_shader_single_color_blended, GL_FRAGMENT_SHADER);
    }

    conf->hmiParam.program = glCreateProgram();
    glAttachShader(conf->hmiParam.program, frag);
    glAttachShader(conf->hmiParam.program, vert);
    glLinkProgram(conf->hmiParam.program);

    glGetProgramiv(conf->hmiParam.program, GL_LINK_STATUS, &status);
    if (!status)
    {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(conf->hmiParam.program, 1000, &len, log);
        fprintf(stderr, "Error: linking:\n%*s\n", len, log);
        exit_synbench(1);
    }

    glUseProgram(conf->hmiParam.program);

    // get shader attribute bind locations
    conf->hmiParam.pos = glGetAttribLocation(conf->hmiParam.program, "pos");
    conf->hmiParam.col = glGetAttribLocation(conf->hmiParam.program, "color");
    conf->hmiParam.tex1 = glGetAttribLocation(conf->hmiParam.program, "texcoord1");

    conf->hmiParam.rotation_uniform =
            glGetUniformLocation(conf->hmiParam.program, "model_matrix");
    conf->hmiParam.view_uniform =
            glGetUniformLocation(conf->hmiParam.program, "view");
    conf->hmiParam.projection_uniform =
            glGetUniformLocation(conf->hmiParam.program, "projection");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    if (conf->hmi_alpha_blending)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else
    {
        glDisable(GL_BLEND);
    }

    if (conf->hmi_texturing)
    {
        registerTexture(g_hmiTexID, 2048, 1024, "textures/gray_texture_background_2048x1024.data", 4);
    }

    if (conf->hmi_3d_render)
    {
        initialize_singleDrawArrays(conf);
    }
}

// draw the hmi
//------------------------------------------------------------------------------
void draw_hmi(struct config *conf)
{
    //fuint64_t time_now;
    //float fps = calculate_fps(conf, "hmi", time_now);

    glUseProgram(conf->hmiParam.program);
    glDisable(GL_CULL_FACE);

    if (conf->hmi_alpha_blending)
    {
        glEnable(GL_BLEND);
    } else
    {
        glDisable(GL_BLEND);
    }

    if (conf->hmi_texturing)
    {
        glEnable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_hmiTexID);
    }

    // orthographic projection matrix
    float r = 1.0f;
    float l = -1.0f;
    float t = 1.0f;
    float b = -1.0f;
    float n = -1.0f;
    float f = 1.0f;

    glm::mat4 _ortho_matrix = glm::ortho(l, r, b, t, n, f);
    glUniformMatrix4fv(conf->hmiParam.projection_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(_ortho_matrix));

    // view matrix
    glm::vec3 v_eye(0.0f, 0.0f, -1.0f);
    glm::vec3 v_center(0.0f, 0.0f, 0.0f);
    glm::vec3 v_up(0.0f, 1.0f, 0.0f);

    glm::mat4 view_matrix = glm::lookAt(v_eye, v_center, v_up);
    glUniformMatrix4fv(conf->hmiParam.view_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(view_matrix));

    glVertexAttribPointer(conf->hmiParam.pos, 3, GL_FLOAT, GL_FALSE, 0, quad_verts);
    glVertexAttribPointer(conf->hmiParam.col, 4, GL_FLOAT, GL_FALSE, 0, quad_colors);
    glVertexAttribPointer(conf->hmiParam.tex1, 2, GL_FLOAT, GL_FALSE, 0, quad_texcoords);
    glEnableVertexAttribArray(conf->hmiParam.pos);
    glEnableVertexAttribArray(conf->hmiParam.col);
    glEnableVertexAttribArray(conf->hmiParam.tex1);

    //
    // start drawing
    //
    unsigned long pixelsWritten = 0;
    unsigned long bytesWritten = 0;
    unsigned long bytesRead = 0;

    // 1: clear the render target and depth buffer
    glViewport(0, 0, conf->width, conf->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pixelsWritten += conf->width * conf->height; // full-screen clear
    bytesWritten += conf->width * conf->height * 4;

    // 2: draw the number of full screen quads required for the overdraw ratio
    unsigned int maxIters = (unsigned int) floorf(
            conf->hmi_overdraw_ratio - 1.0f); // subtract off the clear - it accounts for 1.0 of the total overdraw
    for (unsigned int x = 0; x < maxIters; x++)
    {
        glm::mat4 model_matrix(1.f);
        glm::mat4 identity_matrix(1.f);
        glUniformMatrix4fv(conf->hmiParam.rotation_uniform, 1, GL_FALSE, (GLfloat *) glm::value_ptr(model_matrix));

        glDrawArrays(GL_TRIANGLES, 0, 6);
        pixelsWritten += conf->width * conf->height; // full-screen quad
        bytesRead += conf->width * conf->height * 4;
        bytesWritten += conf->width * conf->height * 4;
    }

    // 3: draw one finally smaller quad, to equal the overdraw ratio
    float overdrawFractional = conf->hmi_overdraw_ratio - floorf(conf->hmi_overdraw_ratio);
    if (0.0f < overdrawFractional)
    {
        glm::mat4 model_matrix(1.f);
        glm::mat4 identity_matrix(1.f);

        model_matrix = glm::scale(model_matrix, glm::vec3(overdrawFractional, 1.0f, 1.0f));
        glUniformMatrix4fv(conf->hmiParam.rotation_uniform, 1, GL_FALSE, (GLfloat *) glm::value_ptr(model_matrix));
        glDrawArrays(GL_TRIANGLES, 0, 6);
        pixelsWritten += (unsigned long) ((conf->width * overdrawFractional) * conf->height);  // partial-screen quad
        if (conf->hmi_alpha_blending)
        {
            bytesRead += (unsigned long) ((conf->width * overdrawFractional) * conf->height * 4);
        }
        bytesWritten += (unsigned long) ((conf->width * overdrawFractional) * conf->height * 4);
    }

    if (conf->hmi_3d_render)
    {
        draw_singleDrawArrays(conf);
    }

    // 4: draw fps indicator
//   /g_TextRender.DrawDigits(fps, conf);
    conf->frames++;

    double overdraw_ratio = (double) pixelsWritten / (double) (conf->width * conf->height);
    double pixel_rate = (double) pixelsWritten * conf->current_fps / 1024.0f / 1024.0f;
    double mem_bw = ((double) bytesRead + (double) bytesWritten) * conf->current_fps / 1024.0f / 1024.0f;

    int int_fps = ceil(conf->current_fps);
    if (int_fps > 0 && (conf->frames % int_fps) == 0)
    {
        printf("hmi: Overdraw Ratio: %0.02f\n", overdraw_ratio);
        printf("hmi: Pixel Rate: %0.02f MP/s\n", pixel_rate);
        printf("hmi: Memory Bandwidth: %0.02f MB/s\n", mem_bw);
    }
}

void cleanup_hmi(struct config *conf)
{
    glDeleteShader(vert);
    glDeleteShader(frag);
    glDeleteProgram(conf->hmiParam.program);
    glDeleteProgram(conf->texParam.program);

    if (conf->hmi_3d_render)
    {
        cleanup_singleDrawArrays(conf);
    }
}
