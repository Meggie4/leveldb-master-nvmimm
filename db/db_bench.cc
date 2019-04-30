// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
///////////meggie
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>
#include <iostream>
///////////meggie
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/write_batch.h"
#include "port/port.h"
#include "util/crc32c.h"
#include "util/histogram.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "util/testutil.h"

// Comma-separated list of operations to run in the specified order
//   Actual benchmarks:
//      fillseq       -- write N values in sequential key order in async mode
//      fillrandom    -- write N values in random key order in async mode
//      overwrite     -- overwrite N values in random key order in async mode
//      fillsync      -- write N/100 values in random key order in sync mode
//      fill100K      -- write N/1000 100K values in random order in async mode
//      deleteseq     -- delete N keys in sequential order
//      deleterandom  -- delete N keys in random order
//      readseq       -- read N times sequentially
//      readreverse   -- read N times in reverse order
//      readrandom    -- read N times in random order
//      readmissing   -- read N missing keys in random order
//      readhot       -- read N times in random order from 1% section of DB
//      seekrandom    -- N random seeks
//      open          -- cost of opening a DB
//      crc32c        -- repeated crc32c of 4K of data
//      acquireload   -- load N*1000 times
//   Meta operations:
//      compact     -- Compact the entire DB
//      stats       -- Print DB stats
//      sstables    -- Print sstable info
//      heapprofile -- Dump a heap profile (if supported by this port)
static const char* FLAGS_benchmarks =
    "fillseq,";/*
    "fillsync,"
    "fillrandom,"
    "overwrite,"
    "readrandom,"
    "readrandom,"  // Extra run to allow previous compactions to quiesce
    "readseq,"
    "readreverse,"
    "compact,"
    "readrandom,"
    "readseq,"
    "readreverse,"
    "fill100K,"
    "crc32c,"
    "snappycomp,"
    "snappyuncomp,"
    "acquireload,"
    ;*/

// Number of key/values to place in database
static int FLAGS_num = 100000;

// Number of read operations to do.  If negative, do FLAGS_num reads.
static int FLAGS_reads = -1;

// Number of concurrent threads to run.
static int FLAGS_threads = 1;

// Size of each value
static int FLAGS_value_size = 1000;

// Arrange to generate values that shrink to this fraction of
// their original size after compression
static double FLAGS_compression_ratio = 0.5;

// Print histogram of operation timings
static bool FLAGS_histogram = false;

// Number of bytes to buffer in memtable before compacting
// (initialized to default value by "main")
static int FLAGS_write_buffer_size = 0;
/////////////meggie
static int FLAGS_nvm_buffer_size = 0;
/////////////meggie

// Number of bytes written to each file.
// (initialized to default value by "main")
static int FLAGS_max_file_size = 0;

// Approximate size of user data packed per block (before compression.
// (initialized to default value by "main")
static int FLAGS_block_size = 0;

// Number of bytes to use as a cache of uncompressed data.
// Negative means use default settings.
static int FLAGS_cache_size = -1;

// Maximum number of files to keep open at the same time (use default if == 0)
static int FLAGS_open_files = 0;

// Bloom filter bits per key.
// Negative means use default settings.
static int FLAGS_bloom_bits = -1;

// If true, do not destroy the existing database.  If you set this
// flag and also specify a benchmark that wants a fresh database, that
// benchmark will fail.
static bool FLAGS_use_existing_db = false;

// If true, reuse existing log/MANIFEST files when re-opening a database.
static bool FLAGS_reuse_logs = false;

// Use the db with the following name.
static const char* FLAGS_db = nullptr;
///////////////meggie
static const char* FLAGS_nvmdb = nullptr;
///////////////meggie

namespace leveldb {

namespace {
leveldb::Env* g_env = nullptr;

// Helper for quickly generating random data.
class RandomGenerator {
 private:
  std::string data_;
  int pos_;

 public:
  RandomGenerator() {
    // We use a limited amount of data over and over again and ensure
    // that it is larger than the compression window (32KB), and also
    // large enough to serve all typical value sizes we want to write.
    Random rnd(301);
    std::string piece;
    while (data_.size() < 1048576) {
      // Add a short fragment that is as compressible as specified
      // by FLAGS_compression_ratio.
      test::CompressibleString(&rnd, FLAGS_compression_ratio, 100, &piece);
      data_.append(piece);
    }
    pos_ = 0;
  }

