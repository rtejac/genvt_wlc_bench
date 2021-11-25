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

// setup code for dual dial display
//------------------------------------------------------------------------------
void initialize_simpleDial(struct config *conf)
{
    GLint status;

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

    // load textures
    if (0 == g_dialTexID)
    {
        registerTexture(g_dialTexID, 1024, 1024, "textures/dialface_1024x1024.bin", 2);
    }

    registerTexture(g_needleTexID, 32, 128, "textures/needle_32x128.bin", 1);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// draw the dual dial display
//------------------------------------------------------------------------------
void draw_simpleDial(struct config *conf)
{
    //uint64_t time_now;

    static bool first_frame = true;
    static float left_dial_angle = 0.0f;
    static GLfloat angle = 0.0f;

    glUseProgram(conf->texParam.program);

    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_dialTexID);

    static struct timeval Startuptv;
    struct timeval tv;

    if (first_frame)
    {
        // record the initial startup time
        // so that no matter what happens, our dials will spin smoothly
        // and independent of framerate or hitches
        gettimeofday(&Startuptv, NULL);
        first_frame = false;
    }

    // timer for moving objects and timing frame
    //conf->current_fps = calculate_fps(conf, "dials", time_now);

    // startup elapsed
    gettimeofday(&tv, NULL);
    long ElapsedTime = (tv.tv_sec - Startuptv.tv_sec) * 1000 + (tv.tv_usec - Startuptv.tv_usec) / 1000;

    double bbR = 0.005;
    double bbL = 0.1;
    double cc = (ElapsedTime * bbR);
    double dd = (ElapsedTime * bbL);

    angle = (GLfloat) cc;
    if (angle > 360.0)
    {
        angle = angle - (int) (cc / 360.0) * 360;
        //printf("angle: %d\n", angle);
    }

    left_dial_angle = (float) dd;
    if (angle > 360.0)
    {
        left_dial_angle = left_dial_angle - (int) (dd / 360.0) * 360;
    }


    glViewport(0, 0, conf->width, conf->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // orthographic projection matrix
    float r = 1.0f;
    float l = -1.0f;
    float t = 1.0f;
    float b = -1.0f;
    float n = -1.0f;
    float f = 1.0f;

    glm::mat4 _ortho_matrix = glm::ortho(l, r, b, t, n, f);
    glUniformMatrix4fv(conf->texParam.projection_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(_ortho_matrix));

    // view matrix
    glm::vec3 v_eye(0.0f, 0.0f, -1.0f);
    glm::vec3 v_center(0.0f, 0.0f, 0.0f);
    glm::vec3 v_up(0.0f, 1.0f, 0.0f);

    glm::mat4 view_matrix = glm::lookAt(v_eye, v_center, v_up);
    glUniformMatrix4fv(conf->texParam.view_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(view_matrix));


    float aspect_ratio = (float) conf->height / (float) conf->width;
    float shrink = 3.0f / 4.0f;    // aspect ratio of the needle texture

    // set number of draw loops
    // set the number of shader 'work' loops
    glUniform1f(conf->texParam.loop_count, conf->dialsShader_loop_count);


    glVertexAttribPointer(conf->texParam.pos, 3, GL_FLOAT, GL_FALSE, 0, quad_verts);
    glVertexAttribPointer(conf->texParam.col, 4, GL_FLOAT, GL_FALSE, 0, quad_colors);
    glVertexAttribPointer(conf->texParam.tex1, 2, GL_FLOAT, GL_FALSE, 0, quad_texcoords);
    glEnableVertexAttribArray(conf->texParam.pos);
    glEnableVertexAttribArray(conf->texParam.col);
    glEnableVertexAttribArray(conf->texParam.tex1);

    // left dial
    glm::mat4 model_matrix(1.f);
    glm::mat4 identity_matrix(1.f);
    model_matrix = glm::translate(identity_matrix, glm::vec3(-0.5f, 0.0f, 0.0f));
    model_matrix = glm::scale(model_matrix, glm::vec3(aspect_ratio * shrink, 1.0f * shrink, 1.0f));
    glUniformMatrix4fv(conf->texParam.rotation_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(model_matrix));
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // right dial
    model_matrix = glm::translate(identity_matrix, glm::vec3(0.5f, 0.0f, 0.0f));
    model_matrix = glm::scale(model_matrix, glm::vec3(aspect_ratio * shrink, 1.0f * shrink, 1.0f));
    glUniformMatrix4fv(conf->texParam.rotation_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(model_matrix));
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // left needle
    glUniform1f(conf->texParam.loop_count, 0);    // no work for needles, keep them normal
    model_matrix = glm::translate(identity_matrix, glm::vec3(-0.5f, 0.0f, 0.0f));
    model_matrix = glm::scale(model_matrix, glm::vec3(aspect_ratio * shrink, 1.0f * shrink, 1.0f));
    model_matrix = glm::rotate(model_matrix, left_dial_angle * (3.14159265f / 180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    model_matrix = glm::translate(model_matrix, glm::vec3(0.0f, 0.5f, 0.0f));
    model_matrix = glm::scale(model_matrix, glm::vec3(0.05f, 0.3f, 1.0f));
    glUniformMatrix4fv(conf->texParam.rotation_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(model_matrix));

    glBindTexture(GL_TEXTURE_2D, g_needleTexID);
    glDrawArrays(GL_TRIANGLES, 0, 6);


    // right needle
    model_matrix = glm::translate(identity_matrix, glm::vec3(0.5f, 0.0f, 0.0f));
    model_matrix = glm::scale(model_matrix, glm::vec3(aspect_ratio * shrink, 1.0f * shrink, 1.0f));
    model_matrix = glm::rotate(model_matrix, angle * (3.14159265f / 180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    model_matrix = glm::translate(model_matrix, glm::vec3(0.0f, 0.5f, 0.0f));
    model_matrix = glm::scale(model_matrix, glm::vec3(0.05f, 0.3f, 1.0f));
    glUniformMatrix4fv(conf->texParam.rotation_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(model_matrix));
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // draw fps
    //g_TextRender.DrawDigits(conf->current_fps, conf);

    conf->frames++;
}

void cleanup_simpleDial(struct config *conf)
{
    glDeleteTextures(1, &g_dialTexID);
    glDeleteTextures(1, &g_needleTexID);
    glDeleteShader(vert);
    glDeleteShader(frag);
    glDeleteProgram(conf->texParam.program);
}
