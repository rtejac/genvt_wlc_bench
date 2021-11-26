#include <stdint.h>
#include <sys/time.h>
#include "shaders.h" // quad_verts/etc
#include "draw-digits.h"

using namespace std;

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

static GLuint frag, vert;

void initialize_simple_egl(struct config *conf)
{
    static const char *vert_shader_text =
            "uniform mat4 rotation;\n"
            "attribute vec4 pos;\n"
            "attribute vec4 color;\n"
            "varying vec4 v_color;\n"
            "void main() {\n"
            "  gl_Position = rotation * pos;\n"
            "  v_color = color;\n"
            "}\n";

    static const char *frag_shader_text =
            "precision mediump float;\n"
            "varying vec4 v_color;\n"
            "void main() {\n"
            "  gl_FragColor = v_color;\n"
            "}\n";

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

    frag = create_shader(frag_shader_text, GL_FRAGMENT_SHADER);
    vert = create_shader(vert_shader_text, GL_VERTEX_SHADER);

    conf->simpleEParam.program = glCreateProgram();
    glAttachShader(conf->simpleEParam.program, frag);
    glAttachShader(conf->simpleEParam.program, vert);
    glLinkProgram(conf->simpleEParam.program);

    glGetProgramiv(conf->simpleEParam.program, GL_LINK_STATUS, &status);
    if (!status)
    {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(conf->simpleEParam.program, 1000, &len, log);
        fprintf(stderr, "Error: linking:\n%.*s\n", len, log);
        exit_synbench(1);
    }

    glUseProgram(conf->simpleEParam.program);

    conf->simpleEParam.pos = 0;
    conf->simpleEParam.col = 1;

    glBindAttribLocation(conf->simpleEParam.program, conf->simpleEParam.pos, "pos");
    glBindAttribLocation(conf->simpleEParam.program, conf->simpleEParam.col, "color");
    glLinkProgram(conf->simpleEParam.program);

    conf->simpleEParam.rotation_uniform =
            glGetUniformLocation(conf->simpleEParam.program, "rotation");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void draw_simple_egl(struct config *conf)
{
    static const GLfloat verts[3][2] = {
            {-0.5, -0.5},
            {0.5,  -0.5},
            {0,    0.5}
    };
    static const GLfloat colors[3][3] = {
            {1, 0, 0},
            {0, 1, 0},
            {0, 0, 1}
    };
    GLfloat angle;
    GLfloat rotation[4][4] = {
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1}
    };
    static const uint32_t speed_div = 5;
   uint64_t time_now;

    //calculate_fps(conf, "simple-egl", time_now);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_now = (tv.tv_sec * 1000000 + tv.tv_usec) / 1000;

    angle = (GLfloat) ((time_now / speed_div) % 360 * M_PI / 180.0);
    rotation[0][0] = (GLfloat) cos(angle);
    rotation[0][2] = (GLfloat) sin(angle);
    rotation[2][0] = (GLfloat) -sin(angle);
    rotation[2][2] = (GLfloat) cos(angle);

    glViewport(0, 0, conf->width, conf->height);

    glUniformMatrix4fv(conf->simpleEParam.rotation_uniform, 1, GL_FALSE,
                       (GLfloat *) rotation);

    glClearColor(0.0, 0.0, 0.0, 0.5);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glVertexAttribPointer(conf->simpleEParam.pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glVertexAttribPointer(conf->simpleEParam.col, 3, GL_FLOAT, GL_FALSE, 0, colors);
    glEnableVertexAttribArray(conf->simpleEParam.pos);
    glEnableVertexAttribArray(conf->simpleEParam.col);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(conf->simpleEParam.pos);
    glDisableVertexAttribArray(conf->simpleEParam.col);

    conf->frames++;
}

void cleanup_simple_egl(struct config *conf)
{
    glDeleteShader(vert);
    glDeleteShader(frag);
    glDeleteProgram(conf->singleDParam.program);
}
