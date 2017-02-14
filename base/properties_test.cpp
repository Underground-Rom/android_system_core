/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "android-base/properties.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

using namespace std::chrono_literals;

TEST(properties, smoke) {
  android::base::SetProperty("debug.libbase.property_test", "hello");

  std::string s = android::base::GetProperty("debug.libbase.property_test", "");
  ASSERT_EQ("hello", s);

  android::base::SetProperty("debug.libbase.property_test", "world");
  s = android::base::GetProperty("debug.libbase.property_test", "");
  ASSERT_EQ("world", s);

  s = android::base::GetProperty("this.property.does.not.exist", "");
  ASSERT_EQ("", s);

  s = android::base::GetProperty("this.property.does.not.exist", "default");
  ASSERT_EQ("default", s);
}

TEST(properties, empty) {
  // Because you can't delete a property, people "delete" them by
  // setting them to the empty string. In that case we'd want to
  // keep the default value (like cutils' property_get did).
  android::base::SetProperty("debug.libbase.property_test", "");
  std::string s = android::base::GetProperty("debug.libbase.property_test", "default");
  ASSERT_EQ("default", s);
}

static void CheckGetBoolProperty(bool expected, const std::string& value, bool default_value) {
  android::base::SetProperty("debug.libbase.property_test", value.c_str());
  ASSERT_EQ(expected, android::base::GetBoolProperty("debug.libbase.property_test", default_value));
}

TEST(properties, GetBoolProperty_true) {
  CheckGetBoolProperty(true, "1", false);
  CheckGetBoolProperty(true, "y", false);
  CheckGetBoolProperty(true, "yes", false);
  CheckGetBoolProperty(true, "on", false);
  CheckGetBoolProperty(true, "true", false);
}

TEST(properties, GetBoolProperty_false) {
  CheckGetBoolProperty(false, "0", true);
  CheckGetBoolProperty(false, "n", true);
  CheckGetBoolProperty(false, "no", true);
  CheckGetBoolProperty(false, "off", true);
  CheckGetBoolProperty(false, "false", true);
}

TEST(properties, GetBoolProperty_default) {
  CheckGetBoolProperty(true, "burp", true);
  CheckGetBoolProperty(false, "burp", false);
}

template <typename T> void CheckGetIntProperty() {
  // Positive and negative.
  android::base::SetProperty("debug.libbase.property_test", "-12");
  EXPECT_EQ(T(-12), android::base::GetIntProperty<T>("debug.libbase.property_test", 45));
  android::base::SetProperty("debug.libbase.property_test", "12");
  EXPECT_EQ(T(12), android::base::GetIntProperty<T>("debug.libbase.property_test", 45));

  // Default value.
  android::base::SetProperty("debug.libbase.property_test", "");
  EXPECT_EQ(T(45), android::base::GetIntProperty<T>("debug.libbase.property_test", 45));

  // Bounds checks.
  android::base::SetProperty("debug.libbase.property_test", "0");
  EXPECT_EQ(T(45), android::base::GetIntProperty<T>("debug.libbase.property_test", 45, 1, 2));
  android::base::SetProperty("debug.libbase.property_test", "1");
  EXPECT_EQ(T(1), android::base::GetIntProperty<T>("debug.libbase.property_test", 45, 1, 2));
  android::base::SetProperty("debug.libbase.property_test", "2");
  EXPECT_EQ(T(2), android::base::GetIntProperty<T>("debug.libbase.property_test", 45, 1, 2));
  android::base::SetProperty("debug.libbase.property_test", "3");
  EXPECT_EQ(T(45), android::base::GetIntProperty<T>("debug.libbase.property_test", 45, 1, 2));
}

template <typename T> void CheckGetUintProperty() {
  // Positive.
  android::base::SetProperty("debug.libbase.property_test", "12");
  EXPECT_EQ(T(12), android::base::GetUintProperty<T>("debug.libbase.property_test", 45));

  // Default value.
  android::base::SetProperty("debug.libbase.property_test", "");
  EXPECT_EQ(T(45), android::base::GetUintProperty<T>("debug.libbase.property_test", 45));

  // Bounds checks.
  android::base::SetProperty("debug.libbase.property_test", "12");
  EXPECT_EQ(T(12), android::base::GetUintProperty<T>("debug.libbase.property_test", 33, 22));
  android::base::SetProperty("debug.libbase.property_test", "12");
  EXPECT_EQ(T(5), android::base::GetUintProperty<T>("debug.libbase.property_test", 5, 10));
}

TEST(properties, GetIntProperty_int8_t) { CheckGetIntProperty<int8_t>(); }
TEST(properties, GetIntProperty_int16_t) { CheckGetIntProperty<int16_t>(); }
TEST(properties, GetIntProperty_int32_t) { CheckGetIntProperty<int32_t>(); }
TEST(properties, GetIntProperty_int64_t) { CheckGetIntProperty<int64_t>(); }

TEST(properties, GetUintProperty_uint8_t) { CheckGetUintProperty<uint8_t>(); }
TEST(properties, GetUintProperty_uint16_t) { CheckGetUintProperty<uint16_t>(); }
TEST(properties, GetUintProperty_uint32_t) { CheckGetUintProperty<uint32_t>(); }
TEST(properties, GetUintProperty_uint64_t) { CheckGetUintProperty<uint64_t>(); }

TEST(properties, WaitForProperty) {
  std::atomic<bool> flag{false};
  std::thread thread([&]() {
    std::this_thread::sleep_for(100ms);
    android::base::SetProperty("debug.libbase.WaitForProperty_test", "a");
    while (!flag) std::this_thread::yield();
    android::base::SetProperty("debug.libbase.WaitForProperty_test", "b");
  });

  android::base::WaitForProperty("debug.libbase.WaitForProperty_test", "a");
  flag = true;
  android::base::WaitForProperty("debug.libbase.WaitForProperty_test", "b");
  thread.join();
}
