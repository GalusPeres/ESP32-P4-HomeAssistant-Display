#include "src/types/radar/web_styles.h"

void append_radar_styles(String& html) {
  html += R"html(
  <style>
    .tile.radar {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
    }
    .type-fields .field-hint {
      display: block;
      margin-top: -6px;
      margin-bottom: 10px;
      font-size: 12px;
      color: rgba(20, 40, 80, 0.65);
    }
    .type-fields .field-disabled {
      opacity: 0.55;
    }
  </style>
)html";
}
