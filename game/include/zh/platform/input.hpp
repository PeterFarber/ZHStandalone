#pragma once

namespace zh::platform {

void init_input();
void update_input_frame();
void end_input_frame();

[[nodiscard]] bool key_down(int key);
[[nodiscard]] bool key_pressed(int key);
[[nodiscard]] int char_pressed();
[[nodiscard]] bool mouse_down(int button);
[[nodiscard]] bool mouse_pressed(int button);
[[nodiscard]] float mouse_x();
[[nodiscard]] float mouse_y();
[[nodiscard]] float mouse_wheel();

}  // namespace zh::platform
