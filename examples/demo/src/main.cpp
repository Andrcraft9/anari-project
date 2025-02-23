#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

#include <GLFW/glfw3.h>

#define ANARI_EXTENSION_UTILITY_IMPL
#include <anari/anari_cpp.hpp>
#include <anari/anari_cpp/ext/std.h>

constexpr int kWidth = 640;
constexpr int kHeight = 480;

using uvec2 = std::array<unsigned int, 2>;
using uvec3 = std::array<unsigned int, 3>;
using vec3 = std::array<float, 3>;
using vec4 = std::array<float, 4>;
using box3 = std::array<vec3, 2>;

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GL_TRUE);
}

class DisplaySystem {
public:
  DisplaySystem() { glfwInit(); }

  ~DisplaySystem() {
    if (window_ != nullptr) {
      glfwDestroyWindow(window_);
    }
    glfwTerminate();
  }

  void CreateWindow() {
    std::printf("Info: Creating a window\n");
    window_ = glfwCreateWindow(kWidth, kHeight, "glfw3-window", NULL, NULL);
    if (window_ == nullptr) {
      const char *error_str{};
      glfwGetError(&error_str);
      std::printf("Error: Cannot create a window, err=%s\n", error_str);
    }
    glfwMakeContextCurrent(window_);
    glfwSetKeyCallback(window_, key_callback);
  }

  GLFWwindow *Window() { return window_; }

private:
  GLFWwindow *window_{nullptr};
};

static void statusFunc(const void *userData, ANARIDevice device,
                       ANARIObject source, ANARIDataType sourceType,
                       ANARIStatusSeverity severity, ANARIStatusCode code,
                       const char *message) {
  (void)userData;
  (void)device;
  (void)source;
  (void)sourceType;
  (void)code;
  if (severity == ANARI_SEVERITY_FATAL_ERROR) {
    std::fprintf(stderr, "[FATAL] %s\n", message);
  } else if (severity == ANARI_SEVERITY_ERROR) {
    std::fprintf(stderr, "[ERROR] %s\n", message);
  } else if (severity == ANARI_SEVERITY_WARNING) {
    std::fprintf(stderr, "[WARN ] %s\n", message);
  } else if (severity == ANARI_SEVERITY_PERFORMANCE_WARNING) {
    std::fprintf(stderr, "[PERF ] %s\n", message);
  } else if (severity == ANARI_SEVERITY_INFO) {
    std::fprintf(stderr, "[INFO ] %s\n", message);
  } else if (severity == ANARI_SEVERITY_DEBUG) {
    std::fprintf(stderr, "[DEBUG] %s\n", message);
  }
}

static void onFrameCompletion(const void *, anari::Device d, anari::Frame f) {
  std::printf("anari::Device(%p) finished rendering anari::Frame(%p)!\n", d, f);
}

template <typename T>
static T getPixelValue(uvec2 coord, int width, const T *buf) {
  return buf[coord[1] * width + coord[0]];
}

