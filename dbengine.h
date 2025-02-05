#ifndef DBENGINE_H
#define DBENGINE_H

#include "IFileHandler.h"  // The abstract file I/O interface
#include <cstdint>
#include <cstddef>
#include <string.h>
#include <limits>

#define MAX_INDEX_ENTRIES 256
#define MAX_FILENAME_LENGTH 13  // For 8.3 filenames

#define DB_MAGIC_NUMBER     0x53474F4C  // "LOGS" in little-endian hex
#define DB_VERSION          0x0001
#define DB_IDX_VERSION      0x0001

// Define a macro to wrap a ScopedTimer creation.
// Usage: SCOPE_TIMER("functionName");
#define SCOPE_TIMER(name) ScopedTimer timer(name)
//#define SCOPE_TIMER(name)

// For debugging: define a macro that prints debug messages.
//#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#define DEBUG_PRINT(...)

//
// --- DB Header ---
// This header holds metadata for the log file.
#pragma pack(push, 1)
struct DBHeader {
    uint32_t magic;    // Magic number ("LOGS")
    uint16_t version;  // Log file format version
};
#pragma pack(pop)

//
// --- DB Index Header ---
// This header holds metadata for the index file, including the index count.
#pragma pack(push, 1)
struct DBIndexHeader {
    uint32_t magic;      // Magic number ("LOGS")
    uint16_t version;    // Index file format version
    uint32_t indexCount; // Number of index entries stored in the file
};
#pragma pack(pop)

//
// --- Log Entry Header ---
// Represents metadata for a log entry in the database.
#pragma pack(push, 1)
struct LogEntryHeader {
    uint8_t  recordType;   // Identifier for the record type
    uint16_t length;       // Payload length in bytes
    uint32_t key;          // Key value supplied by the caller
    uint8_t  status;       // Implementation-specific status
};
#pragma pack(pop)

//
// --- Index Entry ---
// Represents an entry in the index file, linking keys to their offsets in the log file.
#pragma pack(push, 1)
struct IndexEntry {
    uint32_t key;      // Record's key value
    uint32_t offset;   // File offset in the log file
    uint8_t  status;   // Record status
};
#pragma pack(pop)

//
// --- DBEngine Class ---
// Implements a key-value database with an indexed log-based storage system.
// Public function names have been simplified and key management is done externally.
class DBEngine {
public:
    /**
     * @brief Constructor for the database engine.
     * @param logHandler Reference to a file handler for log file operations.
     * @param indexHandler Reference to a file handler for index file operations.
     */
    DBEngine(IFileHandler& logHandler, IFileHandler& indexHandler);

    /**
     * @brief Opens the database files (log and index) for future operations.
     *
     * @param logFileName Name of the log file.
     * @param indexFileName Name of the index file.
     * @return True if the files were opened and validated (or created), false otherwise.
     */
    bool open(const char logFileName[MAX_FILENAME_LENGTH],
        const char indexFileName[MAX_FILENAME_LENGTH]);



    /**
     * @brief Appends a new record to the log file and creates an index entry.
     *
     * The caller supplies the key and the record payload.
     * The engine checks for key collisions before appending.
     *
     * @param key User-supplied key for the record.
     * @param recordType Type identifier for the record.
     * @param record Pointer to the record data.
     * @param recordSize Size of the record in bytes.
     * @return True if the record was successfully appended, false on failure.
     */
    bool append(uint32_t key, uint8_t recordType, const void* record, uint16_t recordSize);

    /**
     * @brief Updates the status of a record given its index (position in the index file).
     *
     * @param indexId The global index position of the record.
     * @param newStatus The new status value.
     * @return True if the update was successful, false otherwise.
     */
    bool updateStatus(uint32_t indexId, uint8_t newStatus);

    /**
     * @brief Retrieves a record from the log file given its key.
     *
     * @param key The key of the record to retrieve.
     * @param header Reference to store the log entry header.
     * @param payloadBuffer Buffer to store the payload data.
     * @param bufferSize Size of the buffer.
     * @return True if retrieval was successful, false otherwise.
     */
    bool get(uint32_t key, void* payloadBuffer, uint16_t bufferSize, uint16_t* outRecordSize = nullptr);

    /**
     * @brief Finds records with a specific status.
     *
     * @param status The status value to search for.
     * @param results Array to store the matching index positions.
     * @param maxResults Maximum number of results to store.
     * @return The number of records found.
     */
    size_t findByStatus(uint8_t status, uint32_t results[], size_t maxResults) const;

    /**
     * @brief Returns the total number of index entries.
     */
    size_t indexCount(void) const;

    /**
     * @brief Retrieves an index entry by its global index position.
     *
     * @param globalIndex The position of the index entry.
     * @param entry Reference to store the index entry.
     * @return True if the index entry was successfully retrieved, false otherwise.
     */
    bool getIndexEntry(uint32_t globalIndex, IndexEntry& entry);

    /**
     * @brief Deletes a record by key by marking it as deleted.
     *        This function reuses the index entry so that a later insertion
     *        using the same key will update the existing record.
     *
     * @param key The key of the record to mark as deleted.
     * @return True if the record was successfully marked deleted, false otherwise.
     */
    //bool deleteRecord(uint32_t key);

    /**
     * @brief Returns the database file format version.
     */
    uint16_t dbVersion(void) const;

    // --- B-Tree–Style Search Methods ---
    bool findKey(uint32_t key, uint32_t* index);
    bool locateKey(uint32_t key, uint32_t* index);
    bool nextKey(uint32_t currentIndex, uint32_t* nextIndex);
    bool prevKey(uint32_t currentIndex, uint32_t* prevIndex);

    void printStats(void) const;

private:
    char _logFileName[MAX_FILENAME_LENGTH];   // Log file name.
    char _indexFileName[MAX_FILENAME_LENGTH];   // Index file name.

    uint32_t _indexCount;  // Total number of index entries.

    // --- Index Paging ---
    IndexEntry _indexPage[MAX_INDEX_ENTRIES]; // Current index page in memory.
    uint32_t _currentPageNumber;              // Currently loaded page number.
    bool _pageLoaded;                         // Flag to indicate if a page is loaded.
    bool _pageDirty;                          // Flag to indicate if a page has been modified.

    IFileHandler& _logHandler;   // File handler for log operations.
    IFileHandler& _indexHandler; // File handler for index operations.

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

    // --- Database Header Functions for the Index File ---
    bool saveDBHeader(void);
    bool loadDBHeader(void);

    /**
     * @brief Builds or loads the in-memory index from the index file.
     *
     * @return True if the index was successfully loaded or built, false otherwise.
     */
    bool loadIndex(void);

    // Internal storage for DB header metadata.
    DBHeader _dbHeader;
};

#endif // DBENGINE_H
