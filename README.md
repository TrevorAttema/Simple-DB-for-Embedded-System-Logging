Below is the revised README file with an added example demonstrating how to set custom status bits for a record. This extra example shows how a developer can use the status field to mark a record as, for example, “processed” or flag it for further action.

---

# Yet Another Key Value DB for Embedded Systems

**DBEngine** is a lightweight, high–performance key–value database engine written in C++ that is ideally suited for embedded systems. Whether you need to log sensor data to an SD card, store event logs in onboard flash memory, or efficiently capture and later process large volumes of data, DBEngine provides a simple yet robust solution with minimal resource overhead.

*Key real–world examples include:*
- **Data Logging:** Continuously log temperature, humidity, or other sensor data to an SD card.
- **Event Recording:** Maintain a log of system events or error reports in onboard flash.
- **Data Collection:** Efficiently capture high–frequency measurements and later process them using the sorted index.

---

## Overview

DBEngine uses a log–and–index storage strategy:
- **Log File:** Records are appended sequentially.
- **Index File:** A sorted index maps record keys to their offsets in the log file. This index is managed in fixed–size pages to minimize memory usage.

When you call **`open`**, the engine automatically opens both the log and index files, verifies or creates file headers, and loads the first index page into memory. This single call sets up the database for subsequent operations.

---

## Memory Consumption & Paging Configuration

### How the Paged Index Works

- **Index in Pages:**  
  The index is split into pages, with each page holding a fixed number of entries defined by `MAX_INDEX_ENTRIES` (default is 256). Only one page is loaded into RAM at a time, which keeps the memory footprint low.

- **Memory Usage Calculation:**  
  The memory used per page is approximately:  
  ```
  Memory per page = MAX_INDEX_ENTRIES × sizeof(IndexEntry)
  ```
  You can adjust `MAX_INDEX_ENTRIES` to suit your system’s memory limits.

### Choosing the Right Page Size

- **Smaller Pages:**  
  - **Advantages:** Uses less RAM.  
  - **Disadvantages:** If operations span multiple pages, the engine must load and flush pages more frequently. Each page switch incurs additional file I/O (open, seek, read/write), which can slow down performance.

- **Larger Pages:**  
  - **Advantages:** Fewer page switches mean less I/O overhead during searches and insertions.  
  - **Disadvantages:** Requires more RAM per page, which might not be acceptable on systems with extremely limited memory.

**Tip:** Experiment with different `MAX_INDEX_ENTRIES` settings. Use the built–in instrumentation (via the `SCOPE_TIMER` macro) to measure performance—especially in functions like `loadIndexPage` and `flushIndexPage`—and choose the best balance for your application.

---

## Key Capabilities

- **Record Appending:**  
  The **`append`** function adds a new record to the log file. It automatically checks for duplicate keys, writes the record header and payload, and creates a new index entry.

- **Record Retrieval:**  
  The **`get`** function fetches a record by key. It reads the log entry header and payload from the log file using the offset provided by the index.

- **Record Updating:**  
  Use **`updateStatus`** to change a record’s status by its index position. This update is applied both to the log file and to the in–memory index.

- **Record Deletion:**  
  **`deleteRecord`** marks a record as deleted by setting an internal flag. Future calls to **`append`** with the same key will reuse the existing index entry.

- **Efficient Index Searching:**  
  The engine provides several search methods:
  - **`findKey`** and **`locateKey`** perform binary and B–tree–style searches.
  - **`nextKey`** and **`prevKey`** enable easy sequential access.
  - **`searchIndex`** locates an exact key.

- **Instrumentation:**  
  Using scoped timers (via the `SCOPE_TIMER` macro), DBEngine measures execution times of key operations. This helps you tune the system by understanding where performance bottlenecks are.

---

## Design Philosophy

- **Lightweight & Fast:**  
  No STL dependencies and no dynamic memory allocation make DBEngine ideal for systems with limited resources. It is designed for fast reads and low overhead on writes and updates.

- **Portability & Modularity:**  
  The abstract `IFileHandler` interface isolates file I/O from database logic. You can implement this interface for any platform—whether logging to an SD card, onboard flash, or a host system like Windows (using `WindowsFileHandler`).

- **Robustness:**  
  Multiple levels of key–duplication checks and index validation routines ensure that your log and index stay consistent and error–free.

- **Scalability:**  
  The paged index system allows you to support very large datasets while keeping memory usage low.

---

## Project Structure

### DBEngine

The `DBEngine` class manages all operations on the log and index files.

#### Main Functions

- **`open`**  
  Opens the log and index files, creates or validates file headers, resets internal state, and automatically loads the index.

- **`append`**  
  Adds a new record to the log file after checking for duplicates, then inserts the corresponding index entry. (Internally, it may use helper functions such as `insertIndexEntry` and `splitPageAndInsert`.)

- **`updateStatus`**  
  Updates the status of a record based on its global index position.

- **`get`**  
  Retrieves a record by key, reading both the header and payload from the log file.

- **`deleteRecord`**  
  Marks a record as deleted so that future insertions with the same key update the existing entry.

