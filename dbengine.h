#ifndef DBENGINE_H
#define DBENGINE_H

#include "IFileHandler.h"  // The abstract file I/O interface

// Use the C standard headers for maximum portability on embedded devices.
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Constants & Macros
// -----------------------------------------------------------------------------
#define MAX_INDEX_ENTRIES 256
#define MAX_FILENAME_LENGTH 13  // For 8.3 filenames

#define DB_MAGIC_NUMBER     0x53474F4C  // "LOGS" in little-endian hex
#define DB_VERSION          0x0001
#define DB_IDX_VERSION      0x0001

/// Deletion flag for internal_status.
#define INTERNAL_STATUS_DELETED 0x01


// -----------------------------------------------------------------------------
// Instrumentation Macros
// -----------------------------------------------------------------------------
// Only include instrumentation support on Windows. On embedded devices such as the SAMD21,
// the instrumentation code is excluded completely.
#ifndef _WIN32
    #include "Instrumentation.h"
    //#define SCOPE_TIMER(name) ScopedTimer timer(name)
    #define SCOPE_TIMER(name)

    //#define DEBUG_PRINT(...) printf(__VA_ARGS__)
    #define DEBUG_PRINT(...)
#else
  // Provide no-op definitions for non-Windows builds.
    #define SCOPE_TIMER(name)
    #define DEBUG_PRINT(...)
#endif


// -----------------------------------------------------------------------------
// Data Structures
// -----------------------------------------------------------------------------

//
// --- DB Header ---
// This header holds metadata for the log file.
//
#pragma pack(push, 1)
struct DBHeader {
    uint32_t magic;    ///< Magic number ("LOGS")
    uint16_t version;  ///< Log file format version
};
#pragma pack(pop)

//
// --- DB Index Header ---
// This header holds metadata for the index file, including the index count.
//
#pragma pack(push, 1)
struct DBIndexHeader {
    uint32_t magic;      ///< Magic number ("LOGS")
    uint16_t version;    ///< Index file format version
    uint32_t indexCount; ///< Number of index entries stored in the file
};
#pragma pack(pop)

//
// --- Log Entry Header ---
// Represents metadata for a log entry in the database. Note: The user-controlled 
// field "status" is preserved, and we add an additional "internal_status" field 
// for internal bookkeeping (e.g., deletion flag).
//
#pragma pack(push, 1)
struct LogEntryHeader {
    uint8_t  recordType;      ///< Identifier for the record type
    uint16_t length;          ///< Payload length in bytes
    uint32_t key;             ///< Key value supplied by the caller
    uint8_t  status;          ///< User-supplied status (untouched internally)
    uint8_t  internal_status; ///< Internal status (e.g., deletion flag)
};
#pragma pack(pop)

//
// --- Index Entry ---
// Represents an entry in the index file, linking keys to their offsets in the log file.
// We add an "internal_status" field for internal management (e.g., deletion flag).
//
#pragma pack(push, 1)
struct IndexEntry {
    uint32_t key;             ///< Record's key value
    uint32_t offset;          ///< File offset in the log file
    uint8_t  status;          ///< User-supplied status (untouched internally)
    uint8_t  internal_status; ///< Internal status (e.g., deletion flag)
};
#pragma pack(pop)


// -----------------------------------------------------------------------------
// DBEngine Class Declaration
// -----------------------------------------------------------------------------

/**
 * @brief Implements a key-value database with an indexed, log-based storage system.
 *
 * The DBEngine supports appending new records, updating and retrieving records,
 * as well as index management. The index is maintained in a paged format.
 *
 * The implementation is split between general DB engine functions (e.g., file I/O)
 * and index-specific operations (e.g., paging, searching, deletion marking).
 */
class DBEngine {
public:
    // -------------------------------------------------------------------------
    // General Database Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Constructs a DBEngine instance.
     *
     * @param logHandler Reference to a file handler for log file operations.
     * @param indexHandler Reference to a file handler for index file operations.
     */
    DBEngine(IFileHandler& logHandler, IFileHandler& indexHandler);

