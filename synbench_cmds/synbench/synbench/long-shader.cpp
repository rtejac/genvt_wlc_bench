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
#include <signal.h>
#include <fstream>
#include <sys/time.h>

// glm math library
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shaders.h"    // quad_verts/etc
#include "draw-digits.h"

static GLuint frag, vert;

// setup code for long shader
//------------------------------------------------------------------------------
void initialize_longShader(struct config *conf)
{
    GLint status;

    // long shader
    vert = create_shader(vert_shader_textured, GL_VERTEX_SHADER);
    frag = create_shader(frag_shader_long_shader_textured, GL_FRAGMENT_SHADER);

    conf->longSParam.program = glCreateProgram();
    glAttachShader(conf->longSParam.program, frag);
    glAttachShader(conf->longSParam.program, vert);
    glLinkProgram(conf->longSParam.program);

    glGetProgramiv(conf->longSParam.program, GL_LINK_STATUS, &status);
    if (!status)
    {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(conf->longSParam.program, 1000, &len, log);
        fprintf(stderr, "Error: linking:\n%*s\n", len, log);
        exit_synbench(1);
    }

    glUseProgram(conf->longSParam.program);

    // get shader attribute bind locations
    conf->longSParam.pos = glGetAttribLocation(conf->longSParam.program, "pos");
    conf->longSParam.col = glGetAttribLocation(conf->longSParam.program, "color");
    conf->longSParam.trans = glGetAttribLocation(conf->longSParam.program, "trans");
    conf->longSParam.tex1 = glGetAttribLocation(conf->longSParam.program, "texcoord1");

    conf->longSParam.rotation_uniform = glGetUniformLocation(conf->longSParam.program, "model_matrix");
    conf->longSParam.view_uniform = glGetUniformLocation(conf->longSParam.program, "view");
    conf->longSParam.projection_uniform = glGetUniformLocation(conf->longSParam.program, "projection");
    conf->longSParam.loop_count = glGetUniformLocation(conf->longSParam.program, "loop_count");

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

// Test scene: fullscreen quad with variable compute shader
//-----------------------------------------------------------------------------
void draw_longShader(struct config *conf)
{
    static const uint32_t speed_div = 5;
    uint64_t time_now;

    // timer for moving objects and timing frame
    //conf->current_fps = calculate_fps(conf, "long_shader", time_now);
     struct timeval tv;
    gettimeofday(&tv, NULL);
    time_now = (tv.tv_sec * 1000000 + tv.tv_usec) / 1000;

    GLfloat angle = (GLfloat) ((time_now / (speed_div * 2)) % 360);

    glViewport(0, 0, conf->width, conf->height);
    glUseProgram(conf->longSParam.program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_dialTexID);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // orthographic projection matrix
    float r = 1.0f;
    float l = -1.0f;
    float t = 1.0f;
    float b = -1.0f;
    float n = -1.0f;
    float f = 1.0f;

    glm::mat4 ortho_matrix = glm::ortho(l, r, b, t, n, f);
    glUniformMatrix4fv(conf->longSParam.projection_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(ortho_matrix));

    // view matrix
    glm::vec3 v_eye(0.0f, 0.0f, -1.0f);
    glm::vec3 v_center(0.0f, 0.0f, 0.0f);
    glm::vec3 v_up(0.0f, 1.0f, 0.0f);

    glm::mat4 view_matrix = glm::lookAt(v_eye, v_center, v_up);
    glUniformMatrix4fv(conf->longSParam.view_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(view_matrix));

    // set vertex arrays
    glVertexAttribPointer(conf->longSParam.pos, 3, GL_FLOAT, GL_FALSE, 0, quad_verts);
    glVertexAttribPointer(conf->longSParam.col, 4, GL_FLOAT, GL_FALSE, 0, quad_colors);
    glVertexAttribPointer(conf->longSParam.tex1, 2, GL_FLOAT, GL_FALSE, 0, quad_texcoords);
    glEnableVertexAttribArray(conf->longSParam.pos);
    glEnableVertexAttribArray(conf->longSParam.col);
    glEnableVertexAttribArray(conf->longSParam.tex1);

    // fullscreen quad
    glm::mat4 identity_matrix(1.f);
    glm::mat4 model_matrix = glm::rotate(identity_matrix, angle * (3.14159265f / 180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    glUniformMatrix4fv(conf->longSParam.rotation_uniform, 1, GL_FALSE, (GLfloat *) glm::value_ptr(model_matrix));

    // set work amount
    glUniform1f(conf->longSParam.loop_count, conf->longShader_loop_count);

    // draw fullscreen quad
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // draw fps
    //g_TextRender.DrawDigits(conf->current_fps, conf);

    conf->frames++;
}

void cleanup_longShader(struct config *conf)
{
    glDeleteTextures(1, &g_dialTexID);
    glDeleteTextures(1, &g_needleTexID);
    glDeleteShader(vert);
    glDeleteShader(frag);
    glDeleteProgram(conf->longSParam.program);
}
