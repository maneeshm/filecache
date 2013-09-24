//
// Copyright (c) 2013 Nutanix Inc. All rights reserved.
//
// The problem is to implement a file cache in C that implements the defined
// interface given below for struct 'file_cache'. The typical usage is for a
// client to call 'file_cache_pin_files()' to pin a bunch of files in the cache
// and then either read or write to their in-memory contents in the cache.
// Writing to a cache entry makes that entry 'dirty'. Before a dirty entry can
// be evicted from the cache, it must be unpinned and has to be cleaned by
// writing the corresponding data to storage.
//
// All files are assumed to have size 10KB. If a file doesn't exist to begin
// with, it should be created and filled with zeros - the size should be 10KB.
//
// 'file_cache' should be a thread-safe object that can be simultaneously
// accessed by multiple threads. If you are not comfortable with concurrent
// programming, then it may be single-threaded (see alternative in the
// file_cache_pin_files() comment). To implement the problem in its entirety
// may require use of third party libraries and headers. For the sake of
// convenience, it is permissible (although not preferred) to substitute
// external functions with stub implementations, but in doing so, please be
// clear what the intended behavior and side effects would be.
//
// Do not be overly concerned about portability. Pick your platform of choice
// (Nutanix develops on Linux) and be consistent.
//
// The problem has an upper limit of two hours. Note that we'd rather see a
// stable and clean solution that implements a subset of the functionality than
// a complete but-buggy one.
//
// If you have any questions, please email both brian@nutanix.com and
// bnc@nutanix.com. If no reply is received in a timely manner, please make a
// judgement call and state your assumptions in the code or response email.

#ifndef _NUTANIX_FILE_CACHE_H_
#define _NUTANIX_FILE_CACHE_H_

struct file_cache;

// Constructor. 'max_cache_entries' is the maximum number of files that can
// be cached at any time.
struct file_cache *filecacheconstruct(int max_cache_entries);

// Destructor. Flushes all dirty buffers.
void filecachedestroy(struct file_cache *cache);

// Pins the given files in array 'files' with size 'num_files' in the cache.
// If any of these files are not already cached, they are first read from the
// local filesystem. If the cache is full, then some existing cache entries may
// be evicted. If no entries can be evicted (e.g., if they are all pinned, or
// dirty), then this method will block until a suitable number of cache
// entries becomes available. It is OK for more than one thread to pin the
// same file, however the file should not become unpinned until both pins
// have been removed.
//
// Is is the application's responsibility to ensure that the files may
// eventually be pinned. For example, if 'max_cache_entries' is 5, an
// irresponsible client may try to pin 4 files, and then an additional 2
// files without unpinning any, resulting in the client deadlocking. The
// implementation *does not* have to handle this.
//
// If you are not comfortable with multi-threaded programming or
// synchronization, this function may be modified to return a boolean if
// the requested files cannot be pinned due to the cache being full. However,
// note that entries in 'files' may already be pinned and therefore even a
// full cache may add additional pins to files.
void filecachepinfiles(struct file_cache *cache,
                          const char **files,
                          int num_files);

// Unpin one or more files that were previously pinned. It is ok to unpin
// only a subset of the files that were previously pinned using
// file_cache_pin_files(). It is undefined behavior to unpin a file that wasn't
// pinned.
void filecacheunpinfiles(struct file_cache *cache,
                            const char **files,
                            int num_files);

// Provide read-only access to a pinned file's data in the cache.
//
// It is undefined behavior if the file is not pinned, or to access the buffer
// when the file isn't pinned.
const char *filecachefiledata(struct file_cache *cache, const char *file);

// Provide write access to a pinned file's data in the cache. This call marks
// the file's data as 'dirty'. The caller may update the contents of the file
// by writing to the memory pointed by the returned value.
//
// Multiple clients may have access to the data, however the cache *does not*
// have to worry about synchronizing the clients' accesses (you may assume
// the application does this correctly).
//
// It is undefined behavior if the file is not pinned, or to access the buffer
// when the file is not pinned.
char *filecachemutablefiledata(struct file_cache *cache, const char *file);

#endif  // _NUTANIX_FILE_CACHE_H_
