// The first controller as a SNES pad. Buttons go by position, as on the SNES pad: Xbox A (bottom) = SNES B,
// Xbox B (right) = SNES A, Xbox X (left) = SNES Y, Xbox Y (top) = SNES X; LB/LT = L, RB/RT = R; Back = Select.
// The left stick works as the D-pad. The right stick click is the platform's menu button.
#include <string.h>
#include "s360_internal.h"
#include "xsdk/kernel.h"

enum { kStickDeadzone = 12000, kTriggerThreshold = 64 };

void s360_input_poll(S360Pad *pad) {
  static uint16_t last;
  INPUT_STATE st;
  memset(&st, 0, sizeof st);
  if (XamInputGetState(0, 0, &st) != ERROR_SUCCESS) memset(&st, 0, sizeof st);
  const INPUT_GAMEPAD *g = &st.gamepad;
  uint16_t b = g->buttons;
  if (g->thumb_ly > kStickDeadzone) b |= INPUT_GAMEPAD_DPAD_UP;
  if (g->thumb_ly < -kStickDeadzone) b |= INPUT_GAMEPAD_DPAD_DOWN;
  if (g->thumb_lx < -kStickDeadzone) b |= INPUT_GAMEPAD_DPAD_LEFT;
  if (g->thumb_lx > kStickDeadzone) b |= INPUT_GAMEPAD_DPAD_RIGHT;
  if (g->left_trigger > kTriggerThreshold) b |= INPUT_GAMEPAD_LEFT_SHOULDER;
  if (g->right_trigger > kTriggerThreshold) b |= INPUT_GAMEPAD_RIGHT_SHOULDER;
  pad->held = b;
  pad->pressed = b & ~last;
  last = b;
  uint16_t s = 0;
  if (b & INPUT_GAMEPAD_A) s |= kS360_B;
  if (b & INPUT_GAMEPAD_B) s |= kS360_A;
  if (b & INPUT_GAMEPAD_X) s |= kS360_Y;
  if (b & INPUT_GAMEPAD_Y) s |= kS360_X;
  if (b & INPUT_GAMEPAD_LEFT_SHOULDER) s |= kS360_L;
  if (b & INPUT_GAMEPAD_RIGHT_SHOULDER) s |= kS360_R;
  if (b & INPUT_GAMEPAD_START) s |= kS360_Start;
  if (b & INPUT_GAMEPAD_BACK) s |= kS360_Select;
  if (b & INPUT_GAMEPAD_DPAD_UP) s |= kS360_Up;
  if (b & INPUT_GAMEPAD_DPAD_DOWN) s |= kS360_Down;
  if (b & INPUT_GAMEPAD_DPAD_LEFT) s |= kS360_Left;
  if (b & INPUT_GAMEPAD_DPAD_RIGHT) s |= kS360_Right;
  pad->snes = s;
}
