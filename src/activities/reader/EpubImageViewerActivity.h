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

  std::string imagePath;
};
