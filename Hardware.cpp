/**
 * Copyright 2022 William Edward Fisher.
 */
#include "Hardware.h"

#include <Adafruit_MCP4728.h>
#include <Entropy.h>
#include "Utils.h"
#include "constants.h"

bool Hardware::prepareRenderingOfRandomizedKey(State state, uint8_t key) {
  if (state.randomColorShouldChange) {
    state.config.trellis.pixels.setPixelColor(
      key,
      Entropy.random(MAX_UNSIGNED_8_BIT),
      Entropy.random(MAX_UNSIGNED_8_BIT),
      Entropy.random(MAX_UNSIGNED_8_BIT)
    );
  }
  // else no op
  return 1;
}

/**
 * @brief Set color values for a NeoTrellis key as either on or off. Do not yet call 
 * trellis.pixels.show().
 * 
 * @param state Global state object.
 * @param step Which of the 16 steps/keys is targeted for changing. 
 */
bool Hardware::prepareRenderingOfStepGate(State state, uint8_t step) {
  if (state.randomSteps[state.currentBank][step][state.currentChannel]) {
    return Hardware::prepareRenderingOfRandomizedKey(state, step);
  }
  state.config.trellis.pixels.setPixelColor(
    step, 
    state.activeSteps[state.currentBank][step][state.currentChannel]
      ? YELLOW
      : PURPLE
  );
  return 1;
}

/**
 * @brief Set color values for a NeoTrellis key, but do not yet call trellis.pixels.show().
 * 
 * @param state Global state object.
 * @param step Which of the 16 steps/keys is targeted for changing.
 */
bool Hardware::prepareRenderingOfStepVoltage(State state, uint8_t step, uint32_t color) {
  seesaw_NeoPixel pixels = state.config.trellis.pixels;
  if (
    state.selectedKeyForCopying >= 0 &&
    state.flash == 0 &&
    (step == state.selectedKeyForCopying || state.pasteTargetKeys[step])
  ) {
    pixels.setPixelColor(step, 0);
  } 
  else if (state.lockedVoltages[state.currentBank][step][state.currentChannel]) {
    pixels.setPixelColor(step, ORANGE);
  }
  else if (!state.activeSteps[state.currentBank][step][state.currentChannel]) {
    pixels.setPixelColor(step, PURPLE);
  }
  else {
    int16_t voltage = state.voltages[state.currentBank][step][state.currentChannel];
    uint8_t colorValue = static_cast<int>(
      COLOR_VALUE_MAX * voltage * PERCENTAGE_MULTIPLIER_10_BIT
    );
    if (color == YELLOW) {
      pixels.setPixelColor(step, colorValue, colorValue, 0);
    }
    else { // WHITE
      pixels.setPixelColor(step, colorValue, colorValue, colorValue);
    }
  }
  return 1;
}

bool Hardware::reflectState(State state) {
  // voltage output
  bool result = Hardware::setOutputsAll(state);
  if (!result) {
    Serial.println("could not set outputs");
    return result;
  }

  // rendering of color and brightness in the 16 keys
  switch (state.mode) {
    case MODE.BANK_SELECT:
      result = Hardware::renderBankSelect(state);
      break;
    case MODE.EDIT_CHANNEL_SELECT:
      result = Hardware::renderEditChannelSelect(state);
      break;
    case MODE.EDIT_CHANNEL_VOLTAGES:
      result = Hardware::renderEditChannelVoltages(state);
      break;
    case MODE.ERROR:
      result = Hardware::renderError(state);
      break;
    case MODE.GLOBAL_EDIT:
      result = Hardware::renderGlobalEdit(state);
      break;
    case MODE.MODE_SELECT:
      result = Hardware::renderModeSelect(state);
      break;
    case MODE.RECORD_CHANNEL_SELECT:
      result = Hardware::renderRecordChannelSelect(state);
      break;
    case MODE.STEP_CHANNEL_SELECT:
      result = Hardware::renderStepChannelSelect(state);
      break;
    case MODE.STEP_SELECT:
      result = Hardware::renderStepSelect(state);
      break;
  }

  return result;
}

