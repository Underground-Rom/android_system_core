// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metrics_daemon.h"

#include <dbus/dbus-glib-lowlevel.h>

#include <base/file_util.h>
#include <base/logging.h>

#include "counter.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using std::string;

#define SAFE_MESSAGE(e) (e.message ? e.message : "unknown error")
#define DBUS_IFACE_CRASH_REPORTER "org.chromium.CrashReporter"
#define DBUS_IFACE_FLIMFLAM_MANAGER "org.chromium.flimflam.Manager"
#define DBUS_IFACE_POWER_MANAGER "org.chromium.PowerManager"
#define DBUS_IFACE_SESSION_MANAGER "org.chromium.SessionManagerInterface"

static const int kSecondsPerMinute = 60;
static const int kMinutesPerHour = 60;
static const int kHoursPerDay = 24;
static const int kMinutesPerDay = kHoursPerDay * kMinutesPerHour;
static const int kSecondsPerDay = kSecondsPerMinute * kMinutesPerDay;
static const int kDaysPerWeek = 7;
static const int kSecondsPerWeek = kSecondsPerDay * kDaysPerWeek;

// The daily use monitor is scheduled to a 1-minute interval after
// initial user activity and then it's exponentially backed off to
// 10-minute intervals. Although not required, the back off is
// implemented because the histogram buckets are spaced exponentially
// anyway and to avoid too frequent metrics daemon process wake-ups
// and file I/O.
static const int kUseMonitorIntervalInit = 1 * kSecondsPerMinute;
static const int kUseMonitorIntervalMax = 10 * kSecondsPerMinute;

const char kKernelCrashDetectedFile[] = "/tmp/kernel-crash-detected";
static const char kUncleanShutdownDetectedFile[] =
      "/tmp/unclean-shutdown-detected";

// static metrics parameters.
const char MetricsDaemon::kMetricDailyUseTimeName[] =
    "Logging.DailyUseTime";
const int MetricsDaemon::kMetricDailyUseTimeMin = 1;
const int MetricsDaemon::kMetricDailyUseTimeMax = kMinutesPerDay;
const int MetricsDaemon::kMetricDailyUseTimeBuckets = 50;

const char MetricsDaemon::kMetricTimeToNetworkDropName[] =
    "Network.TimeToDrop";
const int MetricsDaemon::kMetricTimeToNetworkDropMin = 1;
const int MetricsDaemon::kMetricTimeToNetworkDropMax =
    8 /* hours */ * kMinutesPerHour * kSecondsPerMinute;
const int MetricsDaemon::kMetricTimeToNetworkDropBuckets = 50;

// crash interval metrics
const char MetricsDaemon::kMetricKernelCrashIntervalName[] =
    "Logging.KernelCrashInterval";
const char MetricsDaemon::kMetricUncleanShutdownIntervalName[] =
    "Logging.UncleanShutdownInterval";
const char MetricsDaemon::kMetricUserCrashIntervalName[] =
    "Logging.UserCrashInterval";

const int MetricsDaemon::kMetricCrashIntervalMin = 1;
const int MetricsDaemon::kMetricCrashIntervalMax =
    4 * kSecondsPerWeek;
const int MetricsDaemon::kMetricCrashIntervalBuckets = 50;

// crash frequency metrics
const char MetricsDaemon::kMetricAnyCrashesDailyName[] =
    "Logging.AnyCrashesDaily";
const char MetricsDaemon::kMetricKernelCrashesDailyName[] =
    "Logging.KernelCrashesDaily";
const char MetricsDaemon::kMetricUncleanShutdownsDailyName[] =
    "Logging.UncleanShutdownsDaily";
const char MetricsDaemon::kMetricUserCrashesDailyName[] =
    "Logging.UserCrashesDaily";
const char MetricsDaemon::kMetricCrashesDailyMin = 1;
const char MetricsDaemon::kMetricCrashesDailyMax = 100;
const char MetricsDaemon::kMetricCrashesDailyBuckets = 50;