- **Index Paging Functions:**  
  Functions like `loadIndexPage`, `flushIndexPage`, `getIndexEntry`, and `setIndexEntry` manage the in–memory index page and ensure it is synchronized with the disk.

- **B–Tree–Style Search Methods:**  
  Methods such as `findKey`, `locateKey`, `nextKey`, `prevKey`, and `searchIndex` enable efficient lookups in the sorted index.

- **Other Helpers:**  
  Functions like `findByStatus`, `indexCount`, and `printStats` allow you to inspect and monitor the database.

### IFileHandler

`IFileHandler` is an abstract interface defining file operations:
- **`open`** – Open a file in a specified mode (e.g., "rb+", "wb+").
- **`close`** – Close the file.
- **`seek` / `seekToEnd`** – Move the file pointer.
- **`tell`** – Return the current file position.
- **`read` / `write`** – Read from or write to a file.

Implement this interface for your target system’s storage medium (such as an SD card or flash memory).

### Instrumentation

The engine uses the `Instrumentation.h` header and `SCOPE_TIMER` macros to time critical operations. This helps you understand the performance impact of your chosen page size and other configuration settings.

---

## Simple Usage Examples

### Example 1: Opening the Database

```cpp
#include "DBEngine.h"
#include "WindowsFileHandler.h" // Or use your custom IFileHandler implementation

int main() {
    WindowsFileHandler logHandler;
    WindowsFileHandler indexHandler;

    // Create the DBEngine instance with your file handlers.
    DBEngine dbEngine(logHandler, indexHandler);

    // Open the database with the specified log and index filenames.
    // This call automatically loads the index.
    if (!dbEngine.open("logfile.db", "indexfile.idx")) {
        printf("Failed to open DBEngine.\n");
        return -1;
    }

    printf("Database opened successfully (index loaded automatically).\n");
    return 0;
}
```

### Example 2: Appending a Record

```cpp
#include <cstdint>
#include <cstdio>
#include "DBEngine.h"

// Define a sample record structure.
struct MyRecord {
    uint32_t id;
    char data[64];
};

int main() {
    // Assume that DBEngine has been initialized and opened as shown in Example 1.
    DBEngine dbEngine(/* appropriate file handlers */);

    MyRecord record = { 1234, "Hello, world!" };

    // Append the record to the database.
    if (!dbEngine.append(record.id, /* recordType */ 0, &record, sizeof(record))) {
        printf("Failed to append record.\n");
        return -1;
    }

    printf("Record appended successfully.\n");
    return 0;
}
```

### Example 3: Retrieving a Record by Key

```cpp
#include <cstdint>
#include <cstdio>
#include "DBEngine.h"

struct MyRecord {
    uint32_t id;
    char data[64];
};

int main() {
    // Assume that DBEngine has been initialized and opened.
    DBEngine dbEngine(/* appropriate file handlers */);
    
    uint32_t keyToFind = 1234;
    MyRecord record;
    uint16_t recordSize = 0;

    if (!dbEngine.get(keyToFind, &record, sizeof(record), &recordSize)) {
        printf("Record with key %u not found.\n", keyToFind);
        return -1;
    }

    printf("Record found (size: %u bytes): %s\n", recordSize, record.data);
    return 0;
}
```

### Example 4: Updating a Record's Status (Custom Status Bits)

Sometimes you might want to use the status field to flag a record for your own purposes—such as marking it as “processed” or indicating another application–specific state. The following example shows how to update a record’s status using a custom status bit (for example, `0x02` to indicate that the record has been processed):

```cpp
#include <cstdint>
#include <cstdio>
#include "DBEngine.h"

int main() {
    // Assume that DBEngine has been initialized and opened.
    DBEngine dbEngine(/* appropriate file handlers */);
    
    uint32_t keyToUpdate = 1234;
    uint32_t index = 0;
    
    // Locate the record in the index.
    if (!dbEngine.searchIndex(keyToUpdate, &index)) {
        printf("Record with key %u not found.\n", keyToUpdate);
        return -1;
    }
    
    // Define a custom status value. For example, 0x02 might indicate "processed."
    uint8_t customStatus = 0x02;
    
    // Update the record's status using its global index.
    if (!dbEngine.updateStatus(index, customStatus)) {
        printf("Failed to update status for record with key %u.\n", keyToUpdate);
        return -1;
    }
    
    printf("Record status updated successfully for key %u.\n", keyToUpdate);
    return 0;
}
```

---

## Final Notes

- **Tuning Memory vs. I/O:**  
  Adjust `MAX_INDEX_ENTRIES` based on your device’s memory limits and I/O capabilities. Smaller pages use less RAM but may lead to more frequent disk accesses; larger pages reduce I/O overhead but require more memory.

- **Instrumentation:**  
  Use the provided instrumentation (via `SCOPE_TIMER`) to monitor execution times and optimize performance. In production builds, you can disable debug prints to minimize overhead.

- **Portability:**  
  If you are not using Windows, implement your own version of `IFileHandler` for your target platform (e.g., for SD cards or onboard flash).

By following these guidelines and using the examples provided, you’ll be well equipped to integrate and fine–tune DBEngine for your embedded application.

---

Happy coding!