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
#include <algorithm>    // std::max
#include <sys/time.h>

#include "shaders.h" // quad_verts/etc
#include "draw-digits.h"

// glm math library
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

static GLuint frag, vert;

// setup code for batch draw
//------------------------------------------------------------------------------
void initialize_batchDrawArrays(struct config *conf)
{
    GLint status;

    // batch_draw shader
    if (0 == conf->singleDParam.program)
    {
        vert = create_shader(vert_shader_single, GL_VERTEX_SHADER);
        frag = create_shader(frag_shader_short_loop, GL_FRAGMENT_SHADER);

        conf->singleDParam.program = glCreateProgram();
        glAttachShader(conf->singleDParam.program, frag);
        glAttachShader(conf->singleDParam.program, vert);
        glLinkProgram(conf->singleDParam.program);

        glGetProgramiv(conf->singleDParam.program, GL_LINK_STATUS, &status);
        if (!status)
        {
            char log[1000];
            GLsizei len;
            glGetProgramInfoLog(conf->singleDParam.program, 1000, &len, log);
            fprintf(stderr, "Error: linking:\n%*s\n", len, log);
            exit_synbench(1);
        }
    }
    glUseProgram(conf->singleDParam.program);

    // get shader attribute location
    conf->singleDParam.pos = glGetAttribLocation(conf->singleDParam.program, "pos");
    conf->singleDParam.col = glGetAttribLocation(conf->singleDParam.program, "color");
    conf->singleDParam.trans = glGetAttribLocation(conf->singleDParam.program, "trans");

    conf->singleDParam.rotation_uniform =
            glGetUniformLocation(conf->singleDParam.program, "model_matrix");
    conf->singleDParam.view_uniform =
            glGetUniformLocation(conf->singleDParam.program, "view");
    conf->singleDParam.projection_uniform =
            glGetUniformLocation(conf->singleDParam.program, "projection");
    conf->singleDParam.loop_count =
            glGetUniformLocation(conf->singleDParam.program, "loop_count");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


// Test scene: draw full screen of pyramids with single draw call
//------------------------------------------------------------------------------
void draw_batchDrawArrays(struct config *conf)
{
    uint64_t time_now;
    static const uint32_t speed_div = 5;

    // timer for moving objects and timing frame
    //conf->current_fps = calculate_fps(conf, "batch_draw", time_now);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_now = (tv.tv_sec * 1000000 + tv.tv_usec) / 1000;

    GLfloat angle = (GLfloat) ((time_now / speed_div) % 360);  // * M_PI / 180.0;


    // start the GL loop
    glUseProgram(conf->singleDParam.program);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // projection matrix
    glm::mat4 proj_matrix = glm::frustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 500.0f);
    glUniformMatrix4fv(conf->singleDParam.projection_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(proj_matrix));

    // view matrix - make frustum so it bounds the pyramid grid tightly
    int maxdim = std::max(x_count, y_count);
    float xCoord = x_count * 1.5f - 1.0f;
    float yCoord = y_count * 1.5f - 1.0f;

    glm::vec3 v_eye(xCoord, yCoord, -3.0f * (maxdim / 2));
    glm::vec3 v_center(xCoord, yCoord, z_count * 1.5f);
    glm::vec3 v_up(0.0f, 1.0f, 0.0f);

    glm::mat4 view_matrix = glm::lookAt(v_eye, v_center, v_up);
    glUniformMatrix4fv(conf->singleDParam.view_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(view_matrix));

    // set the vertex buffers
    glVertexAttribPointer(conf->singleDParam.pos, 3, GL_FLOAT, GL_FALSE, 0, pyramid_positions);
    glVertexAttribPointer(conf->singleDParam.col, 4, GL_FLOAT, GL_FALSE, 0, pyramid_colors_single_draw);
    glVertexAttribPointer(conf->singleDParam.trans, 3, GL_FLOAT, GL_FALSE, 0, pyramid_transforms);

    glEnableVertexAttribArray(conf->singleDParam.pos);
    glEnableVertexAttribArray(conf->singleDParam.col);
    glEnableVertexAttribArray(conf->singleDParam.trans);

    // set the number of shader 'work' loops
    glUniform1f(conf->singleDParam.loop_count, conf->shortShader_loop_count);

    // set the pyramid rotation matrix
    glm::mat4 identity_matrix(1.f);
    glm::mat4 model_matrix = glm::rotate(identity_matrix, angle * (3.14f / 180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(conf->singleDParam.rotation_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(model_matrix));

    // check validity of batching
    if (g_batchSize > (x_count * y_count * z_count))
    {
        g_batchSize = x_count * y_count * z_count;
    }

    if (g_batchSize < 1)
    {
        g_batchSize = 1;
    }

    // draw
    float block_size = (float) (x_count * y_count * z_count);
    block_size /= g_batchSize;
    if (block_size < 1.0f)
    {
        block_size = 1.0f;
    }
    GLuint ublock_size = (GLuint) block_size;
    //printf("block size, # pyramids per group: %d\n", ublock_size);
    //printf("g_batchSize: %d\n", g_batchSize);
    //printf("\n");
    GLuint draw_size = (18 * ublock_size);
    GLuint start_index = 0;

    start_index = 0;
    GLint total_count = 0;
    for (int i = 0; i < g_batchSize; i++)
    {
        //glDrawArrays(GL_TRIANGLES, 0, 18 * (x_count * y_count * z_count));
        //printf("%d: %d - %d, ", i, start_index, start_index+18*ublock_size);

        glDrawArrays(GL_TRIANGLES, start_index, draw_size);

        start_index += draw_size;
        total_count += ublock_size;
    }

    // handle the odd-sized final batch (if there is one)
    if (total_count < (x_count * y_count * z_count))
    {
        GLuint final_block_size = 18 * ((x_count * y_count * z_count) - (ublock_size * g_batchSize));
        //printf(" final block: %d - %d \n", start_index, start_index+final_block_size);
        glDrawArrays(GL_TRIANGLES, start_index, final_block_size);
    }


    glDisableVertexAttribArray(conf->singleDParam.pos);
    glDisableVertexAttribArray(conf->singleDParam.col);
    glDisableVertexAttribArray(conf->singleDParam.trans);

    // render the FPS
    //g_TextRender.DrawDigits(conf->current_fps, conf);

    conf->frames++;
}

void cleanup_batchDrawArrays(struct config *conf)
{
    glDeleteShader(vert);
    glDeleteShader(frag);
    glDeleteProgram(conf->singleDParam.program);
}
