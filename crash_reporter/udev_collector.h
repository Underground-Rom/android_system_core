// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _CRASH_REPORTER_UDEV_COLLECTOR_H_
#define _CRASH_REPORTER_UDEV_COLLECTOR_H_

#include <string>

#include "crash-reporter/crash_collector.h"
#include "gtest/gtest_prod.h"  // for FRIEND_TEST

class FilePath;

// Udev crash collector.
class UdevCollector : public CrashCollector {
 public:
  UdevCollector();

  virtual ~UdevCollector();

  // The udev event string should be formatted as follows:
  //   "ACTION=[action]:KERNEL=[name]:SUBSYSTEM=[subsystem]"
  // The values don't have to be in any particular order. One or more of them
  // could be omitted, in which case it would be treated as a wildcard (*).
  bool HandleCrash(const std::string &udev_event);

 private:
  friend class UdevCollectorTest;

  // Mutator for unit testing.
  void set_log_config_path(const std::string& path) {
    log_config_path_ = FilePath(path);
  }
};

#endif  // _CRASH_REPORTER_UDEV_COLLECTOR_H_
