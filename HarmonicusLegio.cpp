#include "daisy_legio.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisyLegio hw;

constexpr int NUM_EXTRA_VOICES = 3;
constexpr int NUM_CHORDS = 16;
constexpr int NUM_DETUNED = 7;

enum DetuneMode {
  DETUNE_POST,
  DETUNE_PRE,
  DETUNE_OFF
};

struct State {
  int chordIndex;

  float detuneSpread;
  float detuneMix;

  DetuneMode detuneMode;
  float detuneScale;
  float centerAmplitude;
  float sideAmplitude;

  bool editMode;
  int editedInterval;
  int originalRightSwitchPosition;

  bool arpMode;
  int arpPosition;
  int arpDirection;

  void Init() {
    chordIndex = 0;

    detuneSpread = 0.0f;
    detuneMix = 1.0f;

    detuneMode = DetuneMode::DETUNE_PRE;
    detuneScale = 0.0f;
    centerAmplitude = 1.0f;
    sideAmplitude = 0.0f;

    editMode = false;
    editedInterval = 0;
    originalRightSwitchPosition = -1;

    arpMode = false;
    arpPosition = 0;
    arpDirection = 1;
  }
};

State state;

PitchShifter chordShifters[NUM_EXTRA_VOICES];
PitchShifter detuneShifters[NUM_DETUNED];

const float detuneFactors[NUM_DETUNED] = {-0.11002313, -0.06288439, -0.01952356,
                                          0.01991221,  0.06216538,  0.10745242};

struct Chord {
  float intervals[NUM_EXTRA_VOICES];
};

Chord chords[NUM_CHORDS] = {
    {{3.0f, 7.0f, 10.0f}},   // Min7 (classic)
    {{3.0f, 7.0f, 14.0f}},   // Min9
    {{3.0f, 7.0f, 17.0f}},   // Min11
    {{3.0f, 10.0f, 14.0f}},  // Min9 spread
    {{3.0f, 10.0f, 17.0f}},  // Min11 spread
    {{3.0f, 7.0f, 19.0f}},   // Min13
    {{2.0f, 7.0f, 10.0f}},   // Sus2-7
    {{5.0f, 7.0f, 14.0f}},   // Sus9
    {{7.0f, 12.0f, 19.0f}},  // Power spread
    {{3.0f, 6.0f, 10.0f}},   // Min7b5
    {{3.0f, 6.0f, 14.0f}},   // Min9b5
    {{4.0f, 7.0f, 14.0f}},   // Maj9
    {{4.0f, 11.0f, 14.0f}},  // Maj9 spread
    {{3.0f, 7.0f, 22.0f}},   // Min add 15
    {{10.0f, 14.0f, 19.0f}}, // Upper structure
    {{7.0f, 14.0f, 21.0f}}   // Quartal spread
};

void UpdatePitchShifters() {
  for (int i = 0; i < NUM_EXTRA_VOICES; i++) {
    chordShifters[i].SetTransposition(chords[state.chordIndex].intervals[i]);
  }
}

void UpdateDetuneRatios() {
  state.detuneScale =
      1.5 * powf(state.detuneSpread, 6) - 0.8 * powf(state.detuneSpread, 5) +
      0.3 * powf(state.detuneSpread, 3) - 0.3 * powf(state.detuneSpread, 2) +
      0.3 * state.detuneSpread;

  for (int i = 0; i < NUM_DETUNED; i++) {
    float ratio = 1.0f + detuneFactors[i] * state.detuneScale;
    detuneShifters[i].SetRatio(ratio);
  }
}

void UpdatesideAmplitudes() {
  state.centerAmplitude = -0.55366 * state.detuneMix + 0.99785;
  state.sideAmplitude = -0.73764 * state.detuneMix * state.detuneMix +
                          1.2841 * state.detuneMix + 0.044372;
}

