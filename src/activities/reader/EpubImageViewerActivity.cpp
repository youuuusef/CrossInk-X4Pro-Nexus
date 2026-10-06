#include "EpubImageViewerActivity.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <GfxRenderer.h>
#include <HalDisplay.h>

#include "Epub/converters/ImageDecoderFactory.h"
#include "Epub/converters/ImageToFramebufferDecoder.h"

EpubImageViewerActivity::EpubImageViewerActivity(GfxRenderer& renderer,
                                                 MappedInputManager& mappedInput,
                                                 std::string imagePath)
    : Activity("EpubImageViewer", renderer, mappedInput), imagePath(std::move(imagePath)) {}

void EpubImageViewerActivity::onEnter() {
  Activity::onEnter();

  ImageToFramebufferDecoder* decoder = ImageDecoderFactory::getDecoder(imagePath);
  if (decoder) {
    ImageDimensions dimensions;
    if (decoder->getDimensions(imagePath, dimensions) && dimensions.width > 0 && dimensions.height > 0) {
      imageWidth = dimensions.width;
      imageHeight = dimensions.height;
      imageDimensionsLoaded = true;
    }
  }

  requestUpdate();
}

void EpubImageViewerActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

#if CROSSINK_APP_CAP_TOUCH
  if (!mappedInput.hasTouch()) return;

  int touchX = 0;
  int touchY = 0;

  // A two-finger contact takes priority over single-finger dragging.
  if (mappedInput.supportsMultiTouch()) {
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;

    if (mappedInput.getTwoFingerTouch(x1, y1, x2, y2)) {
      const float centerX = (static_cast<float>(x1) + static_cast<float>(x2)) * 0.5f;
      const float centerY = (static_cast<float>(y1) + static_cast<float>(y2)) * 0.5f;
      const float dx = static_cast<float>(x2 - x1);
      const float dy = static_cast<float>(y2 - y1);
      const float distance = std::sqrt(dx * dx + dy * dy);

      if (!pinchActive) {
        pinchActive = true;
        dragging = false;
        suppressSingleTouchUntilRelease = true;
        pinchStartDistance = std::max(distance, 1.0f);
        pinchStartZoom = zoomScale;
        pinchStartCenterX = centerX;
        pinchStartCenterY = centerY;
        return;
      }

      const float newZoom =
          std::clamp(pinchStartZoom * (distance / pinchStartDistance), 1.0f, 8.0f);

      if (std::abs(newZoom - zoomScale) > 0.005f) {
        // Keep the image point underneath the original pinch centre fixed.
        const float oldZoom = zoomScale;
        const float imagePointX = (pinchStartCenterX - renderer.getScreenWidth() * 0.5f - panX) / oldZoom;
        const float imagePointY = (pinchStartCenterY - renderer.getScreenHeight() * 0.5f - panY) / oldZoom;

        zoomScale = newZoom;
        panX = pinchStartCenterX - renderer.getScreenWidth() * 0.5f - imagePointX * zoomScale;
        panY = pinchStartCenterY - renderer.getScreenHeight() * 0.5f - imagePointY * zoomScale;

        clampPan();
        requestUpdate();
      }

      return;
    }
  }

  if (pinchActive) {
    resetPinch();
  }

  // Once a pinch has occurred, don't interpret the remaining finger as a pan.
  if (suppressSingleTouchUntilRelease) {
    if (mappedInput.wasScreenTouchReleased()) {
      suppressSingleTouchUntilRelease = false;
    }
    return;
  }

  if (mappedInput.wasScreenTouchDown(touchX, touchY)) {
    dragging = zoomScale > 1.001f;
    dragLastX = touchX;
    dragLastY = touchY;
    return;
  }

  if (dragging && mappedInput.isScreenTouchHeld(touchX, touchY)) {
    const int dx = touchX - dragLastX;
    const int dy = touchY - dragLastY;

    if (dx != 0 || dy != 0) {
      panX += static_cast<float>(dx);
      panY += static_cast<float>(dy);
      clampPan();

      dragLastX = touchX;
      dragLastY = touchY;
      requestUpdate();
    }

    return;
  }

  if (dragging && mappedInput.wasScreenTouchReleased()) {
    dragging = false;
    return;
  }

  // A normal tap exits the image viewer.
  if (mappedInput.wasScreenTapped(touchX, touchY)) {
    finish();
    return;
  }
#endif
}

void EpubImageViewerActivity::render(RenderLock&&) {
  drawImage();
}

void EpubImageViewerActivity::clampPan() {
  if (!imageDimensionsLoaded) return;

  const float screenWidth = static_cast<float>(renderer.getScreenWidth());
  const float screenHeight = static_cast<float>(renderer.getScreenHeight());

  const float baseScale =
      std::min(screenWidth / static_cast<float>(imageWidth),
               screenHeight / static_cast<float>(imageHeight));

  const float drawWidth = static_cast<float>(imageWidth) * baseScale * zoomScale;
  const float drawHeight = static_cast<float>(imageHeight) * baseScale * zoomScale;

  const float maxPanX = std::max(0.0f, (drawWidth - screenWidth) * 0.5f);
  const float maxPanY = std::max(0.0f, (drawHeight - screenHeight) * 0.5f);

  panX = std::clamp(panX, -maxPanX, maxPanX);
  panY = std::clamp(panY, -maxPanY, maxPanY);
}

void EpubImageViewerActivity::resetPinch() {
#if CROSSINK_APP_CAP_TOUCH
  pinchActive = false;
  pinchStartDistance = 0.0f;
#endif
}

void EpubImageViewerActivity::drawImage() {
  renderer.clearScreen();

  if (!imageDimensionsLoaded || imageWidth <= 0 || imageHeight <= 0) {
    renderer.displayBuffer();
    return;
  }

  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();

  const float fitScale =
      std::min(static_cast<float>(screenWidth) / static_cast<float>(imageWidth),
               static_cast<float>(screenHeight) / static_cast<float>(imageHeight));

  const float scale = fitScale * zoomScale;

  const int drawWidth = std::max(1, static_cast<int>(static_cast<float>(imageWidth) * scale));
  const int drawHeight = std::max(1, static_cast<int>(static_cast<float>(imageHeight) * scale));

  const int x = static_cast<int>((static_cast<float>(screenWidth - drawWidth) * 0.5f) + panX);
  const int y = static_cast<int>((static_cast<float>(screenHeight - drawHeight) * 0.5f) + panY);

  RenderConfig config;
  config.x = x;
  config.y = y;
  config.maxWidth = drawWidth;
  config.maxHeight = drawHeight;
  config.useGrayscale = true;
  config.useDithering = true;
  config.performanceMode = false;
  config.useExactDimensions = true;

  ImageToFramebufferDecoder* decoder = ImageDecoderFactory::getDecoder(imagePath);
  if (!decoder || !decoder->decodeToFramebuffer(imagePath, renderer, config)) {
    renderer.displayBuffer();
    return;
  }

  renderer.preserveImagePolarity(x, y, drawWidth, drawHeight);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}