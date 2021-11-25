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

#define GL_GLEXT_PROTOTYPES

#include <stdio.h>
#include <GL/glew.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "shaders.h"

#define HEADER_PIXEL(data, pixel) {\
pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
data += 4; \
}


using namespace std;

//------------------------------------------------------------------------------
GLuint create_shader(const char *source, GLenum shader_type)
{
    GLuint shader;
    GLint status;

    shader = glCreateShader(shader_type);
    assert(shader != 0);

    glShaderSource(shader, 1, (const char **) &source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        char log[1000];
        GLsizei len;
        glGetShaderInfoLog(shader, 1000, &len, log);
        fprintf(stderr, "Error: compiling %s: %*s\n",
                shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                len, log);
        exit_synbench(1);
    }

    return shader;
}

// generate a checkerboard texture
//------------------------------------------------------------------------------
void checkerBoardTexture(GLuint &textureID, int checkImageWidth, int checkImageHeight, int alpha_style)
{
    GLint max_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    printf("Max texture size: %d\n", max_size);


    GLubyte *checkImage = new GLubyte[checkImageWidth * checkImageHeight * 4];
    printf("doing checkerboard \n");
    fflush(stdout);


    int c;
    for (int i = 0; i < checkImageHeight; i++)
    {
        for (int j = 0; j < checkImageWidth; j++)
        {
            GLubyte *pixel = &checkImage[i + checkImageHeight * j];
            c = ((((i & 0x8) == 0) ^ (((j & 0x8)) == 0))) * 255;
            *pixel++ = (GLubyte) c;
            *pixel++ = (GLubyte) c;
            *pixel++ = (GLubyte) c;
            *pixel = (GLubyte) 255;
        }
    }

    printf("Generated checkerboard ok \n");
    fflush(stdout);

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);


    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, checkImageWidth, checkImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 checkImage);
    delete checkImage;
}

// register textures
//------------------------------------------------------------------------------
void registerTexture(GLuint &textureID, int image_width, int image_height, const char *pimage_file, int alpha_style,
                     bool digits)
{
    ifstream infile;

    printf("registerTexture(): %s\n", pimage_file);

    char *dest_dir = std::getenv("DESTDIR");
    if (dest_dir == nullptr)
    {
        cerr
                << "ERROR : ****** Set environment variable DESTDIR to ensure correct paths of textures/params.txt get picked up."
                << "Example: export DESTDIR=$PWD if local testing or  export DESTDIR=<path to synbench> ******\n";
        exit_synbench(1);
    }

    string full_path = string(dest_dir) + "/" + pimage_file;
    infile.open(full_path.c_str(), std::ifstream::binary);
    if (!infile)
    {
        cerr << "ERROR: Couldn't find " << full_path << endl;
        return;
    }

    GLubyte *raw_image_data = new GLubyte[image_width * image_height * 4];
    GLubyte *pimage_data = new GLubyte[image_width * image_height * 4];
    GLubyte *pimage_data_copy = pimage_data;
    memset(raw_image_data, 255, image_width * image_height * 4);

    if (4 != alpha_style)
    {
        infile.read((char *) pimage_data, image_width * image_height * 4);
    } else
    {
        //texture file only has RGB888, so only read 3 bytes per pixel
        infile.read((char *) pimage_data, image_width * image_height * 3);
    }

    if (!infile)
    {
        cerr << "ERROR: Couldn't read " << image_width * image_height * 4
             << " characters from " << full_path << endl;
    }
    infile.close();

    char *pIndex = (char *) raw_image_data;

    if (0 == alpha_style)
    {
        //printf("alpha=off\n");
        for (int x = 0; x < (image_width * image_height); x++)
        {
            HEADER_PIXEL(pimage_data, pIndex);

            pIndex += 4;
        }
    } else
    {
        if (1 == alpha_style)
        {
            for (int x = 0; x < (image_width * image_height); x++)
            {
                HEADER_PIXEL(pimage_data, pIndex);

                if ((pIndex[0] >= 5 && pIndex[1] >= 5 && pIndex[2] >= 5))
                {
                    pIndex[3] = 0x0;
                } else
                {
                    pIndex[3] = pIndex[0];
                }

                pIndex += 4;
            }
        }
        if (2 == alpha_style)
        {
            for (int x = 0; x < (image_width * image_height); x++)
            {
                HEADER_PIXEL(pimage_data, pIndex);

                if ((pIndex[0] == -1 && pIndex[1] == -1 && pIndex[2] == -1))
                {
                    pIndex[3] = 0x0;
                } else
                {
                    pIndex[3] = pIndex[0];
                }

                pIndex += 4;
            }
        }
        if (3 == alpha_style)
        {
            for (int x = 0; x < (image_width * image_height); x++)
            {
                pIndex[0] = pimage_data[0];
                pIndex[1] = pimage_data[1];
                pIndex[2] = pimage_data[2];
                pIndex[3] = pimage_data[3];
                pimage_data += 4;
                pIndex += 4;
            }
        }
        if (4 == alpha_style)
        {
            for (int x = 0; x < (image_width * image_height); x++)
            {
                pIndex[0] = pimage_data[0];
                pIndex[1] = pimage_data[1];
                pIndex[2] = pimage_data[2];
                pIndex[3] = 0xFF;
                pimage_data += 3;
                pIndex += 4;
            }
        }
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, raw_image_data);

    delete[] raw_image_data;
    delete[] pimage_data_copy;
}
