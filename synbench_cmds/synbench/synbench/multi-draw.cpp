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
#include <fstream>
#include <sys/time.h>

#include <algorithm>    // std::max

#include "shaders.h" // quad_verts/etc
#include "draw-digits.h"

// glm math library
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

static GLuint frag, vert;

// setup code for multi-draw
//------------------------------------------------------------------------------
void initialize_multiDrawArrays(struct config *conf)
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

    // multi_draw shader
    vert = create_shader(vert_shader_multi, GL_VERTEX_SHADER);
    frag = create_shader(frag_shader_short_loop, GL_FRAGMENT_SHADER);

    conf->multiDParam.program = glCreateProgram();
    glAttachShader(conf->multiDParam.program, frag);
    glAttachShader(conf->multiDParam.program, vert);
    glLinkProgram(conf->multiDParam.program);

    glGetProgramiv(conf->multiDParam.program, GL_LINK_STATUS, &status);
    if (!status)
    {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(conf->multiDParam.program, 1000, &len, log);
        fprintf(stderr, "Error: linking:\n%*s\n", len, log);
        exit_synbench(1);
    }
    glUseProgram(conf->multiDParam.program);

    // get shader attribute location
    conf->multiDParam.pos = glGetAttribLocation(conf->multiDParam.program, "pos");
    conf->multiDParam.col = glGetAttribLocation(conf->multiDParam.program, "color");
    conf->multiDParam.trans = glGetAttribLocation(conf->multiDParam.program, "trans");
    conf->multiDParam.loop_count = glGetUniformLocation(conf->multiDParam.program, "loop_count");

    conf->multiDParam.rotation_uniform =
            glGetUniformLocation(conf->multiDParam.program, "model_matrix");
    conf->multiDParam.view_uniform =
            glGetUniformLocation(conf->multiDParam.program, "view");
    conf->multiDParam.projection_uniform =
            glGetUniformLocation(conf->multiDParam.program, "projection");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Test scene: seperate draw call for each pyramid
//------------------------------------------------------------------------------
void draw_multiDrawArrays(struct config *conf)
{
    uint64_t time_now;
    static const uint32_t speed_div = 5;

    // timer for moving objects and timing frame
    //conf->current_fps = calculate_fps(conf, "multi_draw", time_now);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_now = (tv.tv_sec * 1000000 + tv.tv_usec) / 1000;

    GLfloat angle = (GLfloat) ((time_now / speed_div) % 360);  // * M_PI / 180.0;

    // start GL rendering loop
    glUseProgram(conf->multiDParam.program);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // projection matrix
    glm::mat4 proj_matrix = glm::frustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 500.0f);
    glUniformMatrix4fv(conf->multiDParam.projection_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(proj_matrix));

    // view matrix
    int maxdim = std::max(x_count, y_count);
    float xCoord = x_count * 1.5f - 1.0f;
    float yCoord = y_count * 1.5f - 1.0f;

    glm::vec3 v_eye(xCoord, yCoord, -3.0f * (maxdim / 2));
    glm::vec3 v_center(xCoord, yCoord, z_count * 1.5f);
    glm::vec3 v_up(0.0f, 1.0f, 0.0f);

    glm::mat4 view_matrix = glm::lookAt(v_eye, v_center, v_up);
    glUniformMatrix4fv(conf->multiDParam.view_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(view_matrix));

    // vertex attribute pointers
    glVertexAttribPointer(conf->multiDParam.pos, 3, GL_FLOAT, GL_FALSE, 0, pyramid_verts);
    glVertexAttribPointer(conf->multiDParam.col, 4, GL_FLOAT, GL_FALSE, 0, pyramid_colors);
    glEnableVertexAttribArray(conf->multiDParam.pos);
    glEnableVertexAttribArray(conf->multiDParam.col);

    // set the number of shader loops
    glUniform1f(conf->multiDParam.loop_count, conf->shortShader_loop_count);

    // draw the grid like mad
    for (int z = (z_count - 1); z >= 0; z--)
    {
        for (int x = 0; x < x_count; x++)
        {
            for (int y = 0; y < y_count; y++)
            {
                glm::mat4 model_matrix(1.f);
                model_matrix = glm::translate(model_matrix, glm::vec3((float) x * 3, (float) y * 3, (float) z * 3));
                model_matrix = glm::rotate(model_matrix, angle * 3.14f / 180.0f, glm::vec3(0.0f, 1.0f, 0.0f));
                glUniformMatrix4fv(conf->multiDParam.rotation_uniform, 1, GL_FALSE,
                                   (GLfloat *) glm::value_ptr(model_matrix));

                glDrawArrays(GL_TRIANGLES, 0, 18);
            }
        }
    }

    glDisableVertexAttribArray(conf->multiDParam.pos);
    glDisableVertexAttribArray(conf->multiDParam.col);

    // render fps digits
    //g_TextRender.DrawDigits(conf->current_fps, conf);

    conf->frames++;
}

void cleanup_multiDrawArrays(struct config *conf)
{
    glDeleteShader(vert);
    glDeleteShader(frag);
    glDeleteProgram(conf->texParam.program);
}

bool hk_multiDrawArrays(unsigned char key, int x, int y)
{
    bool ret_val = false;
    int delta = 0;
    switch (key)
    {
        //Increase the x, y and z count of pyramids by 5 each time the + key is
        //pressed
        case '+':
            delta = 5;
            break;
            //Decrease the x, y and z count of pyramids by 5 each time the - key is
            //pressed
        case '-':
            delta = -5;
            break;
            //Increase the x, y and z count of pyramids by 1 each time the ) key is
            //pressed
        case ')':
            delta = 1;
            break;
            //Decrease the x, y and z count of pyramids by 1 each time the ( key is
            //pressed
        case '(':
            delta = -1;
            break;
    }

    printf("%c was pressed\n", key);
    if (x_count + delta <= 0)
    {
        printf("Error: Can't reduce the counters to 0 or negative\n");
    } else
    {
        x_count += delta;
        y_count += delta;
        z_count += delta;
        printf("x_count = %d, y_count = %d, z_count = %d\n", x_count, y_count, z_count);
        ret_val = true;
    }

    return ret_val;
}
