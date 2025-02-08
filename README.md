
# Yet Another Key Value DB for Embedded Systems

**DBEngine** is a lightweight, high–performance key–value database engine written in C++ that is ideally suited for embedded systems. Whether you need to log sensor data to an SD card, store event logs in onboard flash memory, or efficiently capture and process high–frequency data, DBEngine provides a simple yet robust solution with minimal resource overhead.

---

## Quick Start

1. **Implement the IFileHandler Interface:**  
   Create your own file handler for your target storage device (e.g., SD card, onboard flash, or any block device). If you’re using FatFs, wrap the FatFs functions in your implementation. See `FileHandler_Windows.cpp` for an example reference.

2. **Instantiate DBEngine:**  
   Create an instance of `DBEngine` using your IFileHandler implementations for both the log and index files.

3. **Open the Database:**  
   Call `open()` with your chosen filenames. This call automatically opens the files, validates (or creates) the headers, and loads the first index page into memory.

4. **Use the Database Functions:**  
   Append, retrieve, update, or delete records, and use the provided search and filtering functions to access data quickly.

---

## Overview

DBEngine uses a log–and–index storage strategy:

- **Log File:**  
  Data records are appended sequentially to a log file. Each record consists of a header (with metadata such as record type, length, key, and status) followed by the payload.

- **Index File:**  
  A sorted index file maps record keys to their offsets in the log file. This index is managed in fixed–size pages to minimize RAM usage—only one page (by default, 256 entries) is loaded into memory at any time.

When you call **`open`**, the engine automatically performs all necessary file operations (opening, header verification/creation, and initial index page load) so that the database is immediately ready for use.

---

## Porting Guide

### Writing Your Own IFileHandler

DBEngine is designed to be portable. The only platform–specific component is the file I/O, which is abstracted by the `IFileHandler` interface. To port DBEngine to your platform:

- **Implement the Following Functions:**
  - **`open(const char* filename, const char* mode)`**  
    Open a file (e.g., using FatFs, your platform’s flash driver, or other block device routines).
  - **`close()`**  
    Close the file.
  - **`seek(uint32_t position)` / `seekToEnd()`**  
    Move the file pointer to a specified position or the end of the file.
  - **`tell()`**  
    Return the current file pointer position.
  - **`read(uint8_t* buffer, size_t size, size_t &bytesRead)`**  
    Read data from the file.
  - **`write(const uint8_t* data, size_t size, size_t &bytesWritten)`**  
    Write data to the file.

- **Reference:**  
  Use the provided `FileHandler_Windows.cpp` as a reference for mapping standard file operations. If you’re using FatFs, your implementation will need to wrap the FatFs functions.

- **Any Block Device:**  
  In fact, any block device (e.g., SD card, NAND flash, SPI NOR flash) can be used as long as your IFileHandler implementation respects the defined interface.

---

## Memory Consumption & Paging Configuration

### How the Paged Index Works

- **Index in Pages:**  
  The index is divided into pages. Each page is a fixed–size array of index entries defined by `MAX_INDEX_ENTRIES` (default is 256). Only one page is loaded into RAM at any time, which keeps memory usage low.

- **Memory Usage Calculation:**  
  ```cpp
  Memory per page = MAX_INDEX_ENTRIES × sizeof(IndexEntry)
  ```
  Adjust `MAX_INDEX_ENTRIES` based on your system’s memory limits.

### Choosing the Right Page Size

- **Smaller Pages:**  
  - **Advantages:** Lower RAM consumption.
  - **Disadvantages:** More frequent page loads and flushes, increasing file I/O (open, seek, read/write) and potentially reducing performance.

- **Larger Pages:**  
  - **Advantages:** Fewer page switches result in lower I/O overhead during searches and insertions.
  - **Disadvantages:** Requires more RAM per page, which might be a problem on highly memory–constrained devices.

**Tip:**  
Experiment with different values for `MAX_INDEX_ENTRIES`. Use the provided instrumentation (via `SCOPE_TIMER`) to measure execution times in functions such as `loadIndexPage` and `flushIndexPage`. This data will help you balance memory usage with I/O performance on your target platform.

