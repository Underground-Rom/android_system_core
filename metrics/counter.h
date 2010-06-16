// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRICS_COUNTER_H_
#define METRICS_COUNTER_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace chromeos_metrics {

// TaggedCounter maintains a persistent storage (i.e., a file)
// aggregation counter for a given tag (e.g., day, hour) that survives
// system shutdowns, reboots and crashes, as well as daemon process
// restarts. The counter object is initialized by pointing to the
// persistent storage file and providing a callback used for reporting
// aggregated data.  The counter can then be updated with additional
// event counts.  The aggregated count is reported through the
// callback when the counter is explicitly flushed or when data for a
// new tag arrives.
class TaggedCounterInterface {
 public:
  // Callback type used for reporting aggregated or flushed data.
  // Once this callback is invoked by the counter, the reported
  // aggregated data is discarded.
  //
  // |handle| is the |reporter_handle| pointer passed through Init.
  // |tag| is the tag associated with the aggregated count.
  // |count| is aggregated count.
  typedef void (*Reporter)(void* handle, int tag, int count);

  virtual ~TaggedCounterInterface() {}

  // Initializes the counter by providing the persistent storage
  // location |filename| and a |reporter| callback for reporting
  // aggregated counts. |reporter_handle| is sent to the |reporter|
  // along with the aggregated counts.
  //
  // NOTE: The assumption is that this object is the sole owner of the
  // persistent storage file so no locking is currently implemented.
  virtual void Init(const char* filename,
                    Reporter reporter, void* reporter_handle) = 0;

  // Adds |count| of events for the given |tag|. If there's an
  // existing aggregated count for a different tag, it's reported
  // through the reporter callback and discarded.
  virtual void Update(int tag, int count) = 0;

  // Reports the current aggregated count (if any) through the
  // reporter callback and discards it.
  virtual void Flush() = 0;
};

class TaggedCounter : public TaggedCounterInterface {
 public:
  TaggedCounter();
  ~TaggedCounter();

  // Implementation of interface methods.
  void Init(const char* filename, Reporter reporter, void* reporter_handle);
  void Update(int tag, int count);
  void Flush();

 private:
  friend class RecordTest;
  friend class TaggedCounterTest;
  FRIEND_TEST(TaggedCounterTest, BadFileLocation);
  FRIEND_TEST(TaggedCounterTest, Flush);
  FRIEND_TEST(TaggedCounterTest, InitFromFile);
  FRIEND_TEST(TaggedCounterTest, Update);

  // The current tag/count record is cached by the counter object to
  // avoid potentially unnecessary I/O. The cached record can be in
  // one of the following states:
  enum RecordState {
    kRecordInvalid,    // Invalid record, sync from persistent storage needed.
    kRecordNull,       // No current record, persistent storage synced.
    kRecordNullDirty,  // No current record, persistent storage is invalid.
    kRecordValid,      // Current record valid, persistent storage synced.
    kRecordValidDirty  // Current record valid, persistent storage is invalid.
  };

  // Defines the tag/count record. Objects of this class are synced
  // with the persistent storage through binary reads/writes.
  class Record {
   public:
    // Creates a new Record with |tag_| and |count_| reset to 0.
    Record() : tag_(0), count_(0) {}

    // Initializes with |tag| and |count|. If |count| is negative,
    // |count_| is set to 0.
    void Init(int tag, int count);

    // Adds |count| to the current |count_|. Negative |count| is
    // ignored. In case of positive overflow, |count_| is saturated to
    // INT_MAX.
    void Add(int count);

    int tag() const { return tag_; }
    int count() const { return count_; }

   private:
    int tag_;
    int count_;
  };

  // Implementation of the Update and Flush methods. Goes through the
  // necessary steps to read, report, update, and sync the aggregated
  // record.
  void UpdateInternal(int tag, int count, bool flush);

  // If the current cached record is invalid, reads it from persistent
  // storage specified through file descriptor |fd| and updates the
  // cached record state to either null, or valid depending on the
  // persistent storage contents.
  void ReadRecord(int fd);

  // If there's an existing valid record and either |flush| is true,
  // or the new |tag| is different than the old one, reports the
  // aggregated data through the reporter callback and resets the
  // cached record.
  void ReportRecord(int tag, bool flush);

  // Updates the cached record given the new |tag| and |count|. This
  // method expects either a null cached record, or a valid cached
  // record with the same tag as |tag|. If |flush| is true, the method
  // asserts that the cached record is null and returns.
  void UpdateRecord(int tag, int count, bool flush);

  // If the cached record state is dirty, updates the persistent
  // storage specified through file descriptor |fd| and switches the
  // record state to non-dirty.
  void WriteRecord(int fd);

  // Persistent storage file path.
  const char* filename_;

  // Aggregated data reporter callback and handle to pass-through.
  Reporter reporter_;
  void* reporter_handle_;

  // Current cached aggregation record.
  Record record_;

  // Current cached aggregation record state.
  RecordState record_state_;
};

}  // namespace chromeos_metrics

#endif  // METRICS_COUNTER_H_