  Slice Generate(size_t len) {
    if (pos_ + len > data_.size()) {
      pos_ = 0;
      assert(len < data_.size());
    }
    pos_ += len;
    return Slice(data_.data() + pos_ - len, len);
  }
};

////////////////////meggie 
class WorkloadGenerator{
 private:
  std::ifstream fin;
  char* pos;
  char* mapstart;
  char* mapend;
  size_t mapsize;
  int fd;
 public:
  WorkloadGenerator(std::string fname):
      fin(fname){
    FILE* file = fopen(fname.c_str(), "rb");
    if(file == nullptr){
        fprintf(stderr, "file create failed\n");
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    mapsize = ftell(file);
    fclose(file);

    fd = open(fname.c_str(), O_RDWR, 0664);
    mapstart = (char *)mmap(NULL,  mapsize, PROT_READ|PROT_WRITE, 
            MAP_SHARED, fd, 0);
    mapend = mapstart + mapsize;
    pos = mapstart;
  }

  ~WorkloadGenerator(){
      munmap(mapstart, mapsize);
      close(fd);
  }

  bool isValid(std::string type){
    char mytype = type[0];
    if(mytype == 'i' ||
        mytype == 'd' ||
        mytype == 'r' ||
        mytype == 's')
      return true;
    else 
      return false;
  }
  
  bool isValid(char mytype){
    if(mytype == 'i' ||
        mytype == 'd' ||
        mytype == 'r' ||
        mytype == 's')
      return true;
    else 
      return false;
  }

  std::string getData(){
      char* start = pos;
      while(*pos != '\r'){
          pos++;
      }
      size_t length = pos - start;
      char data[length + 1];
      memcpy(data, start, length);
      
      data[length] = '\0';
      while(*pos == '\r'||
            *pos == '\n'){
          pos++;
      }
      std::string result;
      result = data;
      return result;
  }

  Status getRequest(char* type, std::string& key, 
          std::string& value){
    if(pos >= mapend)
        return Status::IOError("it's the end\n");
    *type = *(pos++);
    while(*pos == '\r' ||
          *pos == '\n'){
        pos++;
    }
    
    if(isValid(*type)){
        key = getData();
        value = getData();
        return Status::OK();
    }
    else 
        return Status::IOError("type is unvalid\n");
  }

  Status getRequest1(char* type, std::string& key, 
      std::string& value){
    std::string wtype;
    if(getline(fin, wtype) && isValid(wtype)){
      *type = wtype[0];
      switch(*type){
        case 'i':
          getline(fin, key);
          getline(fin, value);
          break;
        case 'd':
        case 'r':
        case 's':
          getline(fin, key);
          break;
        default:
          return Status::IOError("Already finished obtain request....");
      }
      return Status::OK();
    }
    else 
      return Status::IOError("Already finished obtain request....");
  }
};

////////////////////meggie 
#if defined(__linux)
static Slice TrimSpace(Slice s) {
  size_t start = 0;
  while (start < s.size() && isspace(s[start])) {
    start++;
  }
  size_t limit = s.size();
  while (limit > start && isspace(s[limit-1])) {
    limit--;
  }
  return Slice(s.data() + start, limit - start);
}
#endif

static void AppendWithSpace(std::string* str, Slice msg) {
  if (msg.empty()) return;
  if (!str->empty()) {
    str->push_back(' ');
  }
  str->append(msg.data(), msg.size());
}

class Stats {
 private:
  double start_;
  double finish_;
  double seconds_;
  int done_;
  int next_report_;
  int64_t bytes_;
  double last_op_finish_;
  Histogram hist_;
  std::string message_;

 public:
  Stats() { Start(); }

  void Start() {
    next_report_ = 100;
    last_op_finish_ = start_;
    hist_.Clear();
    done_ = 0;
    bytes_ = 0;
    seconds_ = 0;
    start_ = g_env->NowMicros();
    finish_ = start_;
    message_.clear();
  }

  void Merge(const Stats& other) {
    hist_.Merge(other.hist_);
    done_ += other.done_;
    bytes_ += other.bytes_;
    seconds_ += other.seconds_;
    if (other.start_ < start_) start_ = other.start_;
    if (other.finish_ > finish_) finish_ = other.finish_;

    // Just keep the messages from one thread
    if (message_.empty()) message_ = other.message_;
  }

  void Stop() {
    finish_ = g_env->NowMicros();
    seconds_ = (finish_ - start_) * 1e-6;
  }

  void AddMessage(Slice msg) {
    AppendWithSpace(&message_, msg);
  }

  void FinishedSingleOp() {
    if (FLAGS_histogram) {
      double now = g_env->NowMicros();
      double micros = now - last_op_finish_;
      hist_.Add(micros);
      if (micros > 20000) {
        fprintf(stderr, "long op: %.1f micros%30s\r", micros, "");
        fflush(stderr);
      }
      last_op_finish_ = now;
    }

    done_++;
    if (done_ >= next_report_) {
      if      (next_report_ < 1000)   next_report_ += 100;
      else if (next_report_ < 5000)   next_report_ += 500;
      else if (next_report_ < 10000)  next_report_ += 1000;
      else if (next_report_ < 50000)  next_report_ += 5000;
      else if (next_report_ < 100000) next_report_ += 10000;
      else if (next_report_ < 500000) next_report_ += 50000;
      else                            next_report_ += 100000;
      fprintf(stderr, "... finished %d ops%30s\r", done_, "");
      fflush(stderr);
    }
  }

  void AddBytes(int64_t n) {
    bytes_ += n;
  }

  void Report(const Slice& name) {
    // Pretend at least one op was done in case we are running a benchmark
    // that does not call FinishedSingleOp().
    if (done_ < 1) done_ = 1;

    std::string extra;
    if (bytes_ > 0) {
      // Rate is computed on actual elapsed time, not the sum of per-thread
      // elapsed times.
      double elapsed = (finish_ - start_) * 1e-6;
      char rate[100];
      snprintf(rate, sizeof(rate), "%6.1f MB/s, %luB",
               (bytes_ / 1048576.0) / elapsed, bytes_);
      extra = rate;
    }
    AppendWithSpace(&extra, message_);

    fprintf(stdout, "%-12s : %11.3f micros/op;%s%s\n",
            name.ToString().c_str(),
            seconds_ * 1e6 / done_,
            (extra.empty() ? "" : " "),
            extra.c_str());
    if (FLAGS_histogram) {
      fprintf(stdout, "Microseconds per op:\n%s\n", hist_.ToString().c_str());
    }
    fflush(stdout);
  }
};

// State shared by all concurrent executions of the same benchmark.
struct SharedState {
  port::Mutex mu;
  port::CondVar cv GUARDED_BY(mu);
  int total GUARDED_BY(mu);

  // Each thread goes through the following states:
  //    (1) initializing
  //    (2) waiting for others to be initialized
  //    (3) running
  //    (4) done

  int num_initialized GUARDED_BY(mu);
  int num_done GUARDED_BY(mu);
  bool start GUARDED_BY(mu);

  SharedState(int total)
      : cv(&mu), total(total), num_initialized(0), num_done(0), start(false) { }
};

// Per-thread state for concurrent executions of the same benchmark.
struct ThreadState {
  int tid;             // 0..n-1 when running in n threads
  Random rand;         // Has different seeds for different threads
  Stats stats;
  SharedState* shared;