    /**
     * @brief Opens the database files (log and index) for future operations.
     *
     * This function opens and validates (or creates if necessary) the specified
     * log and index files.
     *
     * @param logFileName Name of the log file.
     * @param indexFileName Name of the index file.
     * @return True if the files were successfully opened and validated, false otherwise.
     */
    bool open(const char logFileName[MAX_FILENAME_LENGTH],
        const char indexFileName[MAX_FILENAME_LENGTH]);

    /**
     * @brief Appends a new record to the log file and creates an index entry.
     *
     * The caller supplies the key and the record payload. The engine checks for key
     * collisions before appending.
     *
     * @param key User-supplied key for the record.
     * @param recordType Type identifier for the record.
     * @param record Pointer to the record data.
     * @param recordSize Size of the record in bytes.
     * @return True if the record was successfully appended, false on failure.
     */
    bool append(uint32_t key, uint8_t recordType, const void* record, uint16_t recordSize);

    /**
     * @brief Updates the status of a record given its index position.
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
     * @param payloadBuffer Buffer to store the record payload.
     * @param bufferSize Size of the payloadBuffer in bytes.
     * @param outRecordSize Optional pointer to receive the actual record size.
     * @return True if retrieval was successful, false otherwise.
     */
    bool get(uint32_t key, void* payloadBuffer, uint16_t bufferSize, uint16_t* outRecordSize = nullptr);

    /**
     * @brief Finds records with a specific user-supplied status.
     *
     * Searches the index for records with the specified status and returns the
     * matching global index positions in the provided array.
     *
     * @param status The status value to search for.
     * @param results Array to store the matching index positions.
     * @param maxResults Maximum number of results to store.
     * @return The number of records found.
     */
    size_t findByStatus(uint8_t status, uint32_t results[], size_t maxResults) const;

    /**
     * @brief Returns the total number of index entries.
     *
     * @return The total number of index entries.
     */
    size_t indexCount(void) const;

    /**
     * @brief Retrieves an index entry by its global index position.
     *
     * @param globalIndex The global index position of the entry.
     * @param entry Reference to store the retrieved index entry.
     * @return True if the index entry was successfully retrieved, false otherwise.
     */
    bool getIndexEntry(uint32_t globalIndex, IndexEntry& entry);

    /**
     * @brief Deletes a record by key by marking it as deleted.
     *
     * Marks the record as deleted by setting the deletion flag in its internal status.
     * If the record is already marked deleted, no further action is taken.
     *
     * @param key The key of the record to delete.
     * @return True if the record was successfully marked as deleted, false otherwise.
     */
    bool deleteRecord(uint32_t key);

    /**
     * @brief Returns the database file format version.
     *
     * @return The version number of the database format.
     */
    uint16_t dbVersion(void) const;

    /**
     * @brief Prints database engine statistics.
     */
    void printStats(void) const;

    // -------------------------------------------------------------------------
    // B-Tree / Index Search Methods
    // -------------------------------------------------------------------------

    /**
     * @brief Searches for an exact key using a binary search on the index.
     *
     * If the key is found, its global index is returned via the foundIndex pointer.
     *
     * @param key The key to search for.
     * @param foundIndex Pointer to store the global index of the found key.
     * @return True if the key is found, false otherwise.
     */
    bool searchIndex(uint32_t key, uint32_t* foundIndex) const;

    /**
     * @brief Finds the key using a B-Tree style search.
     *
     * @param key The key to find.
     * @param index Pointer to store the global index if found.
     * @return True if the key is found, false otherwise.
     */
    bool findKey(uint32_t key, uint32_t* index);

    /**
     * @brief Locates the position where a key should be in the index.
     *
     * @param key The key to locate.
     * @param index Pointer to store the insertion or matching position.
     * @return True if a suitable location is found, false otherwise.
     */
    bool locateKey(uint32_t key, uint32_t* index);

    /**
     * @brief Returns the next key's global index relative to the given index.
     *
     * @param currentIndex The current global index.
     * @param nextIndex Pointer to store the next key's index.
     * @return True if there is a next key, false otherwise.
     */
    bool nextKey(uint32_t currentIndex, uint32_t* nextIndex);

