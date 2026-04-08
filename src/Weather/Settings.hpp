// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Weather/Features.hpp"
#include "net/http/Features.hpp"

#ifdef HAVE_PCMET

#include "PCMet/Settings.hpp"

#endif

struct WeatherSettings {
#ifdef HAVE_PCMET
  PCMetSettings pcmet;
#endif

#ifdef HAVE_HTTP
  /**
   * Enable Thermal Information Map?
   */
  bool enable_tim;

  /**
   * Search radius for Thermal Information Map in kilometers
   */
  unsigned tim_radius;

  /**
   * Maximum age of thermals in hours
   */
  unsigned tim_max_age;
#endif

  void SetDefaults() {
#ifdef HAVE_PCMET
    pcmet.SetDefaults();
#endif

#ifdef HAVE_HTTP
    enable_tim = false;
    tim_radius = 20;
    tim_max_age = 1;
#endif
  }
};
