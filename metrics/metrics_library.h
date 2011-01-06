// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRICS_LIBRARY_H_
#define METRICS_LIBRARY_H_

#include <sys/types.h>
#include <string>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

class MetricsLibraryInterface {
 public:
  virtual void Init() = 0;
  virtual bool SendToUMA(const std::string& name, int sample,
                         int min, int max, int nbuckets) = 0;
  virtual bool SendEnumToUMA(const std::string& name, int sample, int max) = 0;
  virtual bool SendUserActionToUMA(const std::string& action) = 0;
  virtual ~MetricsLibraryInterface() {}
};

// Library used to send metrics to both Autotest and Chrome/UMA.
class MetricsLibrary : public MetricsLibraryInterface {
 public:
  MetricsLibrary();

  // Initializes the library.
  void Init();

  // Returns whether or not the machine is running in guest mode.
  bool IsGuestMode();

  // Returns whether or not metrics collection is enabled.
  bool AreMetricsEnabled();

  // Sends histogram data to Chrome for transport to UMA and returns
  // true on success. This method results in the equivalent of an
  // asynchronous non-blocking RPC to UMA_HISTOGRAM_CUSTOM_COUNTS
  // inside Chrome (see base/histogram.h).
  //
  // |sample| is the sample value to be recorded (|min| <= |sample| < |max|).
  // |min| is the minimum value of the histogram samples (|min| > 0).
  // |max| is the maximum value of the histogram samples.
  // |nbuckets| is the number of histogram buckets.
  // [0,min) is the implicit underflow bucket.
  // [|max|,infinity) is the implicit overflow bucket.
  //
  // Note that the memory allocated in Chrome for each histogram is
  // proportional to the number of buckets. Therefore, it is strongly
  // recommended to keep this number low (e.g., 50 is normal, while
  // 100 is high).
  bool SendToUMA(const std::string& name, int sample,
                 int min, int max, int nbuckets);

  // Sends linear histogram data to Chrome for transport to UMA and
  // returns true on success. This method results in the equivalent of
  // an asynchronous non-blocking RPC to UMA_HISTOGRAM_ENUMERATION
  // inside Chrome (see base/histogram.h).
  //
  // |sample| is the sample value to be recorded (1 <= |sample| < |max|).
  // |max| is the maximum value of the histogram samples.
  // 0 is the implicit underflow bucket.
  // [|max|,infinity) is the implicit overflow bucket.
  //
  // An enumaration histogram requires |max| + 1 number of
  // buckets. Note that the memory allocated in Chrome for each
  // histogram is proportional to the number of buckets. Therefore, it
  // is strongly recommended to keep this number low (e.g., 50 is
  // normal, while 100 is high).
  bool SendEnumToUMA(const std::string& name, int sample, int max);

  // Sends a user action to Chrome for transport to UMA and returns true on
  // success. This method results in the equivalent of an asynchronous
  // non-blocking RPC to UserMetrics::RecordAction (see the comments in
  // chrome/browser/chromeos/external_metrics.cc and
  // chrome/browser/metrics/user_metrics.h on how to register new user actions).
  //
  // |action| is the user-generated event (e.g., "MuteKeyPressed").
  bool SendUserActionToUMA(const std::string& action);

  // Sends to Autotest and returns true on success.
  static bool SendToAutotest(const std::string& name, int value);

 private:
  friend class CMetricsLibraryTest;
  friend class MetricsLibraryTest;
  FRIEND_TEST(MetricsLibraryTest, AreMetricsEnabled);
  FRIEND_TEST(MetricsLibraryTest, FormatChromeMessage);
  FRIEND_TEST(MetricsLibraryTest, FormatChromeMessageTooLong);
  FRIEND_TEST(MetricsLibraryTest, IsDeviceMounted);
  FRIEND_TEST(MetricsLibraryTest, SendMessageToChrome);
  FRIEND_TEST(MetricsLibraryTest, SendMessageToChromeUMAEventsBadFileLocation);

  // Sets |*result| to whether or not the |mounts_file| indicates that
  // the |device_name| is currently mounted.  Uses |buffer| of
  // |buffer_size| to read the file.  Returns false if any error.
  bool IsDeviceMounted(const char* device_name,
                       const char* mounts_file,
                       char* buffer, int buffer_size,
                       bool* result);

  // Sends message of size |length| to Chrome for transport to UMA and
  // returns true on success.
  bool SendMessageToChrome(int32_t length, const char* message);

  // Formats a name/value message for Chrome in |buffer| and returns the
  // length of the message or a negative value on error.
  //
  // Message format is: | LENGTH(binary) | NAME | \0 | VALUE | \0 |
  //
  // The arbitrary |format| argument covers the non-LENGTH portion of the
  // message. The caller is responsible to store the \0 character
  // between NAME and VALUE (e.g. "%s%c%d", name, '\0', value).
  int32_t FormatChromeMessage(int32_t buffer_size, char* buffer,
                              const char* format, ...);

  // Time at which we last checked if metrics were enabled.
  static time_t cached_enabled_time_;

  // Cached state of whether or not metrics were enabled.
  static bool cached_enabled_;

  const char* uma_events_file_;
  const char* consent_file_;
};

#endif  // METRICS_LIBRARY_H_