// static
const char* MetricsDaemon::kDBusMatches_[] = {
  "type='signal',"
  "interface='" DBUS_IFACE_CRASH_REPORTER "',"
  "path='/',"
  "member='UserCrash'",

  "type='signal',"
  "sender='org.chromium.flimflam',"
  "interface='" DBUS_IFACE_FLIMFLAM_MANAGER "',"
  "path='/',"
  "member='StateChanged'",

  "type='signal',"
  "interface='" DBUS_IFACE_POWER_MANAGER "',"
  "path='/'",

  "type='signal',"
  "sender='org.chromium.SessionManager',"
  "interface='" DBUS_IFACE_SESSION_MANAGER "',"
  "path='/org/chromium/SessionManager',"
  "member='SessionStateChanged'",
};

// static
const char* MetricsDaemon::kNetworkStates_[] = {
#define STATE(name, capname) #name,
#include "network_states.h"
};

// static
const char* MetricsDaemon::kPowerStates_[] = {
#define STATE(name, capname) #name,
#include "power_states.h"
};

// static
const char* MetricsDaemon::kSessionStates_[] = {
#define STATE(name, capname) #name,
#include "session_states.h"
};

// Invokes a remote method over D-Bus that takes no input arguments
// and returns a string result. The method call is issued with a 2
// second blocking timeout. Returns an empty string on failure or
// timeout.
static string DBusGetString(DBusConnection* connection,
                            const string& destination,
                            const string& path,
                            const string& interface,
                            const string& method) {
  DBusMessage* message =
      dbus_message_new_method_call(destination.c_str(),
                                   path.c_str(),
                                   interface.c_str(),
                                   method.c_str());
  if (!message) {
    DLOG(WARNING) << "DBusGetString: unable to allocate a message";
    return "";
  }

  DBusError error;
  dbus_error_init(&error);
  const int kTimeout = 2000;  // ms
  DLOG(INFO) << "DBusGetString: dest=" << destination << " path=" << path
             << " iface=" << interface << " method=" << method;
  DBusMessage* reply =
      dbus_connection_send_with_reply_and_block(connection, message, kTimeout,
                                                &error);
  dbus_message_unref(message);
  if (dbus_error_is_set(&error) || !reply) {
    DLOG(WARNING) << "DBusGetString: call failed";
    return "";
  }
  DBusMessageIter iter;
  dbus_message_iter_init(reply, &iter);
  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
    NOTREACHED();
    dbus_message_unref(reply);
    return "";
  }
  const char* c_result = "";
  dbus_message_iter_get_basic(&iter, &c_result);
  string result = c_result;
  DLOG(INFO) << "DBusGetString: result=" << result;
  dbus_message_unref(reply);
  return result;
}

MetricsDaemon::MetricsDaemon()
    : network_state_(kUnknownNetworkState),
      power_state_(kUnknownPowerState),
      session_state_(kUnknownSessionState),
      user_active_(false),
      usemon_interval_(0),
      usemon_source_(NULL) {}

MetricsDaemon::~MetricsDaemon() {}

void MetricsDaemon::Run(bool run_as_daemon) {
  if (run_as_daemon && daemon(0, 0) != 0)
    return;

  if (CheckSystemCrash(kKernelCrashDetectedFile)) {
    ProcessKernelCrash();
  }

  if (CheckSystemCrash(kUncleanShutdownDetectedFile)) {
    ProcessUncleanShutdown();
  }

  Loop();
}

