#ifndef DBENGINE_H
#define DBENGINE_H

#include "IFileHandler.h"  // The abstract file I/O interface
#include <cstdint>
#include <stddef.h>
#include <string.h>
#include <limits>

#define MAX_INDEX_ENTRIES 256
#define MAX_FILENAME_LENGTH 13  // For 8.3 filenames

// Define a macro to wrap a ScopedTimer creation.
// Usage: SCOPE_TIMER("functionName");
#define SCOPE_TIMER(name) ScopedTimer timer(name)
//#define SCOPE_TIMER(name)

// For debugging: define a macro that prints debug messages.
//#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#define DEBUG_PRINT(...)

// --- Log Entry Header ---
// Represents metadata for a log entry in the database.
#pragma pack(push, 1)
struct LogEntryHeader {
    uint8_t  recordType;   // Identifier for the record type
    uint16_t length;       // Payload length in bytes
    uint32_t key;          // Key value (can be a timestamp, ID, etc.)
    uint8_t  status;       // Implementation-specific status
};
#pragma pack(pop)

// --- Index Entry ---
// Represents an entry in the index file, linking keys to their offsets in the log file.
#pragma pack(push, 1)
struct IndexEntry {
    uint32_t key;      // Record's key value
    uint32_t offset;   // File offset in the log file
    uint8_t  status;   // Record status
};
#pragma pack(pop)

// --- DBEngine Class ---
// Implements a key-value database with an indexed log-based storage system.
// Designed for embedded systems, it is optimized for fast reads and lightweight memory usage.
class DBEngine {
public:
    /**
     * @brief Constructor for the database engine.
     * @param logHandler Reference to a file handler for log file operations.
     * @param indexHandler Reference to a file handler for index file operations.
     */
    DBEngine(IFileHandler& logHandler, IFileHandler& indexHandler);

    /**
     * @brief Opens the database files for logging and indexing.
     * @param logFileName Name of the log file.
     * @param indexFileName Name of the index file.
     */
    void dbOpen(const char logFileName[MAX_FILENAME_LENGTH],
        const char indexFileName[MAX_FILENAME_LENGTH]);

    /**
     * @brief Builds or loads the in-memory index from the index file.
     * @return True if the index was successfully loaded or built, false otherwise.
     */
    bool dbBuildIndex(void);

    /**
     * @brief Appends a new record to the log file and adds an index entry.
     * @param recordType Type identifier for the record.
     * @param record Pointer to the record data.
     * @param recordSize Size of the record in bytes.
     * @return True if the record was successfully appended, false on failure.
     */
    bool dbAppendRecord(uint8_t recordType, const void* record, uint16_t recordSize);

    /**
     * @brief Updates the status of a record given its index ID.
     * @param indexId The global index ID of the record.
     * @param newStatus The new status value.
     * @return True if the update was successful, false otherwise.
     */
    bool dbUpdateRecordStatusByIndexId(uint32_t indexId, uint8_t newStatus);

    /**
     * @brief Retrieves a record from the log file given its index ID.
     * @param indexId The global index ID of the record.
     * @param header Reference to store the log entry header.
     * @param payloadBuffer Buffer to store the payload data.
     * @param bufferSize Size of the buffer.
     * @return True if retrieval was successful, false otherwise.
     */
    bool dbGetRecordByIndexId(uint32_t indexId, LogEntryHeader& header,
        void* payloadBuffer, uint16_t bufferSize);

    /**
     * @brief Retrieves a record by searching the index for the given key.
     * @param key The key of the record to retrieve.
     * @param header Reference to store the log entry header.
     * @param payloadBuffer Buffer to store the payload data.
     * @param bufferSize Size of the buffer.
     * @return True if retrieval was successful, false otherwise.
     */
    bool dbGetRecordByKey(uint32_t key, LogEntryHeader& header,
        void* payloadBuffer, uint16_t bufferSize);

    /**
     * @brief Finds records with a specific status.
     * @param status The status value to search for.
     * @param results Array to store the matching index positions.
     * @param maxResults Maximum number of results to store.
     * @return The number of records found.
     */
    size_t dbFindRecordsByStatus(uint8_t status, uint32_t results[], size_t maxResults) const;

    // --- B-Tree–Style Search Methods ---
    bool btreeFindKey(uint32_t key, uint32_t* index);
    bool btreeLocateKey(uint32_t key, uint32_t* index);
    bool btreeNextKey(uint32_t currentIndex, uint32_t* nextIndex);
    bool btreePrevKey(uint32_t currentIndex, uint32_t* prevIndex);

    size_t dbGetIndexCount(void) const;

    // Retrieves an index entry by its global index position.
    bool getIndexEntry(uint32_t globalIndex, IndexEntry& entry);

private:
    char _logFileName[MAX_FILENAME_LENGTH];  // Log file name.
    char _indexFileName[MAX_FILENAME_LENGTH];  // Index file name.

    uint32_t _indexCount;  // Total number of index entries.

    // --- Index Paging ---
    IndexEntry _indexPage[MAX_INDEX_ENTRIES]; // Current index page in memory.
    uint32_t _currentPageNumber;              // Currently loaded page number.
    bool _pageLoaded;                         // Flag to indicate if a page is loaded.
    bool _pageDirty;                          // Flag to indicate if a page has been modified.

    IFileHandler& _logHandler;  // File handler for log operations.
    IFileHandler& _indexHandler;  // File handler for index operations.

    // --- Internal Helper Functions ---
    bool flushIndexPage(void);
    bool loadIndexPage(uint32_t pageNumber);
    bool setIndexEntry(uint32_t globalIndex, const IndexEntry& entry);
    bool saveIndexHeader(void);
    bool loadIndexHeader(void);
    bool searchIndex(uint32_t key, uint32_t* foundIndex) const;
    bool findIndexEntry(uint32_t key, uint32_t& offset) const;
    bool insertIndexEntry(uint32_t key, uint32_t offset, uint8_t status);
    bool splitPageAndInsert(uint32_t targetPage, uint32_t offsetInPage,
        uint32_t key, uint32_t recordOffset, uint8_t status);
    bool validateIndex(void);
};

#endif // DBENGINE_H