void ProcessEncoder() {
  if (hw.encoder.Pressed()) {
    if (state.originalRightSwitchPosition == -1) {
      state.originalRightSwitchPosition = hw.sw[DaisyLegio::SW_RIGHT].Read();
    }

    if (hw.encoder.Increment() != 0) {
      float &interval =
          chords[state.chordIndex].intervals[state.editedInterval];
      interval = fclamp(interval + hw.encoder.Increment(), -24.0f, 24.0f);
      UpdatePitchShifters();
    }

    state.editedInterval = hw.sw[DaisyLegio::SW_RIGHT].Read();
    state.editMode = true;
    hw.SetLed(DaisyLegio::LED_RIGHT, 1.0f, 0.0f, 0.0f);
  } else {
    if (state.originalRightSwitchPosition != -1) {
      if (hw.sw[DaisyLegio::SW_RIGHT].Read() ==
          state.originalRightSwitchPosition) {
        state.originalRightSwitchPosition = -1;
        state.editMode = false;

        hw.SetLed(DaisyLegio::LED_RIGHT, 0.0f, 0.0f, 0.0f);
      }
    } else {
      if (hw.encoder.Increment() != 0) {
        state.chordIndex =
            (state.chordIndex + hw.encoder.Increment()) % NUM_CHORDS;
        if (state.chordIndex < 0)
          state.chordIndex += NUM_CHORDS;
        UpdatePitchShifters();
      }
    }
  }
}

void ProcessLeftSwitch() {
  switch (hw.sw[DaisyLegio::SW_LEFT].Read()) {
  case Switch3::POS_UP:
    state.detuneMode = DetuneMode::DETUNE_PRE;
    break;
  case Switch3::POS_DOWN:
    state.detuneMode = DetuneMode::DETUNE_OFF;
    break;
  default:
    state.detuneMode = DetuneMode::DETUNE_POST;
    break;
  }
}

void ProcessRightSwitch() {
  if (state.editMode) {
    return;
  }

  switch (hw.sw[DaisyLegio::SW_RIGHT].Read()) {
  case Switch3::POS_UP:
    state.arpMode = true;
    state.arpDirection = 1;
    break;
  case Switch3::POS_DOWN:
    state.arpMode = true;
    state.arpDirection = -1;
    break;
  default:
    state.arpMode = false;
    break;
  }
}

void ProcessControls() {
  hw.ProcessAllControls();

  ProcessEncoder();
  ProcessLeftSwitch();
  ProcessRightSwitch();

  float detuneMix = hw.GetKnobValue(DaisyLegio::CONTROL_KNOB_BOTTOM);
  if (detuneMix != state.detuneMix) {
    state.detuneMix = detuneMix;
    UpdatesideAmplitudes();
  }

  float detuneSpread = hw.GetKnobValue(DaisyLegio::CONTROL_KNOB_TOP);
  if (detuneSpread != state.detuneSpread) {
    state.detuneSpread = detuneSpread;
    UpdateDetuneRatios();
  }

  if (state.arpMode && hw.gate.Trig()) {
    state.arpPosition =
        (state.arpPosition + state.arpDirection) % (NUM_EXTRA_VOICES + 1);
    if (state.arpPosition < 0) {
      state.arpPosition = NUM_EXTRA_VOICES;
    }
  }
}

float ProcessDetune(float in) {
  float swarm = in * state.centerAmplitude;
  for (int i = 0; i < NUM_DETUNED; i++) {
    swarm += detuneShifters[i].Process(in) * state.sideAmplitude;
  }
  return swarm;
}

float ProcessChord(float in) {
  if (state.arpMode) {
    if (state.arpPosition == 0) {
      return in;
    }

    return chordShifters[state.arpPosition - 1].Process(in);
  }

  float chord = in;
  for (int v = 0; v < NUM_EXTRA_VOICES; v++) {
    chord += chordShifters[v].Process(in);
  }
  return chord / (1 + NUM_EXTRA_VOICES);
}

void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                   AudioHandle::InterleavingOutputBuffer out, size_t size) {
  for (size_t i = 0; i < size; i += 2) {
    float mix = (in[i] + in[i + 1]) * 0.5f;

    if (state.detuneMode == DetuneMode::DETUNE_PRE) {
      mix = ProcessDetune(mix);
    }
    
    mix = ProcessChord(mix);

    if (state.detuneMode == DetuneMode::DETUNE_POST) {
      mix = ProcessDetune(mix);
    }

    out[i] = mix;
    out[i + 1] = mix;
  }
}

int main(void) {
  hw.Init();
  float sampleRate = hw.AudioSampleRate();

  state.Init();

  for (int i = 0; i < NUM_EXTRA_VOICES; i++) {
    chordShifters[i].Init(sampleRate);
    chordShifters[i].SetDelSize(2400);
  }

  for (int i = 0; i < NUM_DETUNED; i++) {
    detuneShifters[i].Init(sampleRate);
    detuneShifters[i].SetDelSize(1200);
  }

  UpdatePitchShifters();
  UpdatesideAmplitudes();
  UpdateDetuneRatios();

  hw.StartAudio(AudioCallback);
  hw.StartAdc();

  while (1) {
    ProcessControls();
    hw.UpdateLeds();
  }
}