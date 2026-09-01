// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>
#include <type_traits>

struct VarioSoundSettings {
  bool enabled;
  uint8_t volume;
  bool dead_band_enabled;

  unsigned min_frequency;
  unsigned zero_frequency;
  unsigned max_frequency;

  unsigned min_period_ms;
  unsigned max_period_ms;

  double min_dead;
  double max_dead;

  /**
   * Override for the Kalman filter noise variance used when processing the
   * barometric pressure sensor (in hPa²).  A value of 0 means "use the
   * sensor-specific default" (looked up by sensor name in NonGPSSensors.java,
   * falling back to 0.05 for unknown sensors).  Set to a positive value to
   * override; smaller values make the vario more responsive, larger values
   * produce heavier smoothing.
   */
  double kalman_filter_variance;

  void SetDefaults();
};

static_assert(std::is_trivial<VarioSoundSettings>::value, "type is not trivial");
