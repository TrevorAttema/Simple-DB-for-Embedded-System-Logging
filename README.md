# Yet Another Key Value DB for Embedded Systems

**DBEngine** is a high-performance, lightweight database key value engine written in C++ for microcontrollers such as the SAMD21. It is designed to be fast when reading data, minimize resource usage, and operate without the STL. The engine was developed and tested on Windows and later ported to embedded systems. It relies on a user?provided implementation of the `IFileHandler` interface to integrate with the target hardware’s file I/O mechanisms.

## Overview

DBEngine provides a simple log-and-index system for storing, retrieving, and updating records on disk. The engine maintains a sorted index of record keys (e.g., timestamps or unique IDs) that point to positions in a log file where the actual data is stored. Using a paged index system, DBEngine can efficiently manage large amounts of data even when only a fraction of the index is kept in memory at any time.

## Key Capabilities

- **Record Appending:**  
  Append records to a log file with a generated unique key, ensuring that duplicate keys are rejected.

- **Record Retrieval:**  
  Retrieve records by index ID or by key with fast lookup times.

- **Record Updating:**  
  Update the status of a record in both the log file and the index.

- **Index Management:**  
  Maintain a sorted, paged index with binary search and B-tree–style search methods for fast lookups.

- **Index Integrity:**  
  Validate the index (e.g., check that keys are sorted) to detect corruption.

- **Instrumentation:**  
  Measure execution times of critical functions using scoped timers to help optimize performance.

## Design Philosophy

- **Lightweight & Fast:**  
  Written without STL dependencies and dynamic memory allocation, DBEngine is optimized for systems with limited resources. The engine is designed for lightning-fast reads and minimal overhead when writing or updating records.

- **Portability & Modularity:**  
  The separation of file I/O into an abstract `IFileHandler` interface means you can easily port DBEngine to any system by providing your own file handler implementation. For Windows development, a `WindowsFileHandler` is included.

- **Robustness:**  
  The engine employs defense-in-depth techniques (e.g., duplicate key checks both at record-append time and in the index insertion logic) and includes validation routines to keep the data and index in sync and error free.

- **Scalability:**  
  Instead of loading the entire index into memory, the engine uses a paged index system. Only one page of index entries (up to 256 entries) is held in memory at a time, reducing RAM requirements while still supporting efficient binary and B-tree–style searches.

## Project Structure

### DBEngine

The `DBEngine` class is the core of the project. It manages log and index files, implements record operations, and handles all index-related tasks.

#### Main Functions

- **`dbOpen`**  
  Initializes the engine with given filenames (using 8.3 format) and resets internal state.

- **`dbBuildIndex`**  
  Loads (or creates) the index header, validates the index integrity, and sets up the in-memory paged index.  
  *Key Helpers:* `loadIndexHeader`, `saveIndexHeader`, `validateIndex`.

- **`dbAppendRecord`**  
  Appends a new record to the log file. It first generates a key (using a millisecond counter), checks for duplicate keys in the index, writes the record header and data, and finally inserts an index entry.  
  *Key Helpers:* `insertIndexEntry`, `splitPageAndInsert`.

- **`dbUpdateRecordStatusByIndexId`**  
  Updates the status field of a record in both the log file and its corresponding index entry.

- **`dbGetRecordByIndexId` / `dbGetRecordByKey`**  
  Retrieve a record from the log file using an index ID or by searching for a specific key.

- **Index Paging Functions:**  
  `loadIndexPage`, `flushIndexPage`, `getIndexEntry`, and `setIndexEntry` handle the paged in-memory index that is written to and read from disk as needed.

- **B-Tree Style Search Methods:**  
  Functions such as `btreeFindKey`, `btreeLocateKey`, `btreeNextKey`, and `btreePrevKey` implement efficient search techniques on the sorted index.

### IFileHandler

The `IFileHandler` class is an abstract interface that defines all necessary file operations:
- **`open`** – Opens a file in a specified mode (e.g., "rb+", "wb+").
- **`close`** – Closes the file.
- **`seek` / `seekToEnd`** – Moves the file pointer.
- **`tell`** – Returns the current file position.
- **`read` / `write`** – Performs reading and writing operations.

You must provide an implementation of this interface (for example, `WindowsFileHandler` for Windows development) to integrate DBEngine with your target system.

### Instrumentation

Using the `Instrumentation.h` header and the `SCOPE_TIMER` macro, the engine measures the execution time of critical functions. This data can help you tune performance and ensure that operations meet the speed requirements of your application.

## Detailed Functionality

### Record Handling

- **`dbAppendRecord`**  
  Generates a unique key, verifies that no duplicate key exists (by calling `searchIndex`), and then appends a new record to the log file. After successfully writing the log record, it calls `insertIndexEntry` to add the corresponding index entry.

- **`dbUpdateRecordStatusByIndexId`**  
  Retrieves the index entry for the given ID, opens the log file to update the status field at the calculated offset, and finally updates the in-memory index.

- **`dbGetRecordByIndexId` / `dbGetRecordByKey`**  
  These functions open the log file, seek to the record’s offset (retrieved from the index), read the record header, and then read the actual record data into a provided buffer.

### Index Handling

- **`insertIndexEntry`**  
  Uses binary search to find the correct insertion point for a new index entry. It performs duplicate key checks (even if these checks have been performed earlier) to enforce uniqueness. If the target index page is full, it calls `splitPageAndInsert` to split the page and then insert the new entry.

- **`splitPageAndInsert`**  
  When an index page is full, this function splits the page roughly in half, inserts the new entry into the correct half, updates the global index count, and flushes the pages to disk.

- **Paging Functions (`loadIndexPage`, `flushIndexPage`, etc.):**  
  These manage the in-memory index pages, ensuring that only a small subset of the index is loaded at a time. This design minimizes RAM usage while still allowing efficient access and updates.

- **Index Validation (`validateIndex`):**  
  On building the index, the engine loads the first page and checks that the keys are in sorted order to detect any corruption.

### B-Tree–Style Searches

- **`btreeFindKey` and `btreeLocateKey`:**  
  Provide binary search capabilities to either find an exact key or locate the smallest key greater than or equal to a given value.

- **`btreeNextKey` and `btreePrevKey`:**  
  Return the next or previous index in the sequence, facilitating sequential access in a sorted index.

