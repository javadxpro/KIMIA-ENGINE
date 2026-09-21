#include <kimia/Image.h>

#include <kimia/VendoredWarnings.h>

// Both vendored implementations live in their own translation units
// (ThirdParty/stb/stb_image_impl.c and stb_image_write_impl.c): only the
// declarations are read here, so this file stays under the engine's full
// warning gate.
#include <stb_image.h>
#include <stb_image_write.h>

#include <stdexcept>

namespace kimia {

namespace {

struct WriteContext {
  std::vector<u8>* buffer;
};

void writeCallback(void* context, void* data, int size) {
  auto* ctx = static_cast<WriteContext*>(context);
  const u8* bytes = static_cast<const u8*>(data);
  ctx->buffer->insert(ctx->buffer->end(), bytes, bytes + size);
}

}  // namespace

std::optional<Image> Image::load(const std::string& path, std::string& error) {
  int width = 0;
  int height = 0;
  int channels = 0;
  u8* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
  if (data == nullptr) {
    const char* reason = stbi_failure_reason();
    error = "cannot load image '" + path + "'";
    if (reason != nullptr && reason[0] != '\0') error += ": " + std::string(reason);
    return std::nullopt;
  }
  Image image;
  image.width = width;
  image.height = height;
  image.channels = channels;
  const usize byteCount = static_cast<usize>(width) * static_cast<usize>(height) * static_cast<usize>(channels);
  image.pixels.assign(data, data + byteCount);
  stbi_image_free(data);
  return image;
}

Image Image::loadOrThrow(const std::string& path) {
  std::string error;
  auto image = load(path, error);
  if (!image.has_value()) throw std::runtime_error(error);
  return *image;
}

std::vector<u8> Image::encodePNG() const {
  std::vector<u8> result;
  if (isEmpty()) return result;
  WriteContext context{&result};
  const int stride = width * channels;
  if (stbi_write_png_to_func(writeCallback, &context, width, height, channels, pixels.data(), stride) == 0) {
    result.clear();
  }
  return result;
}

std::vector<u8> Image::encodeJPG(i32 quality) const {
  std::vector<u8> result;
  if (isEmpty() || channels < 3) return result;
  quality = std::max(1, std::min(quality, 100));
  WriteContext context{&result};
  if (stbi_write_jpg_to_func(writeCallback, &context, width, height, channels, pixels.data(), quality) == 0) {
    result.clear();
  }
  return result;
}

bool Image::writePNG(const std::string& path) const {
  if (isEmpty()) return false;
  const int stride = width * channels;
  return stbi_write_png(path.c_str(), width, height, channels, pixels.data(), stride) != 0;
}

bool Image::writeJPG(const std::string& path, i32 quality) const {
  if (isEmpty()) return false;
  return stbi_write_jpg(path.c_str(), width, height, channels, pixels.data(), quality) != 0;
}

}  // namespace kimia