---

## Key Capabilities

- **Record Appending:**  
  The **`append`** function adds a new record to the log file. It checks for duplicate keys, writes the record header and payload, and creates a new index entry.

- **Record Retrieval:**  
  The **`get`** function fetches a record by key. It reads both the log entry header and payload from the log file based on the offset provided by the index.

- **Record Updating:**  
  **`updateStatus`** updates a record’s status (both in the log file and the in–memory index) using its global index position.

- **Record Deletion:**  
  **`deleteRecord`** marks a record as deleted by setting an internal flag. Future insertions with the same key will update the existing index entry.

- **Efficient Index Searching:**  
  Several functions enable fast lookups:
  - **`findKey`** and **`locateKey`** for binary/B–tree–style searches.
  - **`nextKey`** and **`prevKey`** for sequential access.
  - **`searchIndex`** to locate an exact key.

- **Index Record Filtering and Counting:**  
  **New Functions:**  
  To provide more granular control over index entries based on internal status flags, DBEngine now includes:
  - **`getFirstMatchingIndexEntry(uint8_t mustBeSet, uint8_t mustBeClear, IndexEntry &entry, uint32_t &indexPosition)`**  
    Returns the first index entry where all bits in `mustBeSet` are present and all bits in `mustBeClear` are absent in the entry’s `internal_status`.
  - **`getFirstActiveIndexEntry(IndexEntry &entry, uint32_t &indexPosition)`**  
    A convenience wrapper that returns the first index entry not marked as deleted (i.e. with the `INTERNAL_STATUS_DELETED` flag clear).
  - **`getFirstDeletedIndexEntry(IndexEntry &entry, uint32_t &indexPosition)`**  
    A convenience wrapper that returns the first index entry marked as deleted (i.e. with the `INTERNAL_STATUS_DELETED` flag set).
  - **`recordCount(uint8_t internalStatus)` and `recordCount(uint8_t mustBeSet, uint8_t mustBeClear)`**  
    These overloads count index entries based on internal status criteria. For example, calling `recordCount(0, INTERNAL_STATUS_DELETED)` returns the number of active records (those without the deletion flag), and `recordCount(INTERNAL_STATUS_DELETED)` returns the number of deleted records.

- **Instrumentation:**  
  Scoped timers (via the `SCOPE_TIMER` macro) measure execution times of key operations, aiding performance tuning and configuration optimization.

---

## Instrumentation & Debugging Configuration

- **Scope Timer (SCOPE_TIMER):**  
  The `SCOPE_TIMER` macro is used for measuring the execution time of functions during Windows development and testing. For embedded devices, **this feature is disabled** (compiled out) to avoid overhead.  
  - **For Windows development/testing:**  
    ```cpp
    #define SCOPE_TIMER(name) ScopedTimer timer(name)
    ```
  - **For embedded targets (production):**  
    ```cpp
    #define SCOPE_TIMER(name)
    ```
    
- **Debug Output (DEBUG_PRINT):**  
  Similarly, debug print statements are enabled for testing (if uncommented) but are compiled out in embedded builds.  
  - **For testing:**  
    ```cpp
    //#define DEBUG_PRINT(...) printf(__VA_ARGS__)
    #define DEBUG_PRINT(...)
    ```
  - **For embedded production:**  
    Debug prints remain disabled:
    ```cpp
    #define DEBUG_PRINT(...)
    ```

These configurations ensure that instrumentation and debug output do not affect performance or resource usage on embedded devices.

---

## Design Philosophy

- **Lightweight & Fast:**  
  DBEngine has no STL dependencies and does not use dynamic memory allocation. It is optimized for embedded systems where fast reads and low overhead on writes/updates are critical.

- **Portability & Modularity:**  
  With the abstract `IFileHandler` interface, DBEngine is easy to port. Implement the interface for your target platform—whether you’re logging to an SD card using FatFs, onboard flash memory, or even a desktop system (using `WindowsFileHandler`).

