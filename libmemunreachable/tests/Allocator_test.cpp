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

#include <Allocator.h>
#include <sys/time.h>

#include <chrono>
#include <functional>
#include <list>
#include <vector>

#include <gtest/gtest.h>
#include <ScopedDisableMalloc.h>


std::function<void()> ScopedAlarm::func_;

using namespace std::chrono_literals;

class AllocatorTest : public testing::Test {
 protected:
  AllocatorTest() : heap(), disable_malloc_() {}
  virtual void SetUp() {
    heap_count = 0;
  }
  virtual void TearDown() {
    ASSERT_EQ(heap_count, 0);
    ASSERT_TRUE(heap.empty());
    ASSERT_FALSE(disable_malloc_.timed_out());
  }
  Heap heap;
 private:
  ScopedDisableMallocTimeout disable_malloc_;
};

TEST_F(AllocatorTest, simple) {
  Allocator<char[100]> allocator(heap);
  void *ptr = allocator.allocate();
  ASSERT_TRUE(ptr != NULL);
  allocator.deallocate(ptr);
}

TEST_F(AllocatorTest, multiple) {
  Allocator<char[100]> allocator(heap);
  void *ptr1 = allocator.allocate();
  ASSERT_TRUE(ptr1 != NULL);
  void *ptr2 = allocator.allocate();
  ASSERT_TRUE(ptr2 != NULL);
  ASSERT_NE(ptr1, ptr2);
  allocator.deallocate(ptr1);
  void *ptr3 = allocator.allocate();
  ASSERT_EQ(ptr1, ptr3);
  allocator.deallocate(ptr3);
  allocator.deallocate(ptr2);
}

TEST_F(AllocatorTest, many) {
  const int num = 4096;
  const int size = 128;
  Allocator<char[size]> allocator(heap);
  void *ptr[num];
  for (int i = 0; i < num; i++) {
    ptr[i] = allocator.allocate();
    memset(ptr[i], 0xaa, size);
    *(reinterpret_cast<unsigned char*>(ptr[i])) = i;
  }

  for (int i = 0; i < num; i++) {
    for (int j = 0; j < num; j++) {
      if (i != j) {
        ASSERT_NE(ptr[i], ptr[j]);
      }
    }
  }

  for (int i = 0; i < num; i++) {
    ASSERT_EQ(*(reinterpret_cast<unsigned char*>(ptr[i])), i & 0xFF);
    allocator.deallocate(ptr[i]);
  }
}

TEST_F(AllocatorTest, large) {
  const size_t size = 1024 * 1024;
  Allocator<char[size]> allocator(heap);
  void *ptr = allocator.allocate();
  memset(ptr, 0xaa, size);
  allocator.deallocate(ptr);
}

TEST_F(AllocatorTest, many_large) {
  const int num = 128;
  const int size = 1024 * 1024;
  Allocator<char[size]> allocator(heap);
  void *ptr[num];
  for (int i = 0; i < num; i++) {
    ptr[i] = allocator.allocate();
    memset(ptr[i], 0xaa, size);
    *(reinterpret_cast<unsigned char*>(ptr[i])) = i;
  }

  for (int i = 0; i < num; i++) {
    ASSERT_EQ(*(reinterpret_cast<unsigned char*>(ptr[i])), i & 0xFF);
    allocator.deallocate(ptr[i]);
  }
}

TEST_F(AllocatorTest, copy) {
  Allocator<char[100]> a(heap);
  Allocator<char[200]> b = a;
  Allocator<char[300]> c(b);
  Allocator<char[100]> d(a);
  Allocator<char[100]> e(heap);

  ASSERT_EQ(a, b);
  ASSERT_EQ(a, c);
  ASSERT_EQ(a, d);
  ASSERT_EQ(a, e);

  void* ptr1 = a.allocate();
  void* ptr2 = b.allocate();
  void* ptr3 = c.allocate();
  void* ptr4 = d.allocate();

  b.deallocate(ptr1);
  d.deallocate(ptr2);
  a.deallocate(ptr3);
  c.deallocate(ptr4);
}