bool Hardware::renderBankSelect(State state) {
  seesaw_NeoPixel pixels = state.config.trellis.pixels;
  if (state.selectedKeyForCopying < 0) {
    for (uint8_t i = 0; i < 16; i++) {
      if (i != state.currentBank) {
        pixels.setPixelColor(i, 0);
      }
    }
    pixels.setPixelColor(state.currentBank, BLUE);
  }
  else {
    for (int8_t step = 0; step < 16; step++) {
      pixels.setPixelColor(
        step, 
        state.flash && (step == state.selectedKeyForCopying || state.pasteTargetKeys[step])
          ? BLUE
          : 0
      );
    }
  }
  pixels.show();
  return 1;
}

bool Hardware::renderEditChannelSelect(State state) {
  seesaw_NeoPixel pixels = state.config.trellis.pixels;
  for (uint8_t i = 0; i < 16; i++) {
    if (i > 7) {
      if (state.readyForRandom) {
        Hardware::prepareRenderingOfRandomizedKey(state, i);
      }
      else {
        pixels.setPixelColor(i, 0);
      }
    }
    else {
      if (state.randomOutputChannels[state.currentBank][i]) {
        Hardware::prepareRenderingOfRandomizedKey(state, i);
      }
      else {
        pixels.setPixelColor(
          i, 
          state.gateChannels[state.currentBank][i]
            ? PURPLE
            : state.flash == 0 && 
              (i == state.selectedKeyForCopying || 
               state.pasteTargetKeys[i] || 
               state.randomOutputChannels[state.currentBank][i])
              ? 0
              : YELLOW
        );
      }
    }
  }
  pixels.show();
  return 1;
}

bool Hardware::renderEditChannelVoltages(State state) {
  seesaw_NeoPixel pixels = state.config.trellis.pixels;
  for (uint8_t i = 0; i < 16; i++) {
    if (state.gateChannels[state.currentBank][state.currentChannel]) {
      Hardware::prepareRenderingOfStepGate(state, i);
    }
    else {
      uint32_t color = state.currentStep == i
        ? WHITE
        : YELLOW;
      Hardware::prepareRenderingOfStepVoltage(state, i, color);
    }
  }
  pixels.show();
  return 1;
}

bool Hardware::renderError(State state) {
  seesaw_NeoPixel pixels = state.config.trellis.pixels;
  for (uint8_t key = 0; key < 16; key++) {
    pixels.setPixelColor(key, state.flash ? RED : 0);
  }
  pixels.show();
  return 0; // stay in error mode
}

bool Hardware::renderGlobalEdit(State state) {
  seesaw_NeoPixel pixels = state.config.trellis.pixels;
  for (uint8_t i = 0; i < 16; i++) {
    bool allChannelVoltagesLocked = 1;
    bool allChannelStepsInactive = 1;
    for (uint8_t j = 0; j < 8; j++) {
      if (!state.lockedVoltages[state.currentBank][i][j]) {
        allChannelVoltagesLocked = 0;
      }
      if (state.activeSteps[state.currentBank][i][j]) {
        allChannelStepsInactive = 0;
      }
    }
    if (
      state.removedSteps[i] ||
      (
        state.flash == 0 &&
        (state.selectedKeyForCopying == i || state.pasteTargetKeys[i])
      )
    ) {
      pixels.setPixelColor(i, 0);
    } 
    else if (allChannelVoltagesLocked) {
      pixels.setPixelColor(i, ORANGE);
    }
    else if (allChannelStepsInactive) {
      pixels.setPixelColor(i, PURPLE);
    }
    else {
      pixels.setPixelColor(i, state.currentStep == i ? WHITE : GREEN);
    }
  }
  pixels.show();
  return 1;
}

bool Hardware::renderModeSelect(State state) {
  seesaw_NeoPixel pixels = state.config.trellis.pixels;
  for (uint8_t i = 0; i < 16; i++) {
    switch (Utils::keyQuadrant(i)) {
      case QUADRANT.INVALID:
        return 0;
      case QUADRANT.NW:
        pixels.setPixelColor(i, YELLOW); // EDIT_CHANNEL_SELECT
        break;
      case QUADRANT.NE:
        pixels.setPixelColor(i, RED); // RECORD_CHANNEL_SELECT
        break;
      case QUADRANT.SW:
        pixels.setPixelColor(i, GREEN); // GLOBAL_EDIT
        break;
      case QUADRANT.SE:
        pixels.setPixelColor(i, BLUE); // BANK_SELECT
        break;
    }
  }
  pixels.show();
  return 1;
}

