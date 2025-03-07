

/*
   Copyright The Overlaybd Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

#define protected public
#define private public


#include "../throttled-file.h"
#include "../throttled-file.cpp"
#include "../../alog.h"
#include "../filesystem.h"
#include "../../photon/thread11.h"
#include "../../photon/thread.h"

#include "mock.h"

using namespace FileSystem;

void push_in_another_thread(StatisticsQueue &queue) {
    photon::thread_sleep(1);
    queue.push_back(64);
    LOG_DEBUG("PUSHED 64, sum is `", VALUE(queue.sum()));
}

void pop_in_another_thread(StatisticsQueue &queue) {
    photon::thread_sleep(2);
    queue.try_pop(); //it should pop out two items.
    LOG_DEBUG("POPED");
}

TEST(ThrottledFile, statistics_queue) {
    using namespace FileSystem;
    photon::init();
    StatisticsQueue queue(50*4096, 128);
    EXPECT_EQ(0UL, queue.sum());

    queue.push_back(32);
    EXPECT_EQ(32U, queue.back().amount);
    EXPECT_EQ(32UL, queue.sum());
    queue.push_back(64);
    EXPECT_EQ(96UL, queue.sum());
    EXPECT_EQ(0UL, queue.min_duration());
    photon::thread_sleep(2);
    queue.try_pop();
    EXPECT_EQ(0UL, queue.sum());
    for (int i=0;i<51;i++) {
        queue.push_back(4096);
    } //4096*100 400k

    EXPECT_EQ(4096*50UL, queue.limit());
    EXPECT_EQ(4096*50U, queue.rate());
}

TEST(ThrottledFile, scoped_queue) {
    using namespace FileSystem;
    photon::init();
    StatisticsQueue queue(4096, 128);

    auto start = photon::now;
    queue.push_back(4096);
    {
        scoped_queue sq(queue, 4096);
        EXPECT_EQ(4096UL, queue.sum());
    }
    EXPECT_GT(photon::now, start + 1000*1000);
    photon::thread_sleep(2);
    queue.try_pop();
    EXPECT_EQ(0UL, queue.sum());
}

TEST(ThrottledFile, scoped_semaphore) {
    using namespace FileSystem;
    photon::semaphore sem(8);
    {
        scoped_semaphore ss(sem, 5);
        EXPECT_EQ(3UL, sem.m_count);
    }
    EXPECT_EQ(8UL, sem.m_count);
}

TEST(ThrottledFile, split_iovector_view) {
    using namespace FileSystem;
    iovec iov[10];
    for (int i=0;i < 10; i++) {
        iov[i].iov_base = (void*)(i * 4096UL);
        iov[i].iov_len = 4096;
    }
    split_iovector_view siv(iov, 10, 1024); // should become 40 blocks;
    for (int i=0;i<40;i++) {
        iovec *v = siv.iov;
        EXPECT_EQ(1024UL, v->iov_len);
        EXPECT_EQ(i * 1024UL, (uint64_t)(v->iov_base));
        siv.next();
    }
}

ssize_t count_iov_size(const iovec* v, int n) {
    ssize_t ret = 0;
    for (auto p = v; p != v+n; p++) {
        ret += p->iov_len;
    }
    return ret;
}

TEST(ThrottledFile, basic_throttled) {
    using namespace testing;
    ThrottleLimits limit;
    limit.R.block_size = 1024;
    limit.R.concurent_ops = 1024;
    limit.R.IOPS = 1024*128;
    limit.R.throughput = 4*1024*1024;
    limit.W = limit.R;
    limit.RW = limit.R;
    Mock::MockNullFile mock;
    IFile * pmock = &mock;
    IFile * tf = new_throttled_file(pmock, limit);
    iovec iov[10];
    for (int i=0;i < 10; i++) {
        iov[i].iov_base = (void*)(i * 4000UL);
        iov[i].iov_len = 4000;
    }
    const struct iovec* c_nulliov = iov;
    struct iovec* nulliov = iov;
    char buff[4096];
    EXPECT_CALL(mock, pread(_, _, _)).Times(AtLeast(1)).WillRepeatedly(ReturnArg<1>());
    EXPECT_CALL(mock, preadv(_, _, _)).Times(AtLeast(1)).WillRepeatedly(WithArgs<0, 1>(Invoke(count_iov_size)));
    EXPECT_CALL(mock, pwrite(_, _, _)).Times(AtLeast(1)).WillRepeatedly(ReturnArg<1>());
    EXPECT_CALL(mock, pwritev(_, _, _)).Times(AtLeast(1)).WillRepeatedly(WithArgs<0, 1>(Invoke(count_iov_size)));
    EXPECT_CALL(mock, read(_, _)).Times(AtLeast(1)).WillRepeatedly(ReturnArg<1>());
    EXPECT_CALL(mock, readv(_, _)).Times(AtLeast(1)).WillRepeatedly(Invoke(count_iov_size));
    EXPECT_CALL(mock, write(_, _)).Times(AtLeast(1)).WillRepeatedly(ReturnArg<1>());
    EXPECT_CALL(mock, writev(_, _)).Times(AtLeast(1)).WillRepeatedly(Invoke(count_iov_size));
    tf->pread(nullptr, 0, 0);
    tf->pread(buff, 4096, 0);
    tf->preadv(nulliov, 10, 0);
    tf->preadv(c_nulliov, 10, 0);
    tf->pwrite(nullptr, 0, 0);
    tf->pwrite(buff, 4096, 0);
    tf->pwritev(nulliov, 10, 0);
    tf->pwritev(c_nulliov, 10, 0);
    tf->read(nullptr, 0);
    tf->read(buff, 4096);
    tf->readv(nulliov, 10);
    tf->readv(c_nulliov, 10);
    tf->write(nullptr, 0);
    tf->write(buff, 4096);
    tf->writev(nulliov, 10);
    tf->writev(c_nulliov, 10);
    delete tf;
}

TEST(ThrottledFile, huge_enque) {
    using namespace FileSystem;
    StatisticsQueue q(1024, 128);
    q.push_back(4096);
    EXPECT_EQ(3UL*1024*1024, q.min_duration());
}

void enq_thread(StatisticsQueue &q) {
    scoped_queue(q, 4096);
}

TEST(ThrottledFile, huge_scope_que) {
    using namespace FileSystem;
    StatisticsQueue q(1024, 128);
    auto start = photon::now;
    photon::thread_create11(enq_thread, q);
    do { photon::thread_yield_to(nullptr); q.try_pop(); } while (q.sum()>0);
    EXPECT_EQ(0UL, q.sum());
    EXPECT_GE(photon::now - start, 4UL*1000*1000);
    EXPECT_LE(photon::now - start, 5UL*1000*1000);
}

TEST(ThrottledFile, split_io) {
    using namespace FileSystem;
    auto _o_output = log_output;
    log_output = log_output_null;
    DEFER({ log_output = _o_output; });
    auto ret = split_io("testio", 2000, 1024,
        [](size_t len){
            if (len == 1024) return 1024;
            return 11;
        },
        [](size_t len){
        }
    );
    EXPECT_EQ(1024+11, ret);
    ret = split_io("testio", 2000, 1024,
        [](size_t len){
            if (len == 1024) return 1024;
            return -1;
        },
        [](size_t len){
        }
    );
    EXPECT_EQ(-1, ret);
}

void large_pulse_write(IFile* tf, uint64_t slt) {
    photon::thread_usleep(slt+100); // skip time_window
    tf->write(nullptr, 2048); //twice of time_window
}

TEST(ThrottledFile, large_pulse) {
    using namespace FileSystem;
    using namespace testing;
    ThrottleLimits limit;
    limit.RW.block_size = 1024;
    limit.RW.concurent_ops = 1024;
    limit.RW.IOPS = 1024;
    limit.RW.throughput = 1024;
    limit.R = limit.RW;
    limit.W = limit.RW;
    Mock::MockNullFile *mock = new Mock::MockNullFile();
    DEFER({ delete mock; });
    EXPECT_CALL(*mock, write(_, _)).WillRepeatedly(ReturnArg<1>());
    IFile * pmock = mock;
    IFile * tf = new_throttled_file(pmock, limit);
    photon::thread_yield(); // update now
    auto start = photon::now; // take now as start time
    vector<photon::join_handle*> jhs;
    for (int i=0;i<3;i++) {
        jhs.emplace_back(photon::thread_enable_join(photon::thread_create11(large_pulse_write, tf, i*1024*1024)));
    }
    for (auto p : jhs) {
        photon::thread_join(p);
    }
    EXPECT_GE(photon::now - start, 4UL*1000*1000);
}

TEST(ThrottledFile, limit_cover) {
    using namespace FileSystem;
    using namespace testing;
    ThrottleLimits limit;
    limit.RW.block_size = 0;
    limit.RW.concurent_ops = 0;
    limit.RW.IOPS = 0;
    limit.RW.throughput = 1024;
    Mock::MockNullFile *mock = new Mock::MockNullFile();
    DEFER({ delete mock; });
    EXPECT_CALL(*mock, write(_, _)).WillRepeatedly(ReturnArg<1>());
    IFile * pmock = mock;
    IFile * tf = new_throttled_file(pmock, limit);
    photon::thread_yield(); // update now
    auto start = photon::now; // take now as start time
    vector<photon::join_handle*> jhs;
    for (int i=0;i<3;i++) {
        jhs.emplace_back(photon::thread_enable_join(photon::thread_create11(large_pulse_write, tf, i*1024*1024)));
    }
    for (auto p : jhs) {
        photon::thread_join(p);
    }
    EXPECT_GE(photon::now - start, 4UL*1000*1000);
}

TEST(ThrottledFile, timestamp) {
    StatisticsQueue q(4096, 128);
    auto stamp = q._get_stamp(photon::now / 1024);
    auto time = q._get_time(stamp);
    EXPECT_EQ(photon::now / 1024, time);
}

TEST(ThrottledFile, timebase_overflow) {
    StatisticsQueue q(4096, 128);
    photon::init();
    auto now = photon::now / 1024;
    q.push_back(1);
    EXPECT_EQ(now, q._get_time(q.m_events.front().time_stamp));
    q.m_events.front().time_stamp = (1<<30); //q.m_timestamp_base + 1<<30
    auto t = q._get_time(q.m_events.front().time_stamp);
    q._update_timestamp_base(q.m_timestamp_base + ((1<<30)));
    EXPECT_EQ(t, q._get_time(q.m_events.front().time_stamp));
}


int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    LOG_ERROR_RETURN(0, ret, VALUE(ret));
}
