// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ToneSynthesiser.hpp"
#include "thread/Mutex.hxx"

#include <algorithm>

/**
 * This class generates vario sound.
 */
class VarioSynthesiser final : public ToneSynthesiser {
  /**
   * This mutex protects all atttributes below.  It is locked
   * automatically by all public methods.
   */
  Mutex mutex;

  /**
   * The number of audible samples in each period.
   */
  size_t audible_count;

  /**
   * The number of silent samples in each period.  If this is zero,
   * then no silence will be generated (continuous tone).
   */
  size_t silence_count;

  /**
   * The number of audible/silence samples remaining in the current
   * period.  These two attributes will be reset to the according
   * _count value when both reach zero.
   */
  size_t audible_remaining, silence_remaining;

  /**
   * The number of audible samples that have already been generated in
   * the current phase.  Used for fade-in shaping.
   */
  size_t audible_phase_elapsed;

  bool dead_band_enabled;
  bool dead_band_active;
  bool continuous_tone;
  bool phase_attack_enabled;

  /**
   * The tone frequency for #min_vario.
   */
  unsigned min_frequency;

  /**
   * The tone frequency for stationary altitude.
   */
  unsigned zero_frequency;

  /**
   * The tone frequency for #max_vario.
   */
  unsigned max_frequency;

  /**
   * The minimum silence+audible period for #max_vario.
   */
  unsigned min_period_ms;

  /**
   * The maximum silence+audible period for #min_vario.
   */
  unsigned max_period_ms;

  /**
   * The vario range of the "dead band" during which no sound is emitted
   * [cm/s].
   */
  int min_dead, max_dead;

  /**
   * The number of samples used for fade-in/fade-out shaping.
   */
  size_t fade_samples;

  /**
   * Deadband hysteresis [cm/s].
   */
  int dead_band_hysteresis;

public:
  explicit VarioSynthesiser(unsigned sample_rate)
    :ToneSynthesiser(sample_rate),
     audible_count(0), silence_count(1),
     audible_remaining(0), silence_remaining(0),
     audible_phase_elapsed(0),
     dead_band_enabled(false), dead_band_active(false),
     continuous_tone(false), phase_attack_enabled(true),
     min_frequency(200), zero_frequency(500), max_frequency(1500),
     min_period_ms(150), max_period_ms(600),
     min_dead(-30), max_dead(10),
     fade_samples(std::max<size_t>(sample_rate / 125u, 1u)),
     dead_band_hysteresis(5) {}

  /**
   * Update the vario value.  This calculates a new tone frequency and
   * a new "silence" rate (for positive vario values).
   *
   * @param vario the current vario value [m/s]
   */
  void SetVario(double vario);

  /**
   * Produce silence from now on.
   */
  void SetSilence();

  /**
   * Enable/disable the dead band silence
   */
  void SetDeadBand(bool enabled) {
    dead_band_enabled = enabled;
    if (!enabled)
      dead_band_active = false;
  }

  /**
   * Set the base frequencies for minimum, zero and maximum lift
   */
  void SetFrequencies(unsigned min, unsigned zero, unsigned max) {
    min_frequency = min;
    zero_frequency = zero;
    max_frequency = max;
  }

  /**
   * Set the time periods for minimum and maximum lift
   */
  void SetPeriods(unsigned min, unsigned max) {
    min_period_ms = min;
    max_period_ms = max;
  }

  /**
   * Set the vario range of the "dead band" during which no sound is emitted
   */
  void SetDeadBandRange(double min, double max) {
    min_dead = (int)(min * 100);
    max_dead = (int)(max * 100);
  }

  /* methods from class PCMSynthesiser */
  virtual void Synthesise(int16_t *buffer, size_t n);

private:
  /**
   * Same as SetSilence(), but doesn't lock the mutex.
   */
  void UnsafeSetSilence();

  bool ShouldMuteForDeadBand(int ivario);

  void ApplyAttackEnvelope(int16_t *buffer, size_t n) noexcept;

  void ApplyEnvelope(int16_t *buffer, size_t n,
                     size_t remaining_before) noexcept;

  /**
   * Convert a vario value to a tone frequency.
   *
   * @param ivario the current vario value [cm/s]
   */
  [[gnu::const]]
  unsigned VarioToFrequency(int ivario);

};