- **Robustness:**  
  Multiple levels of duplicate key checking and index validation ensure data integrity and consistency.

- **Scalability:**  
  The paged index system enables support for large datasets while keeping the in–RAM index footprint minimal.

---

## Project Structure

### DBEngine

The `DBEngine` class is responsible for all database operations, including log and index file management.

#### Main Functions

- **`open`**  
  Opens the log and index files, validates or creates file headers, resets internal state, and automatically loads the first index page.

- **`append`**  
  Adds a new record after checking for duplicates, then creates a corresponding index entry. (Internally, helper functions such as `insertIndexEntry` and `splitPageAndInsert` are used.)

- **`updateStatus`**  
  Changes the status of a record using its global index position.

- **`get`**  
  Retrieves a record by its key, reading both the header and payload.

- **`deleteRecord`**  
  Marks a record as deleted so that later calls to `append` with the same key update the existing entry.

- **Index Paging Functions:**  
  Functions like `loadIndexPage`, `flushIndexPage`, `getIndexEntry`, and `setIndexEntry` manage the in–memory index page and synchronize it with the disk.

- **B–Tree–Style Search Methods:**  
  Functions such as `findKey`, `locateKey`, `nextKey`, `prevKey`, and `searchIndex` provide efficient lookups.

- **Index Record Filtering and Counting:**  
  In addition to the standard operations, DBEngine now provides additional functions for filtering and counting index entries based on internal status flags:
  - **`getFirstMatchingIndexEntry()`**
  - **`getFirstActiveIndexEntry()`**
  - **`getFirstDeletedIndexEntry()`**
  - **`recordCount()` overloads  
  These functions allow you to quickly determine how many records are active or deleted and to retrieve the first entry that meets your filtering criteria.

- **Other Helpers:**  
  Functions like `findByStatus`, `indexCount`, and `printStats` allow you to monitor and inspect the database.

### IFileHandler

`IFileHandler` is an abstract interface for file operations. It requires implementations for:

- **`open`** – Open a file in a specified mode (e.g., "rb+", "wb+").
- **`close`** – Close the file.
- **`seek` / `seekToEnd`** – Move the file pointer.
- **`tell`** – Return the current file position.
- **`read` / `write`** – Read from or write to the file.

Implement this interface for your target platform’s storage (e.g., SD card, flash memory, etc.).

### Instrumentation

DBEngine uses the `Instrumentation.h` header and `SCOPE_TIMER` macros to time critical functions. This instrumentation is useful for:
- Measuring the impact of different paging sizes.
- Identifying performance bottlenecks.
- Ensuring that your configuration meets your application’s requirements.

Remember, on embedded devices these features are compiled out to reduce overhead.

---

## Debugging & Test Considerations

- **Testing with Sequential Keys:**  
  The test suite uses sequential key values. Running tests multiple times without resetting the files may cause duplicate key errors since DBEngine only accepts unique keys.

- **Resetting the Test Environment:**  
  For testing, delete the existing log and index files before running tests. This guarantees that each test run starts fresh. (See the example in the test main function below.)

- **Troubleshooting Tips:**  
  - If duplicate key errors occur, verify that old files have been removed.
  - Use instrumentation output to determine if page switches are causing performance issues.

---

## Final Notes

- **Tuning Memory vs. I/O:**  
  Adjust `MAX_INDEX_ENTRIES` based on your system’s memory and performance characteristics. Smaller pages reduce RAM usage but increase I/O overhead, while larger pages improve I/O performance at the cost of higher memory usage.

- **Instrumentation:**  
  Use the provided instrumentation (via `SCOPE_TIMER`) to measure the performance of key functions. Remember that on embedded devices the scope timer and debug prints are compiled out to avoid unnecessary overhead.

- **Portability:**  
  If you’re not using Windows, implement your own version of `IFileHandler` to interface with your target device’s storage (e.g., FatFs for SD cards, custom drivers for onboard flash, etc.).

By following these guidelines and using the detailed examples provided, you’ll be well equipped to integrate, port, and fine–tune DBEngine for your embedded application.

---

Happy coding!
