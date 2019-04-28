#include "pch.h"
#include "util.h"
#include "dds.h"

unsigned int BMP_to_GL(const char * file) {
    unsigned int texture_id = 0;

    SDL_Surface* texture = SDL_LoadBMP(file);
    if(texture == NULL) trace("failed to load BMP, wrong path?");

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    int mode = GL_RGB;
    if(texture->format->BytesPerPixel == 4)  mode = GL_RGBA;
    
    glTexImage2D(GL_TEXTURE_2D, 0, mode, texture->w, texture->h, 
                        0, mode, GL_UNSIGNED_BYTE, texture->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    return texture_id;
}