    /**
     * @brief Returns the previous key's global index relative to the given index.
     *
     * @param currentIndex The current global index.
     * @param prevIndex Pointer to store the previous key's index.
     * @return True if there is a previous key, false otherwise.
     */
    bool prevKey(uint32_t currentIndex, uint32_t* prevIndex);

    // -------------------------------------------------------------------------
    // Index Record Filtering and Counting (Index Functions)
    // These functions are grouped together because they work with the index
    // entries and their internal status flags.
    // -------------------------------------------------------------------------

    /**
     * @brief Retrieves the first index entry matching specified internal_status criteria.
     *
     * The match criteria are defined by two bit masks:
     *   - All bits in mustBeSet must be present in the entry's internal_status.
     *   - None of the bits in mustBeClear may be present in the entry's internal_status.
     *
     * If a matching entry is found, it is stored in 'entry' and its global index
     * is stored in 'indexPosition'.
     *
     * @param mustBeSet Bit mask of flags that must be set.
     * @param mustBeClear Bit mask of flags that must be clear.
     * @param entry Reference to store the matching index entry.
     * @param indexPosition Reference to store the global index of the matching entry.
     * @return True if a matching entry is found, false otherwise.
     */
    bool getFirstMatchingIndexEntry(uint8_t mustBeSet, uint8_t mustBeClear,
        IndexEntry& entry, uint32_t& indexPosition) const;

    /**
     * @brief Retrieves the first active index entry (i.e. not marked as deleted).
     *
     * Active entries are those for which the INTERNAL_STATUS_DELETED flag is clear.
     *
     * @param entry Reference to store the active index entry.
     * @param indexPosition Reference to store the global index of the active entry.
     * @return True if an active entry is found, false otherwise.
     */
    bool getFirstActiveIndexEntry(IndexEntry& entry, uint32_t& indexPosition) const;

    /**
     * @brief Retrieves the first deleted index entry.
     *
     * Deleted entries are those for which the INTERNAL_STATUS_DELETED flag is set.
     *
     * @param entry Reference to store the deleted index entry.
     * @param indexPosition Reference to store the global index of the deleted entry.
     * @return True if a deleted entry is found, false otherwise.
     */
    bool getFirstDeletedIndexEntry(IndexEntry& entry, uint32_t& indexPosition) const;

    /**
     * @brief Returns the count of index entries whose internal_status has all bits
     *        in the specified mask set.
     *
     * This is a convenience overload which does not require any bits to be clear.
     *
     * @param internalStatus Bit mask of flags that must be set.
     * @return The count of matching index entries.
     */
    size_t recordCount(uint8_t internalStatus) const;

    /**
     * @brief Returns the count of index entries matching internal_status criteria.
     *
     * Only index entries for which:
     *   (entry.internal_status & mustBeSet) == mustBeSet AND
     *   (entry.internal_status & mustBeClear) == 0
     * are counted.
     *
     * @param mustBeSet Bit mask of flags that must be set.
     * @param mustBeClear Bit mask of flags that must be clear.
     * @return The count of matching index entries.
     */
    size_t recordCount(uint8_t mustBeSet, uint8_t mustBeClear) const;

    // -------------------------------------------------------------------------
    // End of Public Interface
    // -------------------------------------------------------------------------

private:
    // -------------------------------------------------------------------------
    // File and Index Metadata
    // -------------------------------------------------------------------------
    char _logFileName[MAX_FILENAME_LENGTH];   ///< Log file name.
    char _indexFileName[MAX_FILENAME_LENGTH];   ///< Index file name.

    uint32_t _indexCount;  ///< Total number of index entries.

    // -------------------------------------------------------------------------
    // Index Paging Data
    // -------------------------------------------------------------------------
    IndexEntry _indexPage[MAX_INDEX_ENTRIES]; ///< Current index page in memory.
    uint32_t _currentPageNumber;              ///< Currently loaded page number.
    bool _pageLoaded;                         ///< Indicates if an index page is loaded.
    bool _pageDirty;                          ///< Indicates if the current page has been modified.

