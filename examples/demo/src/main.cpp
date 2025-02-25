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

class RenderSystem {
public:
  RenderSystem() = default;
  ~RenderSystem() {
    if (device_ && frame_) {
      anari::release(device_, frame_);
    }
    if (device_) {
      anari::release(device_, device_);
    }
    if (library_) {
      anari::unloadLibrary(library_);
    }
  }

  void Init() {
    std::printf("Initializing ANARI\n");
    std::printf("Loading a library\n");
    library_ = anari::loadLibrary("helide", statusFunc);

    std::printf("Creating a device\n");
    anari::Extensions extensions =
        anari::extension::getDeviceExtensionStruct(library_, "default");
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
    device_ = anari::newDevice(library_, "default");

    std::printf("Creating a renderer\n");
    renderer_ = anari::newObject<anari::Renderer>(device_, "default");
    anari::setParameter(device_, renderer_, "name", "MainRenderer");
    anari::setParameter(device_, renderer_, "ambientRadiance", 1.0F);
    anari::commitParameters(device_, renderer_);
  }

  void CreateScene() {
    std::printf("Creating a scene\n");

    // image size
    uvec2 imgSize = {kWidth, kHeight};

    // camera
    camera_position_ = {0.0F, 0.0F, 0.0F};
    camera_up_ = {0.0F, 1.0F, 0.0F};
    camera_direction_ = {0.1F, 0.0F, 1.0F};

    // create and setup camera
    camera_ = anari::newObject<anari::Camera>(device_, "perspective");
    anari::setParameter(device_, camera_, "aspect",
                        (float)imgSize[0] / (float)imgSize[1]);
    anari::setParameter(device_, camera_, "position", camera_position_);
    anari::setParameter(device_, camera_, "up", camera_up_);
    anari::setParameter(device_, camera_, "direction", camera_direction_);
    anari::commitParameters(device_, camera_);

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

    // The world to be populated with renderable objects
    world_ = anari::newObject<anari::World>(device_);

    // create and setup surface and mesh
    auto mesh = anari::newObject<anari::Geometry>(device_, "triangle");
    anari::setParameterArray1D(device_, mesh, "vertex.position", vertex, 4);
    anari::setParameterArray1D(device_, mesh, "vertex.color", color, 4);
    anari::setParameterArray1D(device_, mesh, "primitive.index", index, 2);
    anari::commitParameters(device_, mesh);

    auto mat = anari::newObject<anari::Material>(device_, "matte");
    anari::setParameter(device_, mat, "color", "color");
    anari::commitParameters(device_, mat);

    // put the mesh into a surface
    auto surface = anari::newObject<anari::Surface>(device_);
    anari::setAndReleaseParameter(device_, surface, "geometry", mesh);
    anari::setAndReleaseParameter(device_, surface, "material", mat);
    anari::setParameter(device_, surface, "id", 2U);
    anari::commitParameters(device_, surface);

    // put the surface directly onto the world
    anari::setParameterArray1D(device_, world_, "surface", &surface, 1);
    anari::setParameter(device_, world_, "id", 3U);
    anari::release(device_, surface);

    anari::commitParameters(device_, world_);
  }

  void SetupFrame() {
    std::printf("Setuping frame\n");

    frame_ = anari::newObject<anari::Frame>(device_);
    anari::setAndReleaseParameter(device_, frame_, "renderer", renderer_);
    anari::setAndReleaseParameter(device_, frame_, "camera", camera_);
    anari::setAndReleaseParameter(device_, frame_, "world", world_);
    anari::setParameter(device_, frame_, "frameCompletionCallback",
                        (anari::FrameCompletionCallback)onFrameCompletion);
    anari::setParameter(device_, frame_, "channel.color",
                        ANARI_UFIXED8_RGBA_SRGB);
    anari::setParameter(device_, frame_, "channel.primitiveId", ANARI_UINT32);
    anari::setParameter(device_, frame_, "channel.objectId", ANARI_UINT32);
    anari::setParameter(device_, frame_, "channel.instanceId", ANARI_UINT32);
    anari::commitParameters(device_, frame_);
  }

  vec3 GetCameraPosition() { return camera_position_; }
  vec3 GetCameraUp() { return camera_up_; }
  vec3 GetCameraDirection() { return camera_direction_; }

  void UpdateCamera(vec3 pos, vec3 up, vec3 dir) {
    camera_position_ = pos;
    camera_up_ = up;
    camera_direction_ = dir;
    anari::setParameter(device_, camera_, "position", camera_position_);
    anari::setParameter(device_, camera_, "up", camera_up_);
    anari::setParameter(device_, camera_, "direction", camera_direction_);
    anari::commitParameters(device_, camera_);
  }

  uvec2 GetFrameSize() { return frame_size_; }

  void UpdateFrameSize(uvec2 size) {
    frame_size_ = size;
    anari::setParameter(device_, frame_, "size", frame_size_);
    anari::commitParameters(device_, frame_);
  }

  void RenderFrame() {
    anari::render(device_, frame_);
    anari::wait(device_, frame_);
  }