bool Hardware::renderRecordChannelSelect(State state) {
  seesaw_NeoPixel pixels = state.config.trellis.pixels;
  for (uint8_t key = 0; key < 16; key++) {
    bool isFlashing = state.currentChannel == key && state.autoRecordEnabled;
    if (key < 8 && !(isFlashing && state.flash == 0)) {
      uint8_t channel = key;
      if (state.lockedVoltages[state.currentBank][state.currentStep][channel]) {
        pixels.setPixelColor(channel, ORANGE);
      }
      uint16_t voltage = state.voltages[state.currentBank][state.currentStep][channel];
      uint8_t colorValue = isFlashing 
        ? 255
        : static_cast<uint8_t>(COLOR_VALUE_MAX * voltage * PERCENTAGE_MULTIPLIER_10_BIT);
      pixels.setPixelColor(channel, colorValue, 0, 0); // shade of red
    } 
    else {
      pixels.setPixelColor(key, 0); // not illuminated
    }
  }
  pixels.show();
  return 1;
}

bool Hardware::renderStepChannelSelect(State state) {
  seesaw_NeoPixel pixels = state.config.trellis.pixels;
  for (uint8_t i = 0; i < 16; i++) {
    if (i < 8) {
      if (state.currentChannel == 1) {
        pixels.setPixelColor(i, WHITE);
      }
      else {
        pixels.setPixelColor(i, 60, 60, 60); // dimmed white
      }
    }
    else {
      pixels.setPixelColor(i, 0);
    }
  }
  pixels.show();
  return 1;
}

bool Hardware::renderStepSelect(State state) {
  seesaw_NeoPixel pixels = state.config.trellis.pixels;
  for (uint8_t i = 0; i < 16; i++) {
    if (state.selectedKeyForRecording == i) {
      uint16_t voltage = state.voltages[state.currentBank][state.selectedKeyForRecording][state.currentChannel];
      uint8_t red = static_cast<uint8_t>(COLOR_VALUE_MAX * voltage * PERCENTAGE_MULTIPLIER_10_BIT);
      pixels.setPixelColor(state.selectedKeyForRecording, red, 0, 0);
    }
    else {
      pixels.setPixelColor(i, state.currentStep == i ? WHITE : 0);
    }
  }
  pixels.show();
  return 1;
}

/**
 * @brief Set the output of a channel.
 * @param state The app-wide state struct. See State.h.
 * @param channel The channel to set, 0-7.
 * @param voltageValue The stored voltage value, a 10-bit integer.
 */
bool Hardware::setOutput(State state, const int8_t channel, const uint16_t voltageValue) {
  if (channel > 7) {
    Serial.printf("%s %u \n", "invalid channel", channel);
    return 0;
  }
  if (-1023 > voltageValue || voltageValue > 1023) {
    Serial.printf("%s %u \n", "invalid 10-bit voltage value", voltageValue);
    return 0;
  }
  Adafruit_MCP4728 dac = channel < 4 ? state.config.dac1 : state.config.dac2;
  int dacChannel = channel < 4 ? channel : channel - 4; // normalize to output indexes 0 to 3.
  bool writeSuccess = dac.setChannelValue(DAC_CHANNELS[dacChannel], Utils::tenBitToTwelveBit(voltageValue));
  if (!writeSuccess) {
    Serial.println("setOutput unsuccessful; setChannelValue error");
    return 0;
  }
  return 1;
}

/**
 * @brief Set the output of all channels.
 * @param state The app-wide state struct. See State.h.
 */
bool Hardware::setOutputsAll(State state) {
  // TODO: revise to use dac.fastWrite(). 
  // See https://adafruit.github.io/Adafruit_MCP4728/html/class_adafruit___m_c_p4728.html
  for (uint8_t channel = 0; channel < 8; channel++) {
    uint16_t voltageValue = Utils::voltageValueForStep(state, state.currentStep, channel);
    if(!Hardware::setOutput(state, channel, voltageValue)) {
      return 0;
    }
  }
  return 1;
}