    // -------------------------------------------------------------------------
    // File Handlers
    // -------------------------------------------------------------------------
    IFileHandler& _logHandler;   ///< File handler for log operations.
    IFileHandler& _indexHandler; ///< File handler for index operations.

    // -------------------------------------------------------------------------
    // Internal / Index Helper Functions
    // These functions are used internally for managing the index file and its pages.
    // -------------------------------------------------------------------------

    /**
     * @brief Flushes the current in-memory index page to disk if it is dirty.
     *
     * @return True if the flush was successful or not needed, false otherwise.
     */
    bool flushIndexPage(void);

    /**
     * @brief Loads the specified index page (0-based) into memory.
     *
     * If the requested page is not fully populated, the remainder is zero-filled.
     *
     * @param pageNumber The page number to load.
     * @return True if the page was successfully loaded, false otherwise.
     */
    bool loadIndexPage(uint32_t pageNumber);

    /**
     * @brief Updates an index entry at the given global index in memory.
     *
     * Marks the page as dirty so that it will be flushed later.
     *
     * @param globalIndex The global index position to update.
     * @param entry The new index entry to write.
     * @return True if the update was successful, false otherwise.
     */
    bool setIndexEntry(uint32_t globalIndex, const IndexEntry& entry);

    /**
     * @brief Writes the index header (including the index count) to disk.
     *
     * @return True if the header was successfully written, false otherwise.
     */
    bool saveIndexHeader(void);

    /**
     * @brief Reads the index header from disk and initializes the in-memory index state.
     *
     * @return True if the header was successfully read and validated, false otherwise.
     */
    bool loadIndexHeader(void);

    /**
     * @brief Finds an index entry by key and returns its file offset.
     *
     * @param key The key to search for.
     * @param offset Reference to store the file offset of the record.
     * @return True if the key is found, false otherwise.
     */
    bool findIndexEntry(uint32_t key, uint32_t& offset) const;

    /**
     * @brief Inserts a new index entry in sorted order.
     *
     * If the target index page is full, the page is split.
     *
     * @param key The key to insert.
     * @param offset The file offset of the new record.
     * @param status User-supplied status.
     * @param internal_status Internal status flags.
     * @return True if the insertion was successful, false otherwise.
     */
    bool insertIndexEntry(uint32_t key, uint32_t offset, uint8_t status, uint8_t internal_status);

    /**
     * @brief Splits a full index page and inserts a new index entry.
     *
     * The current page is split at the midpoint, and the new entry is inserted
     * into the appropriate page. Both pages are then flushed.
     *
     * @param targetPage The page number to split.
     * @param offsetInPage The offset within the page where the new entry should be inserted.
     * @param key The key to insert.
     * @param recordOffset The file offset of the new record.
     * @param status User-supplied status.
     * @param internal_status Internal status flags.
     * @return True if the split and insertion were successful, false otherwise.
     */
    bool splitPageAndInsert(uint32_t targetPage, uint32_t offsetInPage,
        uint32_t key, uint32_t recordOffset, uint8_t status, uint8_t internal_status);

    /**
     * @brief Validates the index file for corruption.
     *
     * Checks the order of keys in the first page and other integrity constraints.
     *
     * @return True if the index is valid, false otherwise.
     */
    bool validateIndex(void);

    // -------------------------------------------------------------------------
    // Database Header Functions for the Index File
    // -------------------------------------------------------------------------

    /**
     * @brief Writes the database header to disk.
     *
     * @return True if the header was successfully written, false otherwise.
     */
    bool saveDBHeader(void);

    /**
     * @brief Loads the database header from disk.
     *
     * @return True if the header was successfully read, false otherwise.
     */
    bool loadDBHeader(void);

    /**
     * @brief Builds or loads the in-memory index from the index file.
     *
     * @return True if the index was successfully loaded or built, false otherwise.
     */
    bool loadIndex(void);

    // -------------------------------------------------------------------------
    // Internal Storage for Database Header Metadata
    // -------------------------------------------------------------------------
    DBHeader _dbHeader;  ///< In-memory copy of the database header.
};

#endif // DBENGINE_H