int main(int argc, const char **argv) {
  (void)argc;
  (void)argv;

  std::printf("Starting the app\n");

  DisplaySystem ds{};
  ds.CreateWindow();

  // image size
  uvec2 imgSize = {kWidth, kHeight};

  // camera
  vec3 cam_pos = {0.f, 0.f, 0.f};
  vec3 cam_up = {0.f, 1.f, 0.f};
  vec3 cam_view = {0.1f, 0.f, 1.f};

  // triangle mesh array
  vec3 vertex[] = {{-1.0f, -1.0f, 3.0f},
                   {-1.0f, 1.0f, 3.0f},
                   {1.0f, -1.0f, 3.0f},
                   {1.0f, 1.0f, 3.0f}};
  vec4 color[] = {{0.9f, 0.5f, 0.5f, 1.0f},
                  {0.8f, 0.8f, 0.8f, 1.0f},
                  {0.8f, 0.8f, 0.8f, 1.0f},
                  {0.5f, 0.9f, 0.5f, 1.0f}};
  uvec3 index[] = {{0, 1, 2}, {1, 2, 3}};

  std::printf("initialize ANARI...");
  anari::Library lib = anari::loadLibrary("helide", statusFunc);

  anari::Extensions extensions =
      anari::extension::getDeviceExtensionStruct(lib, "default");

  if (!extensions.ANARI_KHR_GEOMETRY_TRIANGLE)
    std::printf(
        "WARNING: device doesn't support ANARI_KHR_GEOMETRY_TRIANGLE\n");
  if (!extensions.ANARI_KHR_CAMERA_PERSPECTIVE)
    std::printf(
        "WARNING: device doesn't support ANARI_KHR_CAMERA_PERSPECTIVE\n");
  if (!extensions.ANARI_KHR_MATERIAL_MATTE)
    std::printf("WARNING: device doesn't support ANARI_KHR_MATERIAL_MATTE\n");
  if (!extensions.ANARI_KHR_FRAME_COMPLETION_CALLBACK) {
    std::printf(
        "INFO: device doesn't support ANARI_KHR_FRAME_COMPLETION_CALLBACK\n");
  }

  anari::Device d = anari::newDevice(lib, "default");

  std::printf("done!\n");
  std::printf("setting up camera...");

  // create and setup camera
  auto camera = anari::newObject<anari::Camera>(d, "perspective");
  anari::setParameter(d, camera, "aspect",
                      (float)imgSize[0] / (float)imgSize[1]);
  anari::setParameter(d, camera, "position", cam_pos);
  anari::setParameter(d, camera, "direction", cam_view);
  anari::setParameter(d, camera, "up", cam_up);
  anari::commitParameters(d, camera);

  std::printf("done!\n");
  std::printf("setting up scene...");

  // The world to be populated with renderable objects
  auto world = anari::newObject<anari::World>(d);

  // create and setup surface and mesh
  auto mesh = anari::newObject<anari::Geometry>(d, "triangle");
  anari::setParameterArray1D(d, mesh, "vertex.position", vertex, 4);
  anari::setParameterArray1D(d, mesh, "vertex.color", color, 4);
  anari::setParameterArray1D(d, mesh, "primitive.index", index, 2);
  anari::commitParameters(d, mesh);

  auto mat = anari::newObject<anari::Material>(d, "matte");
  anari::setParameter(d, mat, "color", "color");
  anari::commitParameters(d, mat);

  // put the mesh into a surface
  auto surface = anari::newObject<anari::Surface>(d);
  anari::setAndReleaseParameter(d, surface, "geometry", mesh);
  anari::setAndReleaseParameter(d, surface, "material", mat);
  anari::setParameter(d, surface, "id", 2u);
  anari::commitParameters(d, surface);

  // put the surface directly onto the world
  anari::setParameterArray1D(d, world, "surface", &surface, 1);
  anari::setParameter(d, world, "id", 3u);
  anari::release(d, surface);

  anari::commitParameters(d, world);

  std::printf("done!\n");
  std::printf("setting up renderer...");

  // create renderer
  auto renderer = anari::newObject<anari::Renderer>(d, "default");
  // objects can be named for easier identification in debug output etc.
  anari::setParameter(d, renderer, "name", "MainRenderer");
  anari::setParameter(d, renderer, "ambientRadiance", 1.f);
  anari::commitParameters(d, renderer);

  // Frame
  auto frame = anari::newObject<anari::Frame>(d);
  anari::setAndReleaseParameter(d, frame, "renderer", renderer);
  anari::setAndReleaseParameter(d, frame, "camera", camera);
  anari::setAndReleaseParameter(d, frame, "world", world);
  anari::setParameter(d, frame, "frameCompletionCallback",
                      (anari::FrameCompletionCallback)onFrameCompletion);
  anari::commitParameters(d, frame);

  // Render loop
  const auto start_time = std::chrono::steady_clock::now();
  while (!glfwWindowShouldClose(ds.Window())) {
    const float time{std::chrono::duration_cast<std::chrono::duration<float>>(
                         std::chrono::steady_clock::now() - start_time)
                         .count()};

    int width, height;
    glfwGetFramebufferSize(ds.Window(), &width, &height);
    imgSize[0] = width;
    imgSize[1] = height;

    // Update color
    color[0][0] = std::sin(time);
    color[3][0] = std::cos(time);
    anari::setParameterArray1D(d, mesh, "vertex.color", color, 4);
    anari::commitParameters(d, mesh);

    // Update camera
    vec3 new_cam_pos = cam_pos;
    new_cam_pos[1] = std::sin(time);
    anari::setParameter(d, camera, "position", new_cam_pos);
    anari::commitParameters(d, camera);

    // Create and setup frame
    anari::setParameter(d, frame, "size", imgSize);
    anari::setParameter(d, frame, "channel.color", ANARI_UFIXED8_RGBA_SRGB);
    anari::setParameter(d, frame, "channel.primitiveId", ANARI_UINT32);
    anari::setParameter(d, frame, "channel.objectId", ANARI_UINT32);
    anari::setParameter(d, frame, "channel.instanceId", ANARI_UINT32);
    anari::commitParameters(d, frame);

    // Render frame
    anari::render(d, frame);
    anari::wait(d, frame);

    auto fb = anari::map<uint32_t>(d, frame, "channel.color");

    glViewport(0, 0, width, height);
    glClearColor(0.3F, 0.3F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, fb.data);
    glfwSwapBuffers(ds.Window());

    anari::unmap(d, frame, "channel.color");

    // Check center pixel id buffers
    auto fbPrimId = anari::map<uint32_t>(d, frame, "channel.primitiveId");
    auto fbObjId = anari::map<uint32_t>(d, frame, "channel.objectId");
    auto fbInstId = anari::map<uint32_t>(d, frame, "channel.instanceId");

    uvec2 queryPixel = {imgSize[0] / 2, imgSize[1] / 2};

    std::printf("checking id buffers @ [%u, %u]:\n", queryPixel[0],
                queryPixel[1]);
    if (fbPrimId.pixelType == ANARI_UINT32) {
      std::printf("    primId: %u\n",
                  getPixelValue(queryPixel, imgSize[0], fbPrimId.data));
    }
    if (fbObjId.pixelType == ANARI_UINT32) {
      std::printf("     objId: %u\n",
                  getPixelValue(queryPixel, imgSize[0], fbObjId.data));
    }
    if (fbPrimId.pixelType == ANARI_UINT32) {
      std::printf("    instId: %u\n",
                  getPixelValue(queryPixel, imgSize[0], fbInstId.data));
    }

    glfwPollEvents();
  }

  // Final cleanups
  std::printf("\ncleaning up objects...");
  anari::release(d, frame);
  anari::release(d, d);
  anari::unloadLibrary(lib);

  std::printf("done!\n");
  return 0;
}