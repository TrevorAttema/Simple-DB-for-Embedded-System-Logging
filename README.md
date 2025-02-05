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
   Append, retrieve, update, or delete records, and use the provided search functions to access data quickly.

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

Below is a complete, tidied set of usage examples written in C++ without using the STL. The examples are divided into two sections—**Basic Examples** (for the most common operations) and **Advanced Examples** (covering more complex operations such as B–tree–style searches and deletion). These examples rely only on C standard libraries (e.g., `<stdio.h>`, `<stdlib.h>`, `<string.h>`) along with the DBEngine interface. They also assume you’re using the Windows implementation of `IFileHandler` (from `FileHandler_Windows.h`) for testing; when porting to your embedded device, replace this with your own implementation (for example, one that wraps FatFs functions). Remember that on embedded devices the scope timer and debug prints are compiled out to reduce overhead.

---

## Basic Examples

### Example 1: Resetting Files and Opening the Database

This example shows how to delete any existing database files (to ensure a fresh start) and open the database. The `open()` call automatically loads the index.

```cpp
#include "DBEngine.h"
#include "FileHandler_Windows.h"  // Your Windows implementation of IFileHandler
#include <stdio.h>    // For printf() and remove()
#include <stdlib.h>

int main(void) {
    printf("Starting DBEngine Example: Open Database\n");

    // Delete existing database files so that tests start fresh.
    if (remove("logfile.db") == 0) {
        printf("Deleted existing logfile.db\n");
    }
    if (remove("indexfile.idx") == 0) {
        printf("Deleted existing indexfile.idx\n");
    }

    // Instantiate file handlers.
    WindowsFileHandler logHandler;
    WindowsFileHandler indexHandler;

    // Create and open the DBEngine.
    DBEngine db(logHandler, indexHandler);
    if (!db.open("logfile.db", "indexfile.idx")) {
        printf("Error opening database files.\n");
        return 1;
    }
    printf("Database opened successfully (index loaded automatically).\n");
    return 0;
}
```

---

### Example 2: Appending Records and Verifying the Index Count

This example appends several records to the database and checks that the index count increases as expected.

```cpp
#include "DBEngine.h"
#include "FileHandler_Windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define a sample record structure.
struct TemperatureRecord {
    float temperature;
    float humidity;
    unsigned int height;
    unsigned int width;
    char name[100];
};

int main(void) {
    printf("Starting DBEngine Example: Append Records\n");

    // Remove existing files.
    remove("logfile.db");
    remove("indexfile.idx");

    // Initialize DBEngine.
    WindowsFileHandler logHandler;
    WindowsFileHandler indexHandler;
    DBEngine db(logHandler, indexHandler);
    if (!db.open("logfile.db", "indexfile.idx")) {
        printf("Error opening database files.\n");
        return 1;
    }

    TemperatureRecord rec;
    rec.temperature = 23.5f;
    rec.humidity = 45.0f;
    rec.height = 1;
    rec.width = 2;
    strcpy(rec.name, "Temperature data");

    int success = 1;
    unsigned int i;
    for (i = 0; i < 10; i++) {
        rec.temperature += 0.1f;
        rec.humidity += 0.05f;
        unsigned int key = i + 1;  // Use sequential keys starting at 1.
        if (!db.append(key, 1, &rec, sizeof(rec))) {
            printf("Failed appending record with key %u\n", key);
            success = 0;
            break;
        }
        if (db.indexCount() != i + 1) {
            printf("Index count mismatch after inserting key %u\n", key);
            success = 0;
            break;
        }
    }

    if (success)
        printf("All records appended successfully.\n");
    else
        printf("Error during appending records.\n");

    return success ? 0 : 1;
}
```

---

### Example 3: Retrieving a Record by Key

This example appends a record and then retrieves it using its key.

