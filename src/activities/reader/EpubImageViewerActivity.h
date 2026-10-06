#pragma once

#include <string>

#include "MappedInputManager.h"
#include "activities/Activity.h"

class EpubImageViewerActivity final : public Activity {
 public:
  EpubImageViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string imagePath);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void drawImage();
  void clampPan();
  void resetPinch();

  std::string imagePath;

  int imageWidth = 0;
  int imageHeight = 0;

  float zoomScale = 1.0f;
  float panX = 0.0f;
  float panY = 0.0f;

  bool imageDimensionsLoaded = false;

#if CROSSINK_APP_CAP_TOUCH
  bool dragging = false;
  bool suppressSingleTouchUntilRelease = false;
  int dragLastX = 0;
  int dragLastY = 0;

  bool pinchActive = false;
  float pinchStartDistance = 0.0f;
  float pinchStartZoom = 1.0f;
  float pinchStartCenterX = 0.0f;
  float pinchStartCenterY = 0.0f;
#endif
};