void MetricsDaemon::Init(bool testing, MetricsLibraryInterface* metrics_lib) {
  testing_ = testing;
  DCHECK(metrics_lib != NULL);
  metrics_lib_ = metrics_lib;

  static const char kDailyUseRecordFile[] = "/var/log/metrics/daily-usage";
  daily_use_.reset(new chromeos_metrics::TaggedCounter());
  daily_use_->Init(kDailyUseRecordFile, &ReportDailyUse, this);

  static const char kUserCrashIntervalRecordFile[] =
      "/var/log/metrics/user-crash-interval";
  user_crash_interval_.reset(new chromeos_metrics::TaggedCounter());
  user_crash_interval_->Init(kUserCrashIntervalRecordFile,
                             &ReportUserCrashInterval, this);

  static const char kKernelCrashIntervalRecordFile[] =
      "/var/log/metrics/kernel-crash-interval";
  kernel_crash_interval_.reset(new chromeos_metrics::TaggedCounter());
  kernel_crash_interval_->Init(kKernelCrashIntervalRecordFile,
                               &ReportKernelCrashInterval, this);

  static const char kUncleanShutdownDetectedFile[] =
      "/var/log/metrics/unclean-shutdown-interval";
  unclean_shutdown_interval_.reset(new chromeos_metrics::TaggedCounter());
  unclean_shutdown_interval_->Init(kUncleanShutdownDetectedFile,
                                   &ReportUncleanShutdownInterval, this);

  static const char kUserCrashesDailyRecordFile[] =
      "/var/log/metrics/user-crashes-daily";
  user_crashes_daily_.reset(new chromeos_metrics::FrequencyCounter());
  user_crashes_daily_->Init(kUserCrashesDailyRecordFile,
                            &ReportUserCrashesDaily,
                            this,
                            chromeos_metrics::kSecondsPerDay);

  static const char kKernelCrashesDailyRecordFile[] =
      "/var/log/metrics/kernel-crashes-daily";
  kernel_crashes_daily_.reset(new chromeos_metrics::FrequencyCounter());
  kernel_crashes_daily_->Init(kKernelCrashesDailyRecordFile,
                              &ReportKernelCrashesDaily,
                              this,
                              chromeos_metrics::kSecondsPerDay);

  static const char kUncleanShutdownsDailyRecordFile[] =
      "/var/log/metrics/unclean-shutdowns-daily";
  unclean_shutdowns_daily_.reset(new chromeos_metrics::FrequencyCounter());
  unclean_shutdowns_daily_->Init(kUncleanShutdownsDailyRecordFile,
                                 &ReportUncleanShutdownsDaily,
                                 this,
                                 chromeos_metrics::kSecondsPerDay);

  static const char kAnyCrashesUserCrashDailyRecordFile[] =
      "/var/log/metrics/any-crashes-daily";
  any_crashes_daily_.reset(new chromeos_metrics::FrequencyCounter());
  any_crashes_daily_->Init(kAnyCrashesUserCrashDailyRecordFile,
                           &ReportAnyCrashesDaily,
                           this,
                           chromeos_metrics::kSecondsPerDay);

  // Don't setup D-Bus and GLib in test mode.
  if (testing)
    return;

  g_thread_init(NULL);
  g_type_init();
  dbus_g_thread_init();

  DBusError error;
  dbus_error_init(&error);

  DBusConnection* connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
  LOG_IF(FATAL, dbus_error_is_set(&error)) <<
      "No D-Bus connection: " << SAFE_MESSAGE(error);

  dbus_connection_setup_with_g_main(connection, NULL);

  // Registers D-Bus matches for the signals we would like to catch.
  for (unsigned int m = 0; m < arraysize(kDBusMatches_); m++) {
    const char* match = kDBusMatches_[m];
    DLOG(INFO) << "adding dbus match: " << match;
    dbus_bus_add_match(connection, match, &error);
    LOG_IF(FATAL, dbus_error_is_set(&error)) <<
        "unable to add a match: " << SAFE_MESSAGE(error);
  }

  // Adds the D-Bus filter routine to be called back whenever one of
  // the registered D-Bus matches is successful. The daemon is not
  // activated for D-Bus messages that don't match.
  CHECK(dbus_connection_add_filter(connection, MessageFilter, this, NULL));

  // Initializes the current network state by retrieving it from flimflam.
  string state_name = DBusGetString(connection, "org.chromium.flimflam", "/",
                                    DBUS_IFACE_FLIMFLAM_MANAGER, "GetState");
  NetStateChanged(state_name.c_str(), TimeTicks::Now());
}

void MetricsDaemon::Loop() {
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_main_loop_run(loop);
}