```cpp
#include "DBEngine.h"
#include "FileHandler_Windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define a sample record structure.
struct TemperatureRecord {
    float temperature;
    float humidity;
    unsigned int height;
    unsigned int width;
    char name[100];
};

int main(void) {
    printf("Starting DBEngine Example: Retrieve Record\n");

    // Remove existing files.
    remove("logfile.db");
    remove("indexfile.idx");

    // Initialize and open DBEngine.
    WindowsFileHandler logHandler;
    WindowsFileHandler indexHandler;
    DBEngine db(logHandler, indexHandler);
    if (!db.open("logfile.db", "indexfile.idx")) {
        printf("Error opening database files.\n");
        return 1;
    }

    // Append a record to retrieve.
    TemperatureRecord rec;
    rec.temperature = 25.0f;
    rec.humidity = 50.0f;
    rec.height = 10;
    rec.width = 20;
    strcpy(rec.name, "Record for Retrieval");
    unsigned int key = 123;
    if (!db.append(key, 1, &rec, sizeof(rec))) {
        printf("Failed appending record.\n");
        return 1;
    }

    // Retrieve the record by key.
    TemperatureRecord retrieved;
    unsigned short recordSize = 0;
    if (!db.get(key, &retrieved, sizeof(retrieved), &recordSize)) {
        printf("Failed to retrieve record with key %u\n", key);
        return 1;
    }

    if (recordSize == sizeof(retrieved))
        printf("Record retrieved successfully: %s\n", retrieved.name);
    else
        printf("Record size mismatch.\n");

    return 0;
}
```

---

### Example 4: Updating a Record's Status (Custom Status Bits)

This example demonstrates how to update a record's status. For instance, you might flag a record as "processed" by setting a custom status bit.

```cpp
#include "DBEngine.h"
#include "FileHandler_Windows.h"
#include <stdio.h>
#include <stdlib.h>

#define STATUS_PROCESSED 0x02  // Example custom status: record has been processed.

int main(void) {
    printf("Starting DBEngine Example: Update Record Status\n");

    // Remove existing files.
    remove("logfile.db");
    remove("indexfile.idx");

    // Initialize DBEngine.
    WindowsFileHandler logHandler;
    WindowsFileHandler indexHandler;
    DBEngine db(logHandler, indexHandler);
    if (!db.open("logfile.db", "indexfile.idx")) {
        printf("Error opening database files.\n");
        return 1;
    }

    // Append a record.
    unsigned int key = 123;
    int dummyData = 42;
    if (!db.append(key, 1, &dummyData, sizeof(dummyData))) {
        printf("Failed to append record.\n");
        return 1;
    }

    // Locate the record using searchIndex.
    unsigned int index = 0;
    if (!db.searchIndex(key, &index)) {
        printf("Record with key %u not found.\n", key);
        return 1;
    }

    // Update the record's status to indicate it has been processed.
    if (!db.updateStatus(index, STATUS_PROCESSED)) {
        printf("Failed to update status for record with key %u\n", key);
        return 1;
    }

    printf("Record status updated successfully for key %u\n", key);
    return 0;
}
```

---

## Advanced Examples

### Example 5: B–Tree–Style Search (Finding and Locating Keys)

This example shows how to use DBEngine’s B–tree–style search methods to find and locate keys in the database.

```cpp
#include "DBEngine.h"
#include "FileHandler_Windows.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Starting DBEngine Example: B-Tree Search\n");

    // Remove existing files.
    remove("logfile.db");
    remove("indexfile.idx");

    // Initialize DBEngine.
    WindowsFileHandler logHandler;
    WindowsFileHandler indexHandler;
    DBEngine db(logHandler, indexHandler);
    if (!db.open("logfile.db", "indexfile.idx")) {
        printf("Error opening database files.\n");
        return 1;
    }

    // Append several records with sequential keys.
    unsigned int key;
    int data;
    for (key = 1; key <= 10; key++) {
        data = key * 100;  // Simple integer data.
        if (!db.append(key, 1, &data, sizeof(data))) {
            printf("Failed appending record with key %u\n", key);
            return 1;
        }
    }

    // Use findKey to search for an existing key.
    unsigned int searchKey = 5;
    unsigned int foundIndex = 0;
    if (db.findKey(searchKey, &foundIndex))
        printf("findKey succeeded: Key %u found at index %u\n", searchKey, foundIndex);
    else
        printf("findKey failed: Key %u not found.\n", searchKey);

    // Use locateKey to locate the position of an existing key.
    unsigned int locatedIndex = 0;
    if (db.locateKey(searchKey, &locatedIndex))
        printf("locateKey succeeded: Key %u located at index %u\n", searchKey, locatedIndex);
    else
        printf("locateKey failed: Key %u not located.\n", searchKey);

    // Attempt to search for a non-existent key.
    unsigned int missingKey = 999;
    if (!db.findKey(missingKey, &foundIndex))
        printf("Correctly did not find non-existent key %u\n", missingKey);
    else
        printf("Error: Unexpectedly found non-existent key %u\n", missingKey);

    return 0;
}
```

