#pragma once

#ifdef _WIN32
#define APIENTRY __stdcall
#endif

//#define GLAD_DEBUG

// GLAD
#include <glad/glad.h>

namespace kepler {
namespace gl {


using BufferHandle = GLuint;
using ProgramHandle = GLuint;
using TextureHandle = GLuint;
using SamplerHandle = GLuint;

void replace_glad_callbacks();

} // namespace gl
} // namespace kepler