// static
DBusHandlerResult MetricsDaemon::MessageFilter(DBusConnection* connection,
                                               DBusMessage* message,
                                               void* user_data) {
  Time now = Time::Now();
  TimeTicks ticks = TimeTicks::Now();
  DLOG(INFO) << "message intercepted @ " << now.ToInternalValue();

  int message_type = dbus_message_get_type(message);
  if (message_type != DBUS_MESSAGE_TYPE_SIGNAL) {
    DLOG(WARNING) << "unexpected message type " << message_type;
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  // Signal messages always have interfaces.
  const char* interface = dbus_message_get_interface(message);
  CHECK(interface != NULL);

  MetricsDaemon* daemon = static_cast<MetricsDaemon*>(user_data);

  DBusMessageIter iter;
  dbus_message_iter_init(message, &iter);
  if (strcmp(interface, DBUS_IFACE_CRASH_REPORTER) == 0) {
    CHECK(strcmp(dbus_message_get_member(message),
                 "UserCrash") == 0);
    daemon->ProcessUserCrash();
  } else if (strcmp(interface, DBUS_IFACE_FLIMFLAM_MANAGER) == 0) {
    CHECK(strcmp(dbus_message_get_member(message),
                 "StateChanged") == 0);

    char* state_name;
    dbus_message_iter_get_basic(&iter, &state_name);
    daemon->NetStateChanged(state_name, ticks);
  } else if (strcmp(interface, DBUS_IFACE_POWER_MANAGER) == 0) {
    const char* member = dbus_message_get_member(message);
    if (strcmp(member, "ScreenIsLocked") == 0) {
      daemon->SetUserActiveState(false, now);
    } else if (strcmp(member, "ScreenIsUnlocked") == 0) {
      daemon->SetUserActiveState(true, now);
    } else if (strcmp(member, "PowerStateChanged") == 0) {
      char* state_name;
      dbus_message_iter_get_basic(&iter, &state_name);
      daemon->PowerStateChanged(state_name, now);
    }
  } else if (strcmp(interface, DBUS_IFACE_SESSION_MANAGER) == 0) {
    CHECK(strcmp(dbus_message_get_member(message),
                 "SessionStateChanged") == 0);

    char* state_name;
    dbus_message_iter_get_basic(&iter, &state_name);
    daemon->SessionStateChanged(state_name, now);
  } else {
    DLOG(WARNING) << "unexpected interface: " << interface;
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

void MetricsDaemon::NetStateChanged(const char* state_name, TimeTicks ticks) {
  DLOG(INFO) << "network state: " << state_name;

  NetworkState state = LookupNetworkState(state_name);

  // Logs the time in seconds between the network going online to
  // going offline (or, more precisely, going not online) in order to
  // measure the mean time to network dropping. Going offline as part
  // of suspend-to-RAM is not logged as network drop -- the assumption
  // is that the message for suspend-to-RAM comes before the network
  // offline message which seems to and should be the case.
  if (state != kNetworkStateOnline &&
      network_state_ == kNetworkStateOnline &&
      power_state_ != kPowerStateMem) {
    TimeDelta since_online = ticks - network_state_last_;
    int online_time = static_cast<int>(since_online.InSeconds());
    SendMetric(kMetricTimeToNetworkDropName, online_time,
               kMetricTimeToNetworkDropMin,
               kMetricTimeToNetworkDropMax,
               kMetricTimeToNetworkDropBuckets);
  }

  network_state_ = state;
  network_state_last_ = ticks;
}

MetricsDaemon::NetworkState
MetricsDaemon::LookupNetworkState(const char* state_name) {
  for (int i = 0; i < kNumberNetworkStates; i++) {
    if (strcmp(state_name, kNetworkStates_[i]) == 0) {
      return static_cast<NetworkState>(i);
    }
  }
  DLOG(WARNING) << "unknown network connection state: " << state_name;
  return kUnknownNetworkState;
}

void MetricsDaemon::PowerStateChanged(const char* state_name, Time now) {
  DLOG(INFO) << "power state: " << state_name;
  power_state_ = LookupPowerState(state_name);

  if (power_state_ != kPowerStateOn)
    SetUserActiveState(false, now);
}

MetricsDaemon::PowerState
MetricsDaemon::LookupPowerState(const char* state_name) {
  for (int i = 0; i < kNumberPowerStates; i++) {
    if (strcmp(state_name, kPowerStates_[i]) == 0) {
      return static_cast<PowerState>(i);
    }
  }
  DLOG(WARNING) << "unknown power state: " << state_name;
  return kUnknownPowerState;
}

void MetricsDaemon::SessionStateChanged(const char* state_name, Time now) {
  DLOG(INFO) << "user session state: " << state_name;
  session_state_ = LookupSessionState(state_name);
  SetUserActiveState(session_state_ == kSessionStateStarted, now);
}

MetricsDaemon::SessionState
MetricsDaemon::LookupSessionState(const char* state_name) {
  for (int i = 0; i < kNumberSessionStates; i++) {
    if (strcmp(state_name, kSessionStates_[i]) == 0) {
      return static_cast<SessionState>(i);
    }
  }
  DLOG(WARNING) << "unknown user session state: " << state_name;
  return kUnknownSessionState;
}

void MetricsDaemon::SetUserActiveState(bool active, Time now) {
  DLOG(INFO) << "user: " << (active ? "active" : "inactive");

  // Calculates the seconds of active use since the last update and
  // the day since Epoch, and logs the usage data.  Guards against the
  // time jumping back and forth due to the user changing it by
  // discarding the new use time.
  int seconds = 0;
  if (user_active_ && now > user_active_last_) {
    TimeDelta since_active = now - user_active_last_;
    if (since_active < TimeDelta::FromSeconds(
            kUseMonitorIntervalMax + kSecondsPerMinute)) {
      seconds = static_cast<int>(since_active.InSeconds());
    }
  }
  TimeDelta since_epoch = now - Time();
  int day = since_epoch.InDays();
  daily_use_->Update(day, seconds);
  user_crash_interval_->Update(0, seconds);
  kernel_crash_interval_->Update(0, seconds);

  // Schedules a use monitor on inactive->active transitions and
  // unschedules it on active->inactive transitions.
  if (!user_active_ && active)
    ScheduleUseMonitor(kUseMonitorIntervalInit, /* backoff */ false);
  else if (user_active_ && !active)
    UnscheduleUseMonitor();

  // Remembers the current active state and the time of the last
  // activity update.
  user_active_ = active;
  user_active_last_ = now;
}

void MetricsDaemon::ProcessUserCrash() {
  // Counts the active use time up to now.
  SetUserActiveState(user_active_, Time::Now());

  // Reports the active use time since the last crash and resets it.
  user_crash_interval_->Flush();

  user_crashes_daily_->Update(1);
  any_crashes_daily_->Update(1);
}

void MetricsDaemon::ProcessKernelCrash() {
  // Counts the active use time up to now.
  SetUserActiveState(user_active_, Time::Now());

  // Reports the active use time since the last crash and resets it.
  kernel_crash_interval_->Flush();

  kernel_crashes_daily_->Update(1);
  any_crashes_daily_->Update(1);
}

void MetricsDaemon::ProcessUncleanShutdown() {
  // Counts the active use time up to now.
  SetUserActiveState(user_active_, Time::Now());

  // Reports the active use time since the last crash and resets it.
  unclean_shutdown_interval_->Flush();

  unclean_shutdowns_daily_->Update(1);
  any_crashes_daily_->Update(1);
}

bool MetricsDaemon::CheckSystemCrash(const std::string& crash_file) {
  FilePath crash_detected(crash_file);
  if (!file_util::PathExists(crash_detected))
    return false;

  // Deletes the crash-detected file so that the daemon doesn't report
  // another kernel crash in case it's restarted.
  file_util::Delete(crash_detected,
                    false);  // recursive
  return true;
}

// static
gboolean MetricsDaemon::UseMonitorStatic(gpointer data) {
  return static_cast<MetricsDaemon*>(data)->UseMonitor() ? TRUE : FALSE;
}

bool MetricsDaemon::UseMonitor() {
  SetUserActiveState(user_active_, Time::Now());

  // If a new monitor source/instance is scheduled, returns false to
  // tell GLib to destroy this monitor source/instance. Returns true
  // otherwise to keep calling back this monitor.
  return !ScheduleUseMonitor(usemon_interval_ * 2, /* backoff */ true);
}

bool MetricsDaemon::ScheduleUseMonitor(int interval, bool backoff)
{
  if (testing_)
    return false;

  // Caps the interval -- the bigger the interval, the more active use
  // time will be potentially dropped on system shutdown.
  if (interval > kUseMonitorIntervalMax)
    interval = kUseMonitorIntervalMax;

  if (backoff) {
    // Back-off mode is used by the use monitor to reschedule itself
    // with exponential back-off in time. This mode doesn't create a
    // new timeout source if the new interval is the same as the old
    // one. Also, if a new timeout source is created, the old one is
    // not destroyed explicitly here -- it will be destroyed by GLib
    // when the monitor returns FALSE (see UseMonitor and
    // UseMonitorStatic).
    if (interval == usemon_interval_)
      return false;
  } else {
    UnscheduleUseMonitor();
  }

  // Schedules a new use monitor for |interval| seconds from now.
  DLOG(INFO) << "scheduling use monitor in " << interval << " seconds";
  usemon_source_ = g_timeout_source_new_seconds(interval);
  g_source_set_callback(usemon_source_, UseMonitorStatic, this,
                        NULL); // No destroy notification.
  g_source_attach(usemon_source_,
                  NULL); // Default context.
  usemon_interval_ = interval;
  return true;
}

void MetricsDaemon::UnscheduleUseMonitor() {
  // If there's a use monitor scheduled already, destroys it.
  if (usemon_source_ == NULL)
    return;

  DLOG(INFO) << "destroying use monitor";
  g_source_destroy(usemon_source_);
  usemon_source_ = NULL;
  usemon_interval_ = 0;
}

// static
void MetricsDaemon::ReportDailyUse(void* handle, int tag, int count) {
  if (count <= 0)
    return;

  MetricsDaemon* daemon = static_cast<MetricsDaemon*>(handle);
  int minutes = (count + kSecondsPerMinute / 2) / kSecondsPerMinute;
  daemon->SendMetric(kMetricDailyUseTimeName, minutes,
                     kMetricDailyUseTimeMin,
                     kMetricDailyUseTimeMax,
                     kMetricDailyUseTimeBuckets);
}

// static
void MetricsDaemon::ReportCrashInterval(const char* histogram_name,
                                        void* handle, int count) {
  MetricsDaemon* daemon = static_cast<MetricsDaemon*>(handle);
  daemon->SendMetric(histogram_name, count,
                     kMetricCrashIntervalMin,
                     kMetricCrashIntervalMax,
                     kMetricCrashIntervalBuckets);
}

// static
void MetricsDaemon::ReportUserCrashInterval(void* handle,
                                            int tag, int count) {
  ReportCrashInterval(kMetricUserCrashIntervalName, handle, count);
}

// static
void MetricsDaemon::ReportKernelCrashInterval(void* handle,
                                                int tag, int count) {
  ReportCrashInterval(kMetricKernelCrashIntervalName, handle, count);
}

// static
void MetricsDaemon::ReportUncleanShutdownInterval(void* handle,
                                                    int tag, int count) {
  ReportCrashInterval(kMetricUncleanShutdownIntervalName, handle, count);
}

// static
void MetricsDaemon::ReportCrashesDailyFrequency(const char* histogram_name,
                                                void* handle,
                                                int count) {
  MetricsDaemon* daemon = static_cast<MetricsDaemon*>(handle);
  daemon->SendMetric(histogram_name, count,
                     kMetricCrashesDailyMin,
                     kMetricCrashesDailyMax,
                     kMetricCrashesDailyBuckets);
}

// static
void MetricsDaemon::ReportUserCrashesDaily(void* handle,
                                           int tag, int count) {
  ReportCrashesDailyFrequency(kMetricUserCrashesDailyName, handle, count);
}

// static
void MetricsDaemon::ReportKernelCrashesDaily(void* handle,
                                             int tag, int count) {
  ReportCrashesDailyFrequency(kMetricKernelCrashesDailyName, handle, count);
}

// static
void MetricsDaemon::ReportUncleanShutdownsDaily(void* handle,
                                                int tag, int count) {
  ReportCrashesDailyFrequency(kMetricUncleanShutdownsDailyName, handle, count);
}

// static
void MetricsDaemon::ReportAnyCrashesDaily(void* handle, int tag, int count) {
  ReportCrashesDailyFrequency(kMetricAnyCrashesDailyName, handle, count);
}


void MetricsDaemon::SendMetric(const string& name, int sample,
                               int min, int max, int nbuckets) {
  DLOG(INFO) << "received metric: " << name << " " << sample << " "
             << min << " " << max << " " << nbuckets;
  metrics_lib_->SendToUMA(name, sample, min, max, nbuckets);
}
