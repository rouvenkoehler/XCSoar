// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "VarioSynthesiser.hpp"
#include "Math/FastMath.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>

/**
 * The minimum and maximum vario range for the constants below [cm/s].
 */
static constexpr int min_vario = -500, max_vario = 500;

static constexpr unsigned envelope_scale = 65536;

unsigned
VarioSynthesiser::VarioToFrequency(int ivario)
{
  return ivario > 0
    ? (zero_frequency + (unsigned)ivario * (max_frequency - zero_frequency)
       / (unsigned)max_vario)
    : (zero_frequency - (unsigned)(ivario * (int)(zero_frequency - min_frequency) / min_vario));
}

bool
VarioSynthesiser::ShouldMuteForDeadBand(int ivario)
{
  const int hysteresis = std::min(dead_band_hysteresis,
                                 std::max(0, (max_dead - min_dead) / 2));

  if (dead_band_active) {
    if (ivario >= min_dead - hysteresis && ivario <= max_dead + hysteresis)
      return true;

    dead_band_active = false;
    return false;
  }

  if (ivario >= min_dead + hysteresis && ivario <= max_dead - hysteresis) {
    dead_band_active = true;
    return true;
  }

  return false;
}

void
VarioSynthesiser::ApplyAttackEnvelope(int16_t *buffer, size_t n) noexcept
{
  if (!phase_attack_enabled)
    return;

  for (size_t i = 0; i < n; ++i) {
    const size_t position = audible_phase_elapsed + i;
    if (position >= fade_samples)
      break;

    const unsigned scale =
      (unsigned)((uint64_t)(position + 1) * envelope_scale / fade_samples);
    buffer[i] = (int16_t)((int32_t)buffer[i] * (int32_t)scale
                         / (int32_t)envelope_scale);
  }

  audible_phase_elapsed += n;
  if (audible_phase_elapsed >= fade_samples)
    phase_attack_enabled = false;
}

void
VarioSynthesiser::ApplyEnvelope(int16_t *buffer, size_t n,
                               size_t remaining_before) noexcept
{
  assert(n <= remaining_before);

  for (size_t i = 0; i < n; ++i) {
    unsigned scale = envelope_scale;

    if (phase_attack_enabled) {
      const size_t position = audible_phase_elapsed + i;
      if (position < fade_samples)
        scale = std::min(scale,
                         (unsigned)((uint64_t)(position + 1) * envelope_scale
                                   / fade_samples));
    }

    const size_t remaining = remaining_before - i;
    if (remaining <= fade_samples)
      scale = std::min(scale,
                       (unsigned)((uint64_t)remaining * envelope_scale
                                 / fade_samples));

    buffer[i] = (int16_t)((int32_t)buffer[i] * (int32_t)scale
                         / (int32_t)envelope_scale);
  }

  audible_phase_elapsed += n;
  if (audible_phase_elapsed >= fade_samples)
    phase_attack_enabled = false;
}

void
VarioSynthesiser::SetVario(double vario)
{
  const std::lock_guard lock{mutex};

  const int ivario = std::clamp((int)(vario * 100), min_vario, max_vario);

  if (dead_band_enabled && ShouldMuteForDeadBand(ivario)) {
    /* inside the "dead band" */
    UnsafeSetSilence();
    return;
  }

  /* update the ToneSynthesiser base class */
  SetTone(VarioToFrequency(ivario));

  if (ivario > 0) {
    continuous_tone = false;

    /* while climbing, the vario sound gets interrupted by silence
      periodically */

    const unsigned period_ms = sample_rate
      * (min_period_ms + (max_vario - ivario)
         * (max_period_ms - min_period_ms) / max_vario)
      / 1000;

    silence_count = period_ms / 3;
    audible_count = period_ms - silence_count;

    /* preserve the old "_remaining" values as much as possible, to
       avoid chopping off the previous tone */

    if (audible_remaining > audible_count)
      audible_remaining = audible_count;

    if (silence_remaining > silence_count)
      silence_remaining = silence_count;
  } else {
    /* continuous tone while sinking */
    const bool start_from_silence = !continuous_tone && audible_remaining == 0;

    continuous_tone = true;
    audible_count = 1;
    silence_count = 0;
    silence_remaining = 0;

    if (start_from_silence) {
      audible_phase_elapsed = 0;
      phase_attack_enabled = true;
    }
  }
}

void
VarioSynthesiser::SetSilence()
{
  const std::lock_guard lock{mutex};
  dead_band_active = false;
  UnsafeSetSilence();
}

void
VarioSynthesiser::UnsafeSetSilence()
{
  const bool was_continuous = continuous_tone;
  const bool was_audible = was_continuous || audible_remaining > 0;

  continuous_tone = false;
  audible_count = 0;
  silence_count = 1;

  if (was_audible) {
    /* quit the current period quickly, but keep a small release
       window so the tone fades out smoothly before the final
       zero-crossing stop */
    audible_remaining = was_continuous
      ? fade_samples
      : std::max<size_t>(std::min(audible_remaining, fade_samples), 1u);
    silence_remaining = 1;
    audible_phase_elapsed = 0;
    phase_attack_enabled = false;
  } else {
    audible_remaining = 0;
    silence_remaining = 0;
    audible_phase_elapsed = 0;
    phase_attack_enabled = true;
  }
}


void
VarioSynthesiser::Synthesise(int16_t *buffer, size_t n)
{
  const std::lock_guard lock{mutex};

  assert(audible_count > 0 || silence_count > 0);

  if (continuous_tone) {
    /* magic value for "continuous tone" */
    ToneSynthesiser::Synthesise(buffer, n);
    ApplyAttackEnvelope(buffer, n);
    return;
  }

  while (n > 0) {
    if (audible_remaining > 0) {
      /* generate a period of audible tone */

      unsigned o = silence_count > 0
        ? std::min(n, audible_remaining)
        : n;
      ToneSynthesiser::Synthesise(buffer, o);
      ApplyEnvelope(buffer, o, audible_remaining);
      buffer += o;
      n -= o;
      audible_remaining -= o;

      if (audible_remaining == 0 && silence_remaining > 0) {
        /* finish the current sine wave to avoid clicking noise */
        audible_remaining = ToZero();
        if (audible_remaining == 0)
          /* finished, we can now emit a period of silence */
          Restart();
        else {
          audible_phase_elapsed = 0;
          phase_attack_enabled = false;
        }
      }
    } else if (silence_remaining > 0) {
      /* generate a period of silence (climbing) */

      unsigned o = audible_count > 0
        ? std::min(n, silence_remaining)
        : n;
      /* the "silence" PCM sample value is zero */
      std::fill_n(buffer, o, 0);
      buffer += o;
      n -= o;
      silence_remaining -= o;
    } else {
      /* period finished, begin next one */

      audible_remaining = audible_count;
      silence_remaining = silence_count;
      audible_phase_elapsed = 0;
      phase_attack_enabled = true;
    }
  }
}
