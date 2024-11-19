#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef USE_GLES
#include <GLES3/gl3.h>
#define GLSL(s) (const char*)"#version 300 es\n" #s
#else
#include <GL/glew.h>
#define GLSL(s) (const char*)"#version 400\n" #s
#endif

// it MUST be placed here; after including glew
#include <GLFW/glfw3.h>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

// clang-format off
const char* vert_shader_src = GLSL(
  layout(location = 0) in vec2 p;
  void main() { gl_Position = vec4(p, 0.0, 1.0); }
);
const char* frag_shader_src = GLSL(
  out mediump vec4 color;
  void main() { color = vec4(0.0, 1.0, 0.0, 1.0); }
);
// clang-format on
namespace shader {
GLuint compile_shader(const char* vertex_src, const char* flagment_src);
}

int main() {
  if (!glfwInit()) {
    return 1;
  }
#ifdef USE_GLES
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return 2;
  }
  glfwMakeContextCurrent(window);

#ifndef USE_GLES
  if (glewInit() != GLEW_OK) {
    return 3;
  }
#endif

  // Vertex Array Object that expresses triangle
  GLuint vertex_array;
  glGenVertexArrays(1, &vertex_array);

  GLuint pos_buffer;
  {
    // clang-format off
    GLfloat vertices[] = {
      0.f, 1.f,
      1.f, -1.f,
      -1.f, -1.f,
    };
    // clang-format on
    glGenBuffers(1, &pos_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, pos_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Texture that stores pixels of the Framebuffer
  GLuint color_buffer;
  {
    glGenTextures(1, &color_buffer);
    glBindTexture(GL_TEXTURE_2D, color_buffer);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
        0
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  // create Framebuffer and bind the texture to it
  GLuint frame_buffer;
  {
    glGenFramebuffers(1, &frame_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_buffer, 0
    );
    auto bufstat = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (bufstat != GL_FRAMEBUFFER_COMPLETE) {
      std::printf("Failed to create FrameBuffer; errno=0x%X\n", bufstat);
      return 4;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  GLuint program_id = shader::compile_shader(vert_shader_src, frag_shader_src);
  if (program_id == 0) {
    return 5;
  }

  // initialize draw target (Framebuffer)
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_buffer);
  glViewport(0, 0, WIDTH, HEIGHT);
  glClearColor(0.5f, 0.5f, 0.5f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(program_id);

  // draw!
  glBindVertexArray(vertex_array);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, pos_buffer);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  // get pixels
  std::vector<GLubyte> pixels(WIDTH * HEIGHT * 4, 0xff);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, frame_buffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

  // remove binding
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glFlush();

  // save as BMP
  constexpr uint32_t imagesize = WIDTH * HEIGHT * 3;
  constexpr uint32_t filesize  = imagesize + /* header size = */ 54;
  // clang-format off
  constexpr uint8_t  header[] = {
    // file header
    0x42, 0x4d, // magic
    (filesize & 0x0000'00ff),
    (filesize & 0x0000'ff00) >> 8,
    (filesize & 0x00ff'0000) >> 16,
    (filesize & 0xff00'0000) >> 24,
    0, 0, 0, 0, // reserved
    0x36, 0, 0, 0, // offset to pixel data
    // info header
    0x28, 0, 0, 0, // header size
    WIDTH&0x00ff, (WIDTH&0xff00) >> 8, 0, 0,
    HEIGHT&0x00ff, (HEIGHT&0xff00) >> 8, 0, 0,
    1, 0, // plane number; must be 1
    24, 0, // bits per pixel
    0, 0, 0, 0, // compression = RGB
    (imagesize & 0x0000'00ff),
    (imagesize & 0x0000'ff00) >> 8,
    (imagesize & 0x00ff'0000) >> 16,
    (imagesize & 0xff00'0000) >> 24,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, // neadless info
  };
  // clang-format on
  FILE* f = fopen("texure.bmp", "w+b");
  fwrite(header, sizeof(header), 1, f);
  for (int y = 0; y < (int)HEIGHT; ++y) {
    for (int x = 0; x < (int)WIDTH; ++x) {
      std::fwrite(&pixels[(WIDTH * y + x) * 4 + 2], 1, 1, f);  // B
      std::fwrite(&pixels[(WIDTH * y + x) * 4 + 1], 1, 1, f);  // G
      std::fwrite(&pixels[(WIDTH * y + x) * 4 + 0], 1, 1, f);  // R
    }
  }
  fclose(f);

  return 0;
}
