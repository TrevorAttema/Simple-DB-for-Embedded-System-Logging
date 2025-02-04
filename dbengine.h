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
#pragma pack(push, 1)
struct LogEntryHeader {
    uint8_t  recordType;   // Identifier for the record type
    uint16_t length;       // Payload length in bytes
    uint32_t key;          // Key value (can be a timestamp, ID, etc.)
    uint8_t  status;       // Implementation-specific status
};
#pragma pack(pop)

// --- Index Entry ---
#pragma pack(push, 1)
struct IndexEntry {
    uint32_t key;      // Record's key value
    uint32_t offset;   // File offset in the log file
    uint8_t  status;   // Record status
};
#pragma pack(pop)

// --- DBEngine Class ---
class DBEngine {
public:
    DBEngine(IFileHandler& logHandler, IFileHandler& indexHandler);

    void dbOpen(const char logFileName[MAX_FILENAME_LENGTH],
        const char indexFileName[MAX_FILENAME_LENGTH]);

    bool dbBuildIndex(void);

    bool dbAppendRecord(uint8_t recordType, const void* record, uint16_t recordSize);

    bool dbUpdateRecordStatusByIndexId(uint32_t indexId, uint8_t newStatus);

    bool dbGetRecordByIndexId(uint32_t indexId, LogEntryHeader& header,
        void* payloadBuffer, uint16_t bufferSize);

    bool dbGetRecordByKey(uint32_t key, LogEntryHeader& header,
        void* payloadBuffer, uint16_t bufferSize);

    size_t dbFindRecordsByStatus(uint8_t status, uint32_t results[], size_t maxResults) const;

    // --- B-Tree–Style Search Methods ---
    bool btreeFindKey(uint32_t key, uint32_t* index);
    bool btreeLocateKey(uint32_t key, uint32_t* index);
    bool btreeNextKey(uint32_t currentIndex, uint32_t* nextIndex);
    bool btreePrevKey(uint32_t currentIndex, uint32_t* prevIndex);

    size_t dbGetIndexCount(void) const;

    // Get an index entry (by global index) into 'entry'
    bool getIndexEntry(uint32_t globalIndex, IndexEntry& entry);

private:
    char _logFileName[MAX_FILENAME_LENGTH];
    char _indexFileName[MAX_FILENAME_LENGTH];

    // The total number of index entries.
    uint32_t _indexCount;

    // --- Paging support for the index ---
    // Instead of holding all index entries in memory, we now have a page buffer.
    IndexEntry _indexPage[MAX_INDEX_ENTRIES]; // Current page
    uint32_t _currentPageNumber;              // Which page is currently in RAM?
    bool _pageLoaded;                         // True if _indexPage holds a valid page
    bool _pageDirty;                          // True if _indexPage has been modified and must be flushed

    // References to the file handlers.
    IFileHandler& _logHandler;
    IFileHandler& _indexHandler;

    // --- Helper functions for index paging ---
    // Flush the current page (if dirty) to disk.
    bool flushIndexPage(void);
    // Load the specified page (0-based) from disk into _indexPage.
    bool loadIndexPage(uint32_t pageNumber);

    // Write (or update) an index entry (by global index) from 'entry'
    bool setIndexEntry(uint32_t globalIndex, const IndexEntry& entry);
    // Shift all index entries starting at position 'startIdx' one position to the right.
    bool shiftIndexEntriesRight(uint32_t startIdx);

    // --- Helpers for saving and loading the overall index file ---
    bool saveIndexHeader(void);
    bool loadIndexHeader(void);

    // --- Internal helper functions ---
    // Performs binary search on the entire index for an exact key.
    // If found, sets *foundIndex to the global index and returns true.
    bool searchIndex(uint32_t key, uint32_t* foundIndex) const;
    // Finds an index entry by key.
    // If found, sets 'offset' to the file offset of the matching record and returns true.
    bool findIndexEntry(uint32_t key, uint32_t& offset) const;
    // Inserts a new index entry into the index (maintaining sorted order).
    bool insertIndexEntry(uint32_t key, uint32_t offset, uint8_t status);

    // --- New helper function for deferred flushing and page splitting ---
    // Handles the case where the target page is full by splitting it into two pages.
    // The new entry is then inserted into the appropriate page.
    bool splitPageAndInsert(uint32_t targetPage, uint32_t offsetInPage,
        uint32_t key, uint32_t recordOffset, uint8_t status);

    bool validateIndex(void);

};

#endif // DBENGINE_H
