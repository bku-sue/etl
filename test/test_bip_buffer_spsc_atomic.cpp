/******************************************************************************
The MIT License(MIT)

Embedded Template Library.
https://github.com/ETLCPP/etl
https://www.etlcpp.com

Copyright(c) 2021 jwellbelove

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
******************************************************************************/

#include "unit_test_framework.h"

#include <thread>
#include <chrono>
#include <vector>

#include "etl/bip_buffer_spsc_atomic.h"

#include "data.h"

#if ETL_HAS_ATOMIC

#if defined(ETL_TARGET_OS_WINDOWS)
  #include <Windows.h>
#endif

#define REALTIME_TEST 0

namespace
{
  struct Data
  {
    Data(int a_, int b_ = 2, int c_ = 3, int d_ = 4)
      : a(a_),
      b(b_),
      c(c_),
      d(d_)
    {
    }

    Data()
      : a(0),
      b(0),
      c(0),
      d(0)
    {
    }

    int a;
    int b;
    int c;
    int d;
  };

  bool operator ==(const Data& lhs, const Data& rhs)
  {
    return (lhs.a == rhs.a) && (lhs.b == rhs.b) && (lhs.c == rhs.c) && (lhs.d == rhs.d);
  }

  using ItemM = TestDataM<int>;

  SUITE(test_bip_buffer_spsc_atomic)
  {
    //*************************************************************************
    TEST(test_constructor)
    {
      etl::bip_buffer_spsc_atomic<int, 5> stream;

      CHECK_EQUAL(5U, stream.max_size());
      CHECK_EQUAL(5U, stream.capacity());
    }

    //*************************************************************************
    TEST(test_size_write_read)
    {
      etl::bip_buffer_spsc_atomic<int, 5> stream;
      etl::ibip_buffer_spsc_atomic<int>& istream = stream;

      // Verify empty buffer
      CHECK_EQUAL(0U, stream.size());
      CHECK(stream.empty());
      CHECK((stream.max_size() / 2U) <= stream.available());
      CHECK(stream.available() <= stream.max_size());

      auto reader = istream.read_reserve(1U);
      CHECK_EQUAL(0U, reader.size());

      // Write partially
      auto writer = istream.write_reserve(1U);
      CHECK_EQUAL(1U, writer.size());
      writer[0] = 1;

      CHECK(stream.empty());

      istream.write_commit(writer); // 1 * * * *
      CHECK_EQUAL(1U, stream.size());

      writer = istream.write_reserve(1U);
      CHECK_EQUAL(1U, writer.size());
      writer[0] = 2;

      istream.write_commit(writer); // 1 2 * * *
      CHECK_EQUAL(2U, stream.size());

      // Write to capacity
      writer = istream.write_reserve(istream.available());
      CHECK_EQUAL(3U, writer.size());
      writer[0] = 3;
      writer[1] = 4;
      writer[2] = 5;

      istream.write_commit(writer); // 1 2 3 4 5

      // Verify full buffer
      CHECK_EQUAL(0U, stream.available());
      CHECK(stream.full());
      CHECK((stream.max_size() - 1) <= stream.size());
      CHECK(stream.size() <= stream.max_size());

      writer = istream.write_reserve(1U);
      CHECK_EQUAL(0U, writer.size());

      // Read partially
      reader = istream.read_reserve(1U);
      CHECK_EQUAL(1U, reader.size());
      CHECK_EQUAL(1, reader[0]);
      CHECK_EQUAL(5U, stream.size());

      istream.read_commit(reader); // * 2 3 4 5
      CHECK_EQUAL(4U, stream.size());

      reader = istream.read_reserve(1U);
      CHECK_EQUAL(1U, reader.size());
      CHECK_EQUAL(2, reader[0]);
      CHECK_EQUAL(4U, stream.size());

      istream.read_commit(reader); // * * 3 4 5
      CHECK_EQUAL(3U, stream.size());

      // Write to wraparound area (one element remains reserved)
      writer = istream.write_reserve(istream.available());
      CHECK_EQUAL(1U, writer.size());
      CHECK_EQUAL(1U, stream.available());
      writer[0] = 6;

      istream.write_commit(writer); // 6 * 3 4 5

      // Verify full buffer
      CHECK_EQUAL(0U, stream.available());
      CHECK(stream.full());
      CHECK((stream.max_size() - 1) <= stream.size());
      CHECK(stream.size() <= stream.max_size());

      writer = istream.write_reserve(1U);
      CHECK_EQUAL(0U, writer.size());

      // Read to capacity
      reader = istream.read_reserve(istream.size());
      CHECK_EQUAL(3U, reader.size());
      CHECK_EQUAL(3, reader[0]);
      CHECK_EQUAL(4, reader[1]);
      CHECK_EQUAL(5, reader[2]);
      CHECK_EQUAL(4U, stream.size());

      istream.read_commit(reader); // * 2 * * *
      CHECK_EQUAL(1U, stream.size());

      reader = istream.read_reserve(istream.size());
      CHECK_EQUAL(1U, reader.size());
      CHECK_EQUAL(6, reader[0]);
      CHECK_EQUAL(1U, stream.size());

      istream.read_commit(reader); // * * * * *

      // Verify empty buffer
      CHECK_EQUAL(0U, stream.size());
      CHECK(stream.empty());
      CHECK((stream.max_size() / 2U) <= stream.available());
      CHECK(stream.available() <= stream.max_size());
    }
#if 0

    //*************************************************************************
    TEST(test_size_push_pop_iqueue)
    {
      etl::bip_buffer_spsc_atomic<int, 4> stream;

      etl::ibip_buffer_spsc_atomic<int>& istream = stream;

      CHECK_EQUAL(0U, istream.size());

      istream.push(1);
      CHECK_EQUAL(1U, istream.size());

      istream.push(2);
      CHECK_EQUAL(2U, istream.size());

      istream.push(3);
      CHECK_EQUAL(3U, istream.size());

      istream.push(4);
      CHECK_EQUAL(4U, istream.size());

      CHECK(!istream.push(5));
      CHECK(!istream.push(5));

      int i;

      CHECK(istream.pop(i));
      CHECK_EQUAL(1, i);
      CHECK_EQUAL(3U, istream.size());

      CHECK(istream.pop(i));
      CHECK_EQUAL(2, i);
      CHECK_EQUAL(2U, istream.size());

      CHECK(istream.pop(i));
      CHECK_EQUAL(3, i);
      CHECK_EQUAL(1U, istream.size());

      CHECK(istream.pop(i));
      CHECK_EQUAL(4, i);
      CHECK_EQUAL(0U, istream.size());

      CHECK(!istream.pop(i));
      CHECK(!istream.pop(i));
    }

    //*************************************************************************
    TEST(test_clear)
    {
      etl::bip_buffer_spsc_atomic<int, 4> stream;

      CHECK_EQUAL(0U, stream.size());

      stream.push(1);
      stream.push(2);
      stream.clear();
      CHECK_EQUAL(0U, stream.size());

      // Do it again to check that clear() didn't screw up the internals.
      stream.push(1);
      stream.push(2);
      CHECK_EQUAL(2U, stream.size());
      stream.clear();
      CHECK_EQUAL(0U, stream.size());
    }

    //*************************************************************************
    TEST(test_empty)
    {
      etl::bip_buffer_spsc_atomic<int, 4> stream;
      CHECK(stream.empty());

      stream.push(1);
      CHECK(!stream.empty());

      stream.clear();
      CHECK(stream.empty());

      stream.push(1);
      CHECK(!stream.empty());
    }

    //*************************************************************************
    TEST(test_full)
    {
      etl::bip_buffer_spsc_atomic<int, 4> stream;
      CHECK(!stream.full());

      stream.push(1);
      stream.push(2);
      stream.push(3);
      stream.push(4);
      CHECK(stream.full());

      stream.clear();
      CHECK(!stream.full());

      stream.push(1);
      stream.push(2);
      stream.push(3);
      stream.push(4);
      CHECK(stream.full());
    }

    //*************************************************************************
#if REALTIME_TEST && defined(ETL_COMPILER_MICROSOFT)
    #if defined(ETL_TARGET_OS_WINDOWS) // Only Windows priority is currently supported
      #define FIX_PROCESSOR_AFFINITY1 SetThreadAffinityMask(GetCurrentThread(), 1);
      #define FIX_PROCESSOR_AFFINITY2 SetThreadAffinityMask(GetCurrentThread(), 2);
    #else
      #error No thread priority modifier defined
    #endif

    size_t ticks = 0;

    etl::bip_buffer_spsc_atomic<int, 10> stream;

    const size_t LENGTH = 1000000;

    void timer_event()
    {
      FIX_PROCESSOR_AFFINITY1;

      const size_t TICK = 1;
      size_t tick = TICK;
      ticks = 1;

      while (ticks <= LENGTH)
      {
        if (stream.push(ticks))
        {
          ++ticks;
        }
      }
    }

    TEST(queue_threads)
    {
      FIX_PROCESSOR_AFFINITY2;

      std::vector<int> tick_list;
      tick_list.reserve(LENGTH);

      std::thread t1(timer_event);

      while (tick_list.size() < LENGTH)
      {
        int i;

        if (stream.pop(i))
        {
          tick_list.push_back(i);
        }
      }

      // Join the thread with the main thread
      t1.join();

      CHECK_EQUAL(LENGTH, tick_list.size());

      for (size_t i = 0; i < LENGTH; ++i)
      {
        CHECK_EQUAL(i + 1, tick_list[i]);
      }
    }
#endif
#endif
  };
}

#endif