---

### Example 6: Deleting Records

This advanced example demonstrates several deletion scenarios:
- Deleting a non-existent key.
- Deleting the first record.
- Deleting arbitrary records.
- Reinserting a record with a deleted key to verify that the deletion flag is cleared.

```cpp
#include "DBEngine.h"
#include "FileHandler_Windows.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Starting DBEngine Example: Delete Records\n");

    // Remove existing files.
    remove("logfile.db");
    remove("indexfile.idx");

    // Initialize DBEngine.
    WindowsFileHandler logHandler;
    WindowsFileHandler indexHandler;
    DBEngine db(logHandler, indexHandler);
    if (!db.open("logfile.db", "indexfile.idx")) {
        printf("Error opening database files.\n");
        return 1;
    }

    // Append records with keys 1 to 20.
    unsigned int key;
    int data;
    for (key = 1; key <= 20; key++) {
        data = key;
        if (!db.append(key, 1, &data, sizeof(data))) {
            printf("Failed appending record with key %u\n", key);
            return 1;
        }
    }
    printf("20 records appended.\n");

    // Delete a non-existent key.
    unsigned int nonExistentKey = 9999;
    if (!db.deleteRecord(nonExistentKey))
        printf("Correctly did not delete non-existent key %u\n", nonExistentKey);
    else
        printf("Error: Deleted non-existent key %u\n", nonExistentKey);

    // Delete the first record.
    if (db.deleteRecord(1))
        printf("First record (key 1) deleted successfully.\n");
    else
        printf("Failed to delete first record (key 1).\n");

    // Delete a few arbitrary records.
    unsigned int keysToDelete[3] = { 5, 10, 15 };
    int i;
    for (i = 0; i < 3; i++) {
        if (db.deleteRecord(keysToDelete[i]))
            printf("Record with key %u deleted.\n", keysToDelete[i]);
        else
            printf("Failed to delete record with key %u\n", keysToDelete[i]);
    }

    // Reinsert a record with a deleted key to verify that the deletion flag is cleared.
    int newData = 555;
    unsigned int reinsertKey = 5; // previously deleted key.
    if (db.append(reinsertKey, 1, &newData, sizeof(newData)))
        printf("Record reinserted with key %u\n", reinsertKey);
    else
        printf("Failed to reinsert record with key %u\n", reinsertKey);

    return 0;
}
```

---

## Example Notes

- **No STL:** All examples are written in C++ using only C standard libraries (`stdio.h`, `stdlib.h`, `string.h`).  
- **Portability:** For your embedded target, implement your own version of `IFileHandler` (for example, wrapping FatFs functions) and compile out the scope timer and debug prints as described in the instrumentation configuration.  
- **Sequence:** The basic examples introduce the core operations (opening the database, appending records, retrieving records, and updating status), while the advanced examples cover searching and deletion.

By following these examples, you should be able to quickly learn how to use DBEngine for logging, retrieval, status updates, search operations, and deletion in your applications.

## Final Notes

- **Tuning Memory vs. I/O:**  
  Adjust `MAX_INDEX_ENTRIES` based on your system’s memory and performance characteristics. Smaller pages reduce RAM usage but increase I/O overhead, while larger pages improve I/O performance at the cost of higher memory usage.

- **Instrumentation:**  
  Use the provided instrumentation (via `SCOPE_TIMER`) to measure the performance of key functions. Remember that on embedded devices, the scope timer and debug prints are compiled out to avoid unnecessary overhead.

- **Portability:**  
  If you’re not using Windows, implement your own version of `IFileHandler` to interface with your target device’s storage (e.g., FatFs for SD cards, custom drivers for onboard flash, etc.).

By following these guidelines and using the detailed examples provided, you’ll be well equipped to integrate, port, and fine–tune DBEngine for your embedded application.

---

Happy coding!