TEST_F(AllocatorTest, stl_vector) {
  auto v = allocator::vector<int>(Allocator<int>(heap));
  for (int i = 0; i < 1024; i++) {
    v.push_back(i);
  }
  for (int i = 0; i < 1024; i++) {
    ASSERT_EQ(v[i], i);
  }
  v.clear();
}

TEST_F(AllocatorTest, stl_list) {
  auto v = allocator::list<int>(Allocator<int>(heap));
  for (int i = 0; i < 1024; i++) {
    v.push_back(i);
  }
  int i = 0;
  for (auto iter = v.begin(); iter != v.end(); iter++, i++) {
    ASSERT_EQ(*iter, i);
  }
  v.clear();
}

TEST_F(AllocatorTest, shared) {
  Allocator<int> allocator(heap);

  Allocator<int>::shared_ptr ptr = allocator.make_shared(0);
  {
    auto ptr2 = ptr;
  }
  ASSERT_NE(ptr, nullptr);
}

TEST_F(AllocatorTest, unique) {
  Allocator<int> allocator(heap);

  Allocator<int>::unique_ptr ptr = allocator.make_unique(0);

  ASSERT_NE(ptr, nullptr);
}

class DisableMallocTest : public ::testing::Test {
 protected:
  void alarm(std::chrono::microseconds us) {
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(us);
    itimerval t = itimerval();
    t.it_value.tv_sec = s.count();
    t.it_value.tv_usec = (us - s).count();
    setitimer(ITIMER_REAL, &t, NULL);
  }
};

TEST_F(DisableMallocTest, reenable) {
  ASSERT_EXIT({
    alarm(100ms);
    void *ptr1 = malloc(128);
    ASSERT_NE(ptr1, nullptr);
    free(ptr1);
    {
      ScopedDisableMalloc disable_malloc;
    }
    void *ptr2 = malloc(128);
    ASSERT_NE(ptr2, nullptr);
    free(ptr2);
    _exit(1);
  }, ::testing::ExitedWithCode(1), "");
}

TEST_F(DisableMallocTest, deadlock_allocate) {
  ASSERT_DEATH({
    void *ptr = malloc(128);
    ASSERT_NE(ptr, nullptr);
    free(ptr);
    {
      alarm(100ms);
      ScopedDisableMalloc disable_malloc;
      void* ptr = malloc(128);
      ASSERT_NE(ptr, nullptr);
      free(ptr);
    }
  }, "");
}

TEST_F(DisableMallocTest, deadlock_new) {
  ASSERT_DEATH({
    char* ptr = new(char);
    ASSERT_NE(ptr, nullptr);
    delete(ptr);
    {
      alarm(100ms);
      ScopedDisableMalloc disable_malloc;
      char* ptr = new(char);
      ASSERT_NE(ptr, nullptr);
      delete(ptr);
    }
  }, "");
}

TEST_F(DisableMallocTest, deadlock_delete) {
  ASSERT_DEATH({
    char* ptr = new(char);
    ASSERT_NE(ptr, nullptr);
    {
      alarm(250ms);
      ScopedDisableMalloc disable_malloc;
      delete(ptr);
    }
  }, "");
}

TEST_F(DisableMallocTest, deadlock_free) {
  ASSERT_DEATH({
    void *ptr = malloc(128);
    ASSERT_NE(ptr, nullptr);
    {
      alarm(100ms);
      ScopedDisableMalloc disable_malloc;
      free(ptr);
    }
  }, "");
}

TEST_F(DisableMallocTest, deadlock_fork) {
  ASSERT_DEATH({
    {
      alarm(100ms);
      ScopedDisableMalloc disable_malloc;
      fork();
    }
  }, "");
}
