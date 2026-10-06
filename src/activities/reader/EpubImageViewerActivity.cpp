#include "EpubImageViewerActivity.h"

#include <algorithm>

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>

#include "Epub/converters/ImageDecoderFactory.h"
#include "Epub/converters/ImageToFramebufferDecoder.h"
#include "MappedInputManager.h"

EpubImageViewerActivity::EpubImageViewerActivity(GfxRenderer& renderer,
                                                 MappedInputManager& mappedInput,
                                                 std::string imagePath)
    : Activity("EpubImageViewer", renderer, mappedInput), imagePath(std::move(imagePath)) {}

void EpubImageViewerActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void EpubImageViewerActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

#if CROSSINK_APP_CAP_TOUCH
  int touchX = 0;
  int touchY = 0;
  if (mappedInput.hasTouch() && mappedInput.wasScreenTapped(touchX, touchY)) {
    finish();
    return;
  }
#endif
}

void EpubImageViewerActivity::render(RenderLock&&) {
  drawImage();
}

void EpubImageViewerActivity::drawImage() {
  ImageDimensions dims;
  if (!ImageDecoderFactory::getDecoder(imagePath) ||
      !ImageDecoderFactory::getDecoder(imagePath)->getDimensions(imagePath, dims) ||
      dims.width <= 0 || dims.height <= 0) {
    renderer.clearScreen();
    renderer.displayBuffer();
    return;
  }

  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();

  const float scaleX = static_cast<float>(screenWidth) / static_cast<float>(dims.width);
  const float scaleY = static_cast<float>(screenHeight) / static_cast<float>(dims.height);
  const float scale = std::min(1.0f, std::min(scaleX, scaleY));

  const int drawWidth = std::max(1, static_cast<int>(static_cast<float>(dims.width) * scale));
  const int drawHeight = std::max(1, static_cast<int>(static_cast<float>(dims.height) * scale));
  const int x = (screenWidth - drawWidth) / 2;
  const int y = (screenHeight - drawHeight) / 2;

  RenderConfig config;
  config.x = x;
  config.y = y;
  config.maxWidth = drawWidth;
  config.maxHeight = drawHeight;
  config.useGrayscale = true;
  config.useDithering = true;
  config.performanceMode = false;
  config.useExactDimensions = true;

  renderer.clearScreen();

  ImageToFramebufferDecoder* decoder = ImageDecoderFactory::getDecoder(imagePath);
  if (!decoder || !decoder->decodeToFramebuffer(imagePath, renderer, config)) {
    renderer.displayBuffer();
    return;
  }

  renderer.preserveImagePolarity(x, y, drawWidth, drawHeight);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
