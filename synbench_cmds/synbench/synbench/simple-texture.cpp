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

// glm math library
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shaders.h" // quad_verts/etc
#include "draw-digits.h"

extern void initialize_simpleDial(struct config *conf);

static GLuint frag, vert;

// setup code for spinning large texture
//------------------------------------------------------------------------------
void initialize_simpleTexture(struct config *conf)
{
    GLint status;

    // also loaded by dials
    // A simple texture sampling shader
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

    // flat grey pixel shader - for testing without using texture bandwidth
    vert = create_shader(vert_shader_textured, GL_VERTEX_SHADER);
    frag = create_shader(frag_shader_single_color, GL_FRAGMENT_SHADER);

    conf->flatParam.program = glCreateProgram();
    glAttachShader(conf->flatParam.program, frag);
    glAttachShader(conf->flatParam.program, vert);
    glLinkProgram(conf->flatParam.program);

    glGetProgramiv(conf->flatParam.program, GL_LINK_STATUS, &status);
    if (!status)
    {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(conf->flatParam.program, 1000, &len, log);
        fprintf(stderr, "Error: linking:\n%*s\n", len, log);
        exit_synbench(1);
    }

    glUseProgram(conf->flatParam.program);

    // get shader attribute bind locations
    conf->flatParam.pos = glGetAttribLocation(conf->flatParam.program, "pos");
    conf->flatParam.col = glGetAttribLocation(conf->flatParam.program, "color");
    conf->flatParam.tex1 = glGetAttribLocation(conf->flatParam.program, "texcoord1");

    conf->flatParam.rotation_uniform =
            glGetUniformLocation(conf->flatParam.program, "model_matrix");
    conf->flatParam.view_uniform =
            glGetUniformLocation(conf->flatParam.program, "view");
    conf->flatParam.projection_uniform =
            glGetUniformLocation(conf->flatParam.program, "projection");
    conf->flatParam.loop_count =
            glGetUniformLocation(conf->flatParam.program, "loop_count");

    // blur shader
    vert = create_shader(vert_shader_textured, GL_VERTEX_SHADER);
    frag = create_shader(frag_shader_blur_textured, GL_FRAGMENT_SHADER);

    conf->texBParam.program = glCreateProgram();
    glAttachShader(conf->texBParam.program, frag);
    glAttachShader(conf->texBParam.program, vert);
    glLinkProgram(conf->texBParam.program);

    glGetProgramiv(conf->texBParam.program, GL_LINK_STATUS, &status);
    if (!status)
    {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(conf->texBParam.program, 1000, &len, log);
        fprintf(stderr, "Error: linking:\n%*s\n", len, log);
        exit_synbench(1);
    }

    glUseProgram(conf->texBParam.program);

    // get shader attribute bind locations
    conf->texBParam.pos = glGetAttribLocation(conf->texBParam.program, "pos");
    conf->texBParam.col = glGetAttribLocation(conf->texBParam.program, "color");
    conf->texBParam.tex1 = glGetAttribLocation(conf->texBParam.program, "texcoord1");

    conf->texBParam.rotation_uniform =
            glGetUniformLocation(conf->texBParam.program, "model_matrix");
    conf->texBParam.view_uniform =
            glGetUniformLocation(conf->texBParam.program, "view");
    conf->texBParam.projection_uniform =
            glGetUniformLocation(conf->texBParam.program, "projection");
    conf->texBParam.loop_count =
            glGetUniformLocation(conf->texBParam.program, "loop_count");
    conf->texBParam.blur_radius =
            glGetUniformLocation(conf->texBParam.program, "blur_radius");

    // load textures
    registerTexture(g_textureID, 1024, 1024, "textures/store_1024x1024.bin", 2);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Test scene: fullscreen quad with flat or blur shader
//------------------------------------------------------------------------------
void draw_simpleTexture(struct config *conf)
{
    //uint64_t time_now;

    if (conf->texBParam.texture_flat_no_rotate)
    {
        glUseProgram(conf->flatParam.program);
    } else
    {
        glUseProgram(conf->texBParam.program);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_textureID);

    // timer for moving objects and timing frame
    //conf->current_fps = calculate_fps(conf, "simple_texture", time_now);

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

    glm::mat4 model_matrix;

    glUniform1f(conf->texBParam.blur_radius, conf->texBParam.texture_fetch_radius);

    // set up buffers for draw
    glVertexAttribPointer(conf->texParam.pos, 3, GL_FLOAT, GL_FALSE, 0, quad_verts);
    glVertexAttribPointer(conf->texParam.col, 4, GL_FLOAT, GL_FALSE, 0, quad_colors);
    glVertexAttribPointer(conf->texParam.tex1, 2, GL_FLOAT, GL_FALSE, 0, quad_texcoords);
    glEnableVertexAttribArray(conf->texParam.pos);
    glEnableVertexAttribArray(conf->texParam.col);
    glEnableVertexAttribArray(conf->texParam.tex1);

    // fullscreen quad
    if (conf->texParam.texture_flat_no_rotate)
    {
        model_matrix = glm::mat4(1.f);
    } else
    {
        //model_matrix = rotate(angle, 0.0f, 1.0f, 0.0f);
        model_matrix = glm::mat4(1.f);
    }

    glUniformMatrix4fv(conf->texParam.rotation_uniform, 1, GL_FALSE,
                       (GLfloat *) glm::value_ptr(model_matrix));

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // handle flips/weston
    //g_TextRender.DrawDigits(conf->current_fps, conf);

    conf->frames++;
}

void cleanup_simpleTexture(struct config *conf)
{
    glDeleteTextures(1, &g_textureID);
    glDeleteShader(vert);
    glDeleteShader(frag);
    glDeleteProgram(conf->texParam.program);
}