  ThreadState(int index)
      : tid(index),
        rand(1000 + index) {
  }
};

}  // namespace

class Benchmark {
 private:
  Cache* cache_;
  const FilterPolicy* filter_policy_;
  DB* db_;
  int num_;
  int value_size_;
  int entries_per_batch_;
  WriteOptions write_options_;
  int reads_;
  int heap_counter_;

  void PrintHeader() {
    const int kKeySize = 16;
    PrintEnvironment();
    fprintf(stdout, "Keys:       %d bytes each\n", kKeySize);
    fprintf(stdout, "Values:     %d bytes each (%d bytes after compression)\n",
            FLAGS_value_size,
            static_cast<int>(FLAGS_value_size * FLAGS_compression_ratio + 0.5));
    fprintf(stdout, "Entries:    %d\n", num_);
    fprintf(stdout, "RawSize:    %.1f MB (estimated)\n",
            ((static_cast<int64_t>(kKeySize + FLAGS_value_size) * num_)
             / 1048576.0));
    fprintf(stdout, "FileSize:   %.1f MB (estimated)\n",
            (((kKeySize + FLAGS_value_size * FLAGS_compression_ratio) * num_)
             / 1048576.0));
    PrintWarnings();
    fprintf(stdout, "------------------------------------------------\n");
  }

  void PrintWarnings() {
#if defined(__GNUC__) && !defined(__OPTIMIZE__)
    fprintf(stdout,
            "WARNING: Optimization is disabled: benchmarks unnecessarily slow\n"
            );
#endif
#ifndef NDEBUG
    fprintf(stdout,
            "WARNING: Assertions are enabled; benchmarks unnecessarily slow\n");
#endif

    // See if snappy is working by attempting to compress a compressible string
    const char text[] = "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy";
    std::string compressed;
    if (!port::Snappy_Compress(text, sizeof(text), &compressed)) {
      fprintf(stdout, "WARNING: Snappy compression is not enabled\n");
    } else if (compressed.size() >= sizeof(text)) {
      fprintf(stdout, "WARNING: Snappy compression is not effective\n");
    }
  }

  void PrintEnvironment() {
    fprintf(stderr, "LevelDB:    version %d.%d\n",
            kMajorVersion, kMinorVersion);

#if defined(__linux)
    time_t now = time(nullptr);
    fprintf(stderr, "Date:       %s", ctime(&now));  // ctime() adds newline

    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo != nullptr) {
      char line[1000];
      int num_cpus = 0;
      std::string cpu_type;
      std::string cache_size;
      while (fgets(line, sizeof(line), cpuinfo) != nullptr) {
        const char* sep = strchr(line, ':');
        if (sep == nullptr) {
          continue;
        }
        Slice key = TrimSpace(Slice(line, sep - 1 - line));
        Slice val = TrimSpace(Slice(sep + 1));
        if (key == "model name") {
          ++num_cpus;
          cpu_type = val.ToString();
        } else if (key == "cache size") {
          cache_size = val.ToString();
        }
      }
      fclose(cpuinfo);
      fprintf(stderr, "CPU:        %d * %s\n", num_cpus, cpu_type.c_str());
      fprintf(stderr, "CPUCache:   %s\n", cache_size.c_str());
    }
#endif
  }

 public:
  Benchmark()
  : cache_(FLAGS_cache_size >= 0 ? NewLRUCache(FLAGS_cache_size) : nullptr),
    filter_policy_(FLAGS_bloom_bits >= 0
                   ? NewBloomFilterPolicy(FLAGS_bloom_bits)
                   : nullptr),
    db_(nullptr),
    num_(FLAGS_num),
    value_size_(FLAGS_value_size),
    entries_per_batch_(1),
    reads_(FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads),
    heap_counter_(0) {
    std::vector<std::string> files;
    g_env->GetChildren(FLAGS_db, &files);
    for (size_t i = 0; i < files.size(); i++) {
      if (Slice(files[i]).starts_with("heap-")) {
        g_env->DeleteFile(std::string(FLAGS_db) + "/" + files[i]);
      }
    }
    /////////////meggie
    std::vector<std::string> nvmfiles;
    g_env->GetChildren(FLAGS_nvmdb, &nvmfiles);
    for (size_t i = 0; i < nvmfiles.size(); i++) {
      if (Slice(nvmfiles[i]).starts_with("heap-")) {
        g_env->DeleteFile(std::string(FLAGS_nvmdb) + "/" + nvmfiles[i]);
      }
    }
    ///////////////meggie
    if (!FLAGS_use_existing_db) {
      DestroyDB(FLAGS_db, Options(), FLAGS_nvmdb);
    }
  }

  ~Benchmark() {
    delete db_;
    delete cache_;
    delete filter_policy_;
  }