  anari::MappedFrameData<uint32_t> MapFrame() {
    return anari::map<uint32_t>(device_, frame_, "channel.color");
  }

  void UnmapFrame() { anari::unmap(device_, frame_, "channel.color"); }

private:
  anari::Library library_{};
  anari::Device device_{};
  anari::Renderer renderer_{};

  anari::World world_{};

  vec3 camera_position_{};
  vec3 camera_direction_{};
  vec3 camera_up_{};
  anari::Camera camera_{};

  uvec2 frame_size_{kWidth, kHeight};
  anari::Frame frame_{};
};

class WindowWrapper {
public:
  WindowWrapper(GLFWwindow *window) : window_{window} {}

  ~WindowWrapper() {
    if (window_ != nullptr) {
      glfwDestroyWindow(window_);
    }
  }

  GLFWwindow *Window() { return window_; }

  void HandleKey(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
      switch (key) {
      case GLFW_KEY_ESCAPE: {
        std::printf("Key: Window should close\n");
        glfwSetWindowShouldClose(window_, GL_TRUE);
        break;
      }
      case GLFW_KEY_A: {
        std::printf("Key: Left\n");
        break;
      }
      case GLFW_KEY_D: {
        std::printf("Key: Right\n");
        break;
      }
      default:
        std::printf("Key: Unknown input\n");
        break;
      }
    }
  }

private:
  GLFWwindow *window_;
};

class DisplaySystem {
public:
  DisplaySystem() { glfwInit(); }

  ~DisplaySystem() { glfwTerminate(); }

  void CreateWindow() {
    std::printf("Info: Creating a window\n");
    auto window = glfwCreateWindow(kWidth, kHeight, "glfw3-window", NULL, NULL);
    if (window == nullptr) {
      const char *error_str{};
      glfwGetError(&error_str);
      std::printf("Error: Cannot create a window, err=%s\n", error_str);
    }

    glfwMakeContextCurrent(window);

    window_wrapper_ = std::make_unique<WindowWrapper>(window);
    glfwSetWindowUserPointer(window, window_wrapper_.get());
    auto key_callback = [](GLFWwindow *w, int key, int scancode, int action,
                           int mods) {
      static_cast<WindowWrapper *>(glfwGetWindowUserPointer(w))
          ->HandleKey(key, scancode, action, mods);
    };
    glfwSetKeyCallback(window, key_callback);
  }

  GLFWwindow *Window() { return window_wrapper_->Window(); }

private:
  std::unique_ptr<WindowWrapper> window_wrapper_{nullptr};
};

int main(int argc, const char **argv) {
  (void)argc;
  (void)argv;

  std::printf("Starting the app\n");

  DisplaySystem ds{};
  ds.CreateWindow();

  RenderSystem rs{};
  rs.Init();
  rs.CreateScene();
  rs.SetupFrame();

  // Render loop
  const auto start_time = std::chrono::steady_clock::now();
  while (!glfwWindowShouldClose(ds.Window())) {
    const float time{std::chrono::duration_cast<std::chrono::duration<float>>(
                         std::chrono::steady_clock::now() - start_time)
                         .count()};

    // Handle window resizing
    int width, height;
    glfwGetFramebufferSize(ds.Window(), &width, &height);
    uvec2 frame_size{static_cast<uint32_t>(width),
                     static_cast<uint32_t>(height)};
    rs.UpdateFrameSize(frame_size);

    // Update camera
    auto camera_pos = rs.GetCameraPosition();
    auto camera_up = rs.GetCameraUp();
    auto camera_dir = rs.GetCameraDirection();
    camera_pos[1] = std::sin(time);
    rs.UpdateCamera(camera_pos, camera_up, camera_dir);

    // Render frame
    rs.RenderFrame();

    // Map rendered frame
    auto fb = rs.MapFrame();
    glViewport(0, 0, width, height);
    glClearColor(0.3F, 0.3F, 0.3F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, fb.data);
    glfwSwapBuffers(ds.Window());
    rs.UnmapFrame();

    // Check center pixel id buffers
    // auto fbPrimId = anari::map<uint32_t>(d, frame, "channel.primitiveId");
    // auto fbObjId = anari::map<uint32_t>(d, frame, "channel.objectId");
    // auto fbInstId = anari::map<uint32_t>(d, frame, "channel.instanceId");
    // uvec2 queryPixel = {imgSize[0] / 2, imgSize[1] / 2};
    // std::printf("checking id buffers @ [%u, %u]:\n", queryPixel[0],
    //            queryPixel[1]);
    // if (fbPrimId.pixelType == ANARI_UINT32) {
    //  std::printf("    primId: %u\n",
    //              getPixelValue(queryPixel, imgSize[0], fbPrimId.data));
    //}
    // if (fbObjId.pixelType == ANARI_UINT32) {
    //  std::printf("     objId: %u\n",
    //              getPixelValue(queryPixel, imgSize[0], fbObjId.data));
    //}
    // if (fbPrimId.pixelType == ANARI_UINT32) {
    //  std::printf("    instId: %u\n",
    //              getPixelValue(queryPixel, imgSize[0], fbInstId.data));
    //}

    glfwPollEvents();
  }

  return 0;
}