  void Run() {
    PrintHeader();
    Open();

    const char* benchmarks = FLAGS_benchmarks;
    while (benchmarks != nullptr) {
      const char* sep = strchr(benchmarks, ',');
      Slice name;
      if (sep == nullptr) {
        name = benchmarks;
        benchmarks = nullptr;
      } else {
        name = Slice(benchmarks, sep - benchmarks);
        benchmarks = sep + 1;
      }

      // Reset parameters that may be overridden below
      num_ = FLAGS_num;
      reads_ = (FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads);
      value_size_ = FLAGS_value_size;
      entries_per_batch_ = 1;
      write_options_ = WriteOptions();

      void (Benchmark::*method)(ThreadState*) = nullptr;
      bool fresh_db = false;
      int num_threads = FLAGS_threads;

      if (name == Slice("open")) {
        method = &Benchmark::OpenBench;
        num_ /= 10000;
        if (num_ < 1) num_ = 1;
      } else if (name == Slice("fillseq")) {
        fresh_db = true;
        method = &Benchmark::WriteSeq;
      } else if (name == Slice("fillbatch")) {
        fresh_db = true;
        entries_per_batch_ = 1000;
        method = &Benchmark::WriteSeq;
      } else if (name == Slice("fillrandom")) {
        fresh_db = true;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("overwrite")) {
        fresh_db = false;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("fillsync")) {
        fresh_db = true;
        num_ /= 1000;
        write_options_.sync = true;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("fill100K")) {
        fresh_db = true;
        num_ /= 1000;
        value_size_ = 100 * 1000;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("readseq")) {
        method = &Benchmark::ReadSequential;
      } else if (name == Slice("readreverse")) {
        method = &Benchmark::ReadReverse;
      } else if (name == Slice("readrandom")) {
        method = &Benchmark::ReadRandom;
      } else if (name == Slice("readmissing")) {
        method = &Benchmark::ReadMissing;
      } else if (name == Slice("seekrandom")) {
        method = &Benchmark::SeekRandom;
      } else if (name == Slice("readhot")) {
        method = &Benchmark::ReadHot;
      } else if (name == Slice("readrandomsmall")) {
        reads_ /= 1000;
        method = &Benchmark::ReadRandom;
      } else if (name == Slice("deleteseq")) {
        method = &Benchmark::DeleteSeq;
      } else if (name == Slice("deleterandom")) {
        method = &Benchmark::DeleteRandom;
      } else if (name == Slice("readwhilewriting")) {
        num_threads++;  // Add extra thread for writing
        method = &Benchmark::ReadWhileWriting;
      } else if (name == Slice("compact")) {
        method = &Benchmark::Compact;
      } else if (name == Slice("crc32c")) {
        method = &Benchmark::Crc32c;
      } else if (name == Slice("acquireload")) {
        method = &Benchmark::AcquireLoad;
      } else if (name == Slice("snappycomp")) {
        method = &Benchmark::SnappyCompress;
      } else if (name == Slice("snappyuncomp")) {
        method = &Benchmark::SnappyUncompress;
      } else if (name == Slice("heapprofile")) {
        HeapProfile();
      } else if (name == Slice("stats")) {
        PrintStats("leveldb.stats");
      } else if (name == Slice("sstables")) {
        PrintStats("leveldb.sstables");
      ///////////////meggie
      /////for zipfian1.2
      //////for load 
      } else if(name == Slice("loadzip1k_500k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Loadzip1k_500k;
      } else if(name == Slice("loadzip1k_1000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Loadzip1k_1000k;
      } else if(name == Slice("loadzip1k_2000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Loadzip1k_2000k;
      //////for read 
      } else if(name == Slice("readzip1k_500k")) {
        method = &Benchmark::Readzip1k_500k;
      } else if(name == Slice("readzip1k_1000k")) {
        method = &Benchmark::Readzip1k_1000k;
      } else if(name == Slice("readzip1k_2000k")) {
        method = &Benchmark::Readzip1k_2000k;
      ///for write 
      ////for zipfian
      ///for 1KB value
      } else if(name == Slice("customedzip1k_1000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customedzip1k_1000k;
      } else if(name == Slice("customedzip1k_2000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customedzip1k_2000k;
      } else if(name == Slice("customedzip1k_3000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customedzip1k_3000k;
      ////for zipfian1.2
      ///for 1KB value
      } else if(name == Slice("customed12zip1k_1000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed12zip1k_1000k;
      } else if(name == Slice("customed12zip1k_2000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed12zip1k_2000k;
      } else if(name == Slice("customed12zip1k_3000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed12zip1k_3000k;
      //////////hotspot
      ///////for 100K entries
      //////only write for 1KB value
      } else if(name == Slice("customed99hot1k_100k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed99hot1k_100k;
      } else if(name == Slice("customed80hot1k_100k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed80hot1k_100k;
      } else if(name == Slice("customeduniform1k_100k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::CustomedWorkloadUniform1k_100k;
      ////only write for 4KB value
      } else if(name == Slice("customed99hot4k_100k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed99hot4k_100k;
      } else if(name == Slice("customed80hot4k_100k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed80hot4k_100k;
      } else if(name == Slice("customeduniform4k_100k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::CustomedWorkloadUniform4k_100k;
      /////////for 500K entries
      //////only write for 1KB value
      } else if(name == Slice("customed99hot1k_500k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed99hot1k_500k;
      } else if(name == Slice("customed80hot1k_500k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed80hot1k_500k;
      } else if(name == Slice("customeduniform1k_500k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::CustomedWorkloadUniform1k_500k;
      ////only write for 4KB value
      } else if(name == Slice("customed99hot4k_500k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed99hot4k_500k;
      } else if(name == Slice("customed80hot4k_500k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed80hot4k_500k;
      } else if(name == Slice("customeduniform4k_500k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::CustomedWorkloadUniform4k_500k;
      /////////for 1000K entries
      //////only write for 1KB value
      } else if(name == Slice("customed99hot1k_1000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed99hot1k_1000k;
      } else if(name == Slice("customed80hot1k_1000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed80hot1k_1000k;
      } else if(name == Slice("customeduniform1k_1000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::CustomedWorkloadUniform1k_1000k;
      ////only write for 4KB value
      } else if(name == Slice("customed99hot4k_1000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed99hot4k_1000k;
      } else if(name == Slice("customed80hot4k_1000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::Customed80hot4k_1000k;
      } else if(name == Slice("customeduniform4k_1000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::CustomedWorkloadUniform4k_1000k;
      //////////for 256B
      } else if(name == Slice("customeduniform256_1000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::CustomedWorkloadUniform256_1000k;
      } else if(name == Slice("customeduniform256_5000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::CustomedWorkloadUniform256_5000k;
      } else if(name == Slice("customeduniform256_10000k")) {
        entries_per_batch_ = 1000;
        fresh_db = true;
        method = &Benchmark::CustomedWorkloadUniform256_10000k;
      //////////////meggie 
      } else {
        if (name != Slice()) {  // No error message for empty name
          fprintf(stderr, "unknown benchmark '%s'\n", name.ToString().c_str());
        }
      }

      if (fresh_db) {
        if (FLAGS_use_existing_db) {
          fprintf(stdout, "%-12s : skipped (--use_existing_db is true)\n",
                  name.ToString().c_str());
          method = nullptr;
        } else {
          delete db_;
          db_ = nullptr;
          DestroyDB(FLAGS_db, Options(), FLAGS_nvmdb);
          Open();
        }
      }

      if (method != nullptr) {
        RunBenchmark(num_threads, name, method);
      }
      ////////////meggie
      db_->PrintTimerAudit();
      ////////////meggie
    }
  }

 private:
  struct ThreadArg {
    Benchmark* bm;
    SharedState* shared;
    ThreadState* thread;
    void (Benchmark::*method)(ThreadState*);
  };

  static void ThreadBody(void* v) {
    ThreadArg* arg = reinterpret_cast<ThreadArg*>(v);
    SharedState* shared = arg->shared;
    ThreadState* thread = arg->thread;
    {
      MutexLock l(&shared->mu);
      shared->num_initialized++;
      if (shared->num_initialized >= shared->total) {
        shared->cv.SignalAll();
      }
      while (!shared->start) {
        shared->cv.Wait();
      }
    }

    thread->stats.Start();
    (arg->bm->*(arg->method))(thread);
    thread->stats.Stop();

    {
      MutexLock l(&shared->mu);
      shared->num_done++;
      if (shared->num_done >= shared->total) {
        shared->cv.SignalAll();
      }
    }
  }

  void RunBenchmark(int n, Slice name,
                    void (Benchmark::*method)(ThreadState*)) {
    SharedState shared(n);

    ThreadArg* arg = new ThreadArg[n];
    for (int i = 0; i < n; i++) {
      arg[i].bm = this;
      arg[i].method = method;
      arg[i].shared = &shared;
      arg[i].thread = new ThreadState(i);
      arg[i].thread->shared = &shared;
      g_env->StartThread(ThreadBody, &arg[i]);
    }

    shared.mu.Lock();
    while (shared.num_initialized < n) {
      shared.cv.Wait();
    }

    shared.start = true;
    shared.cv.SignalAll();
    while (shared.num_done < n) {
      shared.cv.Wait();
    }
    shared.mu.Unlock();

    for (int i = 1; i < n; i++) {
      arg[0].thread->stats.Merge(arg[i].thread->stats);
    }
    arg[0].thread->stats.Report(name);

    for (int i = 0; i < n; i++) {
      delete arg[i].thread;
    }
    delete[] arg;
  }

  void Crc32c(ThreadState* thread) {
    // Checksum about 500MB of data total
    const int size = 4096;
    const char* label = "(4K per op)";
    std::string data(size, 'x');
    int64_t bytes = 0;
    uint32_t crc = 0;
    while (bytes < 500 * 1048576) {
      crc = crc32c::Value(data.data(), size);
      thread->stats.FinishedSingleOp();
      bytes += size;
    }
    // Print so result is not dead
    fprintf(stderr, "... crc=0x%x\r", static_cast<unsigned int>(crc));

    thread->stats.AddBytes(bytes);
    thread->stats.AddMessage(label);
  }

  void AcquireLoad(ThreadState* thread) {
    int dummy;
    port::AtomicPointer ap(&dummy);
    int count = 0;
    void *ptr = nullptr;
    thread->stats.AddMessage("(each op is 1000 loads)");
    while (count < 100000) {
      for (int i = 0; i < 1000; i++) {
        ptr = ap.Acquire_Load();
      }
      count++;
      thread->stats.FinishedSingleOp();
    }
    if (ptr == nullptr) exit(1); // Disable unused variable warning.
  }

  void SnappyCompress(ThreadState* thread) {
    RandomGenerator gen;
    Slice input = gen.Generate(Options().block_size);
    int64_t bytes = 0;
    int64_t produced = 0;
    bool ok = true;
    std::string compressed;
    while (ok && bytes < 1024 * 1048576) {  // Compress 1G
      ok = port::Snappy_Compress(input.data(), input.size(), &compressed);
      produced += compressed.size();
      bytes += input.size();
      thread->stats.FinishedSingleOp();
    }

    if (!ok) {
      thread->stats.AddMessage("(snappy failure)");
    } else {
      char buf[100];
      snprintf(buf, sizeof(buf), "(output: %.1f%%)",
               (produced * 100.0) / bytes);
      thread->stats.AddMessage(buf);
      thread->stats.AddBytes(bytes);
    }
  }

  void SnappyUncompress(ThreadState* thread) {
    RandomGenerator gen;
    Slice input = gen.Generate(Options().block_size);
    std::string compressed;
    bool ok = port::Snappy_Compress(input.data(), input.size(), &compressed);
    int64_t bytes = 0;
    char* uncompressed = new char[input.size()];
    while (ok && bytes < 1024 * 1048576) {  // Compress 1G
      ok =  port::Snappy_Uncompress(compressed.data(), compressed.size(),
                                    uncompressed);
      bytes += input.size();
      thread->stats.FinishedSingleOp();
    }
    delete[] uncompressed;

    if (!ok) {
      thread->stats.AddMessage("(snappy failure)");
    } else {
      thread->stats.AddBytes(bytes);
    }
  }

  void Open() {
    assert(db_ == nullptr);
    Options options;
    options.env = g_env;
    options.create_if_missing = !FLAGS_use_existing_db;
    options.block_cache = cache_;
    options.write_buffer_size = FLAGS_write_buffer_size;
    /////////////meggie
    options.nvm_buffer_size = FLAGS_nvm_buffer_size;
    /////////////meggie
    options.max_file_size = FLAGS_max_file_size;
    options.block_size = FLAGS_block_size;
    options.max_open_files = FLAGS_open_files;
    options.filter_policy = filter_policy_;
    options.reuse_logs = FLAGS_reuse_logs;
    Status s = DB::Open(options, FLAGS_db, &db_, FLAGS_nvmdb);
    if (!s.ok()) {
      fprintf(stderr, "open error: %s\n", s.ToString().c_str());
      exit(1);
    }
  }

  void OpenBench(ThreadState* thread) {
    for (int i = 0; i < num_; i++) {
      delete db_;
      Open();
      thread->stats.FinishedSingleOp();
    }
  }

  void WriteSeq(ThreadState* thread) {
    DoWrite(thread, true);
  }

  void WriteRandom(ThreadState* thread) {
    DoWrite(thread, false);
  }

  void DoWrite(ThreadState* thread, bool seq) {
    if (num_ != FLAGS_num) {
      char msg[100];
      snprintf(msg, sizeof(msg), "(%d ops)", num_);
      thread->stats.AddMessage(msg);
    }

    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    int64_t bytes = 0;
    for (int i = 0; i < num_; i += entries_per_batch_) {
      batch.Clear();
      for (int j = 0; j < entries_per_batch_; j++) {
        const int k = seq ? i+j : (thread->rand.Next() % FLAGS_num);
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);
        batch.Put(key, gen.Generate(value_size_));
        bytes += value_size_ + strlen(key);
        thread->stats.FinishedSingleOp();
      }
      s = db_->Write(write_options_, &batch);
      if (!s.ok()) {
        fprintf(stderr, "put error: %s\n", s.ToString().c_str());
        exit(1);
      }
    }
    thread->stats.AddBytes(bytes);
  }

  void ReadSequential(ThreadState* thread) {
    Iterator* iter = db_->NewIterator(ReadOptions());
    int i = 0;
    int64_t bytes = 0;
    for (iter->SeekToFirst(); i < reads_ && iter->Valid(); iter->Next()) {
      bytes += iter->key().size() + iter->value().size();
      thread->stats.FinishedSingleOp();
      ++i;
    }
    delete iter;
    thread->stats.AddBytes(bytes);
  }

  void ReadReverse(ThreadState* thread) {
    Iterator* iter = db_->NewIterator(ReadOptions());
    int i = 0;
    int64_t bytes = 0;
    for (iter->SeekToLast(); i < reads_ && iter->Valid(); iter->Prev()) {
      bytes += iter->key().size() + iter->value().size();
      thread->stats.FinishedSingleOp();
      ++i;
    }
    delete iter;
    thread->stats.AddBytes(bytes);
  }

  void ReadRandom(ThreadState* thread) {
    ReadOptions options;
    std::string value;
    int found = 0;
    for (int i = 0; i < reads_; i++) {
      char key[100];
      const int k = thread->rand.Next() % FLAGS_num;
      snprintf(key, sizeof(key), "%016d", k);
      if (db_->Get(options, key, &value).ok()) {
        found++;
      }
      thread->stats.FinishedSingleOp();
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%d of %d found)", found, num_);
    thread->stats.AddMessage(msg);
  }

  void ReadMissing(ThreadState* thread) {
    ReadOptions options;
    std::string value;
    for (int i = 0; i < reads_; i++) {
      char key[100];
      const int k = thread->rand.Next() % FLAGS_num;
      snprintf(key, sizeof(key), "%016d.", k);
      db_->Get(options, key, &value);
      thread->stats.FinishedSingleOp();
    }
  }

  void ReadHot(ThreadState* thread) {
    ReadOptions options;
    std::string value;
    const int range = (FLAGS_num + 99) / 100;
    for (int i = 0; i < reads_; i++) {
      char key[100];
      const int k = thread->rand.Next() % range;
      snprintf(key, sizeof(key), "%016d", k);
      db_->Get(options, key, &value);
      thread->stats.FinishedSingleOp();
    }
  }

  void SeekRandom(ThreadState* thread) {
    ReadOptions options;
    int found = 0;
    for (int i = 0; i < reads_; i++) {
      Iterator* iter = db_->NewIterator(options);
      char key[100];
      const int k = thread->rand.Next() % FLAGS_num;
      snprintf(key, sizeof(key), "%016d", k);
      iter->Seek(key);
      if (iter->Valid() && iter->key() == key) found++;
      delete iter;
      thread->stats.FinishedSingleOp();
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%d of %d found)", found, num_);
    thread->stats.AddMessage(msg);
  }

  void DoDelete(ThreadState* thread, bool seq) {
    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    for (int i = 0; i < num_; i += entries_per_batch_) {
      batch.Clear();
      for (int j = 0; j < entries_per_batch_; j++) {
        const int k = seq ? i+j : (thread->rand.Next() % FLAGS_num);
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);
        batch.Delete(key);
        thread->stats.FinishedSingleOp();
      }
      s = db_->Write(write_options_, &batch);
      if (!s.ok()) {
        fprintf(stderr, "del error: %s\n", s.ToString().c_str());
        exit(1);
      }
    }
  }

  void DeleteSeq(ThreadState* thread) {
    DoDelete(thread, true);
  }

  void DeleteRandom(ThreadState* thread) {
    DoDelete(thread, false);
  }

  void ReadWhileWriting(ThreadState* thread) {
    if (thread->tid > 0) {
      ReadRandom(thread);
    } else {
      // Special thread that keeps writing until other threads are done.
      RandomGenerator gen;
      while (true) {
        {
          MutexLock l(&thread->shared->mu);
          if (thread->shared->num_done + 1 >= thread->shared->num_initialized) {
            // Other threads have finished
            break;
          }
        }

        const int k = thread->rand.Next() % FLAGS_num;
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);
        Status s = db_->Put(write_options_, key, gen.Generate(value_size_));
        if (!s.ok()) {
          fprintf(stderr, "put error: %s\n", s.ToString().c_str());
          exit(1);
        }
      }

      // Do not count any of the preceding work/delay in stats.
      thread->stats.Start();
    }
  }
  //////////////meggie
  //////////Read 
  ///////zipfian1.2
  //////////load 
  void Loadzip1k_500k(ThreadState* thread){
      std::string fname = "/mnt/readworkloads/workloadzip1.2/runload1k_500k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  void Loadzip1k_1000k(ThreadState* thread){
      std::string fname = "/mnt/readworkloads/workloadzip1.2/runload1k_1000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  void Loadzip1k_2000k(ThreadState* thread){
      std::string fname = "/mnt/readworkloads/workloadzip1.2/runload1k_2000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  //////read
  void Readzip1k_500k(ThreadState* thread){
      std::string fname = "/mnt/readworkloads/workloadzip1.2/runread1k_500k.txt"; 
      CustomedWorkloadRead(thread, fname);
  }
  void Readzip1k_1000k(ThreadState* thread){
      std::string fname = "/mnt/readworkloads/workloadzip1.2/runread1k_1000k.txt"; 
      CustomedWorkloadRead(thread, fname);
  }
  void Readzip1k_2000k(ThreadState* thread){
      std::string fname = "/mnt/readworkloads/workloadzip1.2/runread1k_2000k.txt"; 
      CustomedWorkloadRead(thread, fname);
  }

  void CustomedWorkloadRead(ThreadState* thread, std::string fname) {
      leveldb::WorkloadGenerator wlgnerator(fname);
      char type;
      std::string key;
      std::string value;
      int found = 0;
      ReadOptions options;
      std::string read_value;
      while(wlgnerator.getRequest(&type, key, value).ok()){
        if(db_->Get(options, key, &read_value).ok()) {
            found++;
        }
        thread->stats.FinishedSingleOp();
        break;
      }
      char msg[100];
      snprintf(msg, sizeof(msg), "(%d found)", found);
      thread->stats.AddMessage(msg);
  } 
  
  ///////////write
  ////for zipfian 
  //for 1KB value
  void Customedzip1k_1000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloadzip/runwrite1k_1000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  void Customedzip1k_2000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloadzip/runwrite1k_2000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  void Customedzip1k_3000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloadzip/runwrite1k_3000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  
  ////for zipfian1.2
  //for 1KB value
  void Customed12zip1k_1000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloadzip1.2/runwrite1k_1000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  void Customed12zip1k_2000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloadzip1.2/runwrite1k_2000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  void Customed12zip1k_3000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloadzip1.2/runwrite1k_3000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  ///for hotspot
  ////////for 100K entries 
  /////only write
  //for 1k
  void Customed99hot1k_100k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot1_99/runwrite1k_100K.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
 
  void Customed80hot1k_100k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot20_80/runwrite1k_100k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  void CustomedWorkloadUniform1k_100k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloaduniform/runwrite1k_100k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  ///for 4k
  void Customed99hot4k_100k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot1_99/runwrite4k_100K.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
 
  void Customed80hot4k_100k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot20_80/runwrite4k_100k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  void CustomedWorkloadUniform4k_100k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloaduniform/runwrite4k_100k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  ////////for 500K entries 
  /////only write
  //for 1k
  void Customed99hot1k_500k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot1_99/runwrite1k_500K.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
 
  void Customed80hot1k_500k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot20_80/runwrite1k_500k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  void CustomedWorkloadUniform1k_500k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloaduniform/runwrite1k_500k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  ///for 4k
  void Customed99hot4k_500k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot1_99/runwrite4k_500K.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
 
  void Customed80hot4k_500k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot20_80/runwrite4k_500k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  void CustomedWorkloadUniform4k_500k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloaduniform/runwrite4k_500k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  ////////for 1000K entries 
  /////only write
  //for 1k
  void Customed99hot1k_1000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot1_99/runwrite1k_1000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
 
  void Customed80hot1k_1000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot20_80/runwrite1k_1000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  void CustomedWorkloadUniform1k_1000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloaduniform/runwrite1k_1000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  ///for 4k
  void Customed99hot4k_1000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot1_99/runwrite4k_1000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
 
  void Customed80hot4k_1000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/hot20_80/runwrite4k_1000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  void CustomedWorkloadUniform4k_1000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloaduniform/runwrite4k_1000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  //////////for 256B
  void CustomedWorkloadUniform256_1000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloaduniform/runwrite256_1000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  void CustomedWorkloadUniform256_5000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloaduniform/runwrite256_5000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }
  void CustomedWorkloadUniform256_10000k(ThreadState* thread){
      std::string fname = "/mnt/workloads/workloaduniform/runwrite256_10000k.txt"; 
      CustomedWorkloadWrite(thread, fname);
  }

  void CustomedWorkloadWrite(ThreadState* thread, std::string fname) {
    WriteBatch batch;
    Status s;
    int64_t bytes = 0;
    leveldb::WorkloadGenerator wlgnerator(fname);
    char type;
    std::string key;
    std::string value;
    int batch_num = 0;
    while(wlgnerator.getRequest(&type, key, value).ok()){
        if(batch_num < entries_per_batch_){
            batch.Put(key, value);
            batch_num++;
        }
        else{
            s = db_->Write(write_options_, &batch);
            if (!s.ok()) {
                fprintf(stderr, "put error: %s\n", s.ToString().c_str());
                exit(1);
            }
            batch.Clear();
            batch.Put(key, value);
            batch_num = 1;
        }
        bytes += value.size() + key.size();
        thread->stats.FinishedSingleOp();
    }
    //fprintf(stderr, "add bytes:%lu\n", bytes);
    thread->stats.AddBytes(bytes);
  }
  //////////////meggie
  void Compact(ThreadState* thread) {
    db_->CompactRange(nullptr, nullptr);
  }

  void PrintStats(const char* key) {
    std::string stats;
    if (!db_->GetProperty(key, &stats)) {
      stats = "(failed)";
    }
    fprintf(stdout, "\n%s\n", stats.c_str());
  }

  static void WriteToFile(void* arg, const char* buf, int n) {
    reinterpret_cast<WritableFile*>(arg)->Append(Slice(buf, n));
  }

  void HeapProfile() {
    char fname[100];
    snprintf(fname, sizeof(fname), "%s/heap-%04d", FLAGS_db, ++heap_counter_);
    WritableFile* file;
    Status s = g_env->NewWritableFile(fname, &file);
    if (!s.ok()) {
      fprintf(stderr, "%s\n", s.ToString().c_str());
      return;
    }
    bool ok = port::GetHeapProfile(WriteToFile, file);
    delete file;
    if (!ok) {
      fprintf(stderr, "heap profiling not supported\n");
      g_env->DeleteFile(fname);
    }
  }
};

}  // namespace leveldb

int main(int argc, char** argv) {
  FLAGS_write_buffer_size = leveldb::Options().write_buffer_size;
  //////////////meggie
  FLAGS_nvm_buffer_size = leveldb::Options().nvm_buffer_size;
  //////////////meggie
  FLAGS_max_file_size = leveldb::Options().max_file_size;
  FLAGS_block_size = leveldb::Options().block_size;
  FLAGS_open_files = leveldb::Options().max_open_files;
  std::string default_db_path;
  /////////////meggie
  std::string nvm_db_path;
  /////////////meggie

  for (int i = 1; i < argc; i++) {
    double d;
    int n;
    char junk;
    if (leveldb::Slice(argv[i]).starts_with("--benchmarks=")) {
      FLAGS_benchmarks = argv[i] + strlen("--benchmarks=");
    } else if (sscanf(argv[i], "--compression_ratio=%lf%c", &d, &junk) == 1) {
      FLAGS_compression_ratio = d;
    } else if (sscanf(argv[i], "--histogram=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_histogram = n;
    } else if (sscanf(argv[i], "--use_existing_db=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_use_existing_db = n;
    } else if (sscanf(argv[i], "--reuse_logs=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_reuse_logs = n;
    } else if (sscanf(argv[i], "--num=%d%c", &n, &junk) == 1) {
      FLAGS_num = n;
    } else if (sscanf(argv[i], "--reads=%d%c", &n, &junk) == 1) {
      FLAGS_reads = n;
    } else if (sscanf(argv[i], "--threads=%d%c", &n, &junk) == 1) {
      FLAGS_threads = n;
    } else if (sscanf(argv[i], "--value_size=%d%c", &n, &junk) == 1) {
      FLAGS_value_size = n;
    } else if (sscanf(argv[i], "--write_buffer_size=%d%c", &n, &junk) == 1) {
      FLAGS_write_buffer_size = n * 1024L * 1024L;
    /////////////////meggie
    } else if (sscanf(argv[i], "--nvm_buffer_size=%d%c", &n, &junk) == 1) {
      FLAGS_nvm_buffer_size = n * 1024L * 1024L;
    /////////////////meggie
    } else if (sscanf(argv[i], "--max_file_size=%d%c", &n, &junk) == 1) {
      FLAGS_max_file_size = n;
    } else if (sscanf(argv[i], "--block_size=%d%c", &n, &junk) == 1) {
      FLAGS_block_size = n;
    } else if (sscanf(argv[i], "--cache_size=%d%c", &n, &junk) == 1) {
      FLAGS_cache_size = n;
    } else if (sscanf(argv[i], "--bloom_bits=%d%c", &n, &junk) == 1) {
      FLAGS_bloom_bits = n;
    } else if (sscanf(argv[i], "--open_files=%d%c", &n, &junk) == 1) {
      FLAGS_open_files = n;
    } else if (strncmp(argv[i], "--db=", 5) == 0) {
      FLAGS_db = argv[i] + 5;
    } else {
      fprintf(stderr, "Invalid flag '%s'\n", argv[i]);
      exit(1);
    }
  }

  leveldb::g_env = leveldb::Env::Default();

  // Choose a location for the test database if none given with --db=<path>
  if (FLAGS_db == nullptr) {
      leveldb::g_env->GetTestDirectory(&default_db_path);
      default_db_path += "/dbbench";
      FLAGS_db = default_db_path.c_str();
  }

  /////////////meggie
  if (FLAGS_nvmdb == nullptr) {
      leveldb::g_env->GetMEMDirectory(&nvm_db_path);
      nvm_db_path += "/dbbench";
      FLAGS_nvmdb = nvm_db_path.c_str();
  }
  /////////////meggie
  leveldb::Benchmark benchmark;
  benchmark.Run();
  return 0;
}

/*
int main(){
    std::string fname = "/home/meggie/文档/workloads/workload099/runwrite1k_100k.txt";
    leveldb::WorkloadGenerator wlgnerator(fname);
    char type;
    std::string key;
    std::string value;
    int count = 0;
    while(wlgnerator.getRequest1(&type, key, value).ok()){
        std::string mytype;
        mytype.push_back(type);
        switch(type){
            case 'i':
                std::cout<<"type:  "<<mytype<<", key:  "<<
                    key<<std::endl;
                break;
            case 'd':
            case 'r':
            case 's':
                std::cout<<"type:  "<<mytype<<", key:  "<<key<<std::endl;
                break;
            default:
                std::cout<<"none known type"<<std::endl;
                break;
        }
        count++;
    }
    std::cout<<"kv pair nums: "<<count<<std::endl;
    return 0;
}
*/
