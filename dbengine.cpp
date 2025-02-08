// dbengine.cpp
#include "DBEngine.h"
#include "IFileHandler.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Common Helper
// ---------------------------------------------------------------------------

// --- getMillis ---
// Returns the current time in milliseconds.
// On embedded hardware, replace with an appropriate timer function.
static uint32_t getMillis(void) {
    static uint32_t count = 0;
    return count++; // (uint32_t)(clock() * 1000 / CLOCKS_PER_SEC);
}

// ---------------------------------------------------------------------------
// Common (Non-Index) Methods: Constructor and dbOpen
// ---------------------------------------------------------------------------
DBEngine::DBEngine(IFileHandler& logHandler, IFileHandler& indexHandler)
    : _logHandler(logHandler), _indexHandler(indexHandler),
    _indexCount(0), _currentPageNumber(0),
    _pageLoaded(false), _pageDirty(false)
{
    _logFileName[0] = '\0';
    _indexFileName[0] = '\0';
}

// ---------------------------------------------------------------------------
// Record Handling Functions
// ---------------------------------------------------------------------------

// --- dbAppendRecord ---
// Appends a new record to the log file and creates an index entry.
bool DBEngine::append(uint32_t key, uint8_t recordType, const void* record, uint16_t recordSize) {
    size_t bytesWritten = 0;
    uint32_t foundIndex;
    bool reuseEntry = false;

    // Check for key collision in the index.
    if (searchIndex(key, &foundIndex)) {
        // Retrieve the existing index entry.
        IndexEntry existing;
        if (!getIndexEntry(foundIndex, existing))
            return false;

        // If the record is live (internal_status is not marked deleted), then abort.
        if ((existing.internal_status & INTERNAL_STATUS_DELETED) == 0) {
            DEBUG_PRINT("append: Duplicate live key detected (key=%u). Aborting append.\n", key);
            return false;
        }
        // Otherwise, if the record is marked deleted, we will reuse its index entry.
        DEBUG_PRINT("append: Reusing deleted index entry for key=%u at index %u.\n", key, foundIndex);
        reuseEntry = true;
    }

    // Open log file in read/write mode; if it does not exist, create it and write a DBHeader.
    if (!_logHandler.open(_logFileName, "r+b")) {
        if (!_logHandler.open(_logFileName, "wb+"))
            return false;
        // New file: write log header.
        DBHeader logHeader;
        logHeader.magic = DB_MAGIC_NUMBER;
        logHeader.version = DB_VERSION;
        if (!_logHandler.write(reinterpret_cast<const uint8_t*>(&logHeader), sizeof(logHeader), bytesWritten) ||
            bytesWritten != sizeof(logHeader)) {
            _logHandler.close();
            return false;
        }
    }

    // Seek to the end of the log file to obtain the record offset.
    if (!_logHandler.seekToEnd()) {
        _logHandler.close();
        return false;
    }
    uint32_t offset = _logHandler.tell();

    // Build the log entry header with the caller-supplied key.
    LogEntryHeader header;
    header.recordType = recordType;
    header.length = recordSize;
    header.key = key;
    header.status = 0;             // User-supplied status remains as provided.
    header.internal_status = 0;    // Clear internal status (i.e. record is live).

    // Write the log entry header.
    if (!_logHandler.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header), bytesWritten) ||
        bytesWritten != sizeof(header)) {
        _logHandler.close();
        return false;
    }

    // Write the record data.
    if (!_logHandler.write(reinterpret_cast<const uint8_t*>(record), recordSize, bytesWritten) ||
        bytesWritten != recordSize) {
        _logHandler.close();
        return false;
    }
    _logHandler.close();

    // If a duplicate (deleted) record was found, update its index entry.
    if (reuseEntry) {
        IndexEntry entry;
        if (!getIndexEntry(foundIndex, entry))
            return false;
        // Update the index entry with the new record offset and clear the deletion flag.
        entry.offset = offset;
        // Leave the user status as is.
        entry.internal_status = 0;  // Mark record as live.
        if (!setIndexEntry(foundIndex, entry))
            return false;
        DEBUG_PRINT("append: Updated index entry for key=%u at index %u.\n", key, foundIndex);
    }
    else {
        // Insert a new index entry. We now pass both the user status and internal status.
        if (!insertIndexEntry(header.key, offset, header.status, header.internal_status)) {
            DEBUG_PRINT("append: Failed to insert index entry (unexpected collision) for key=%u.\n", key);
            return false;
        }
    }

    return true;
}

bool DBEngine::updateStatus(uint32_t indexId, uint8_t newStatus) {
    if (indexId >= _indexCount) {
        DEBUG_PRINT("updateStatus: Invalid indexId %u (max %u).\n", indexId, _indexCount);
        return false;
    }

    IndexEntry entry;
    if (!getIndexEntry(indexId, entry))
        return false;

    uint32_t recordOffset = entry.offset;

    if (!_logHandler.open(_logFileName, "rb+"))
        return false;

    // Compute offset to the status field:
    // offset + sizeof(recordType) + sizeof(length) + sizeof(key)
    uint32_t statusFieldOffset = recordOffset + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t);

    if (!_logHandler.seek(statusFieldOffset)) {
        _logHandler.close();
        return false;
    }

    size_t bytesWritten = 0;
    if (!_logHandler.write(reinterpret_cast<const uint8_t*>(&newStatus), sizeof(newStatus), bytesWritten) ||
        bytesWritten != sizeof(newStatus)) {
        _logHandler.close();
        return false;
    }
    _logHandler.close();

    // Update the index entry status.
    entry.status = newStatus;
    if (!setIndexEntry(indexId, entry))
        return false;

    return true;
}


// --- dbGetRecordByKey ---
// Retrieves a record by searching the index for the given key.
bool DBEngine::get(uint32_t key, void* payloadBuffer, uint16_t bufferSize, uint16_t* outRecordSize) {
    uint32_t offset = 0;
    if (!findIndexEntry(key, offset))
        return false;

    size_t bytesRead = 0;
    if (!_logHandler.open(_logFileName, "rb"))
        return false;
    if (!_logHandler.seek(offset)) {
        _logHandler.close();
        return false;
    }

    LogEntryHeader localHeader;
    if (!_logHandler.read(reinterpret_cast<uint8_t*>(&localHeader), sizeof(localHeader), bytesRead) ||
        bytesRead != sizeof(localHeader)) {
        _logHandler.close();
        return false;
    }
    if (localHeader.length > bufferSize) {
        _logHandler.close();
        return false;
    }
    if (!_logHandler.read(reinterpret_cast<uint8_t*>(payloadBuffer), localHeader.length, bytesRead) ||
        bytesRead != localHeader.length) {
        _logHandler.close();
        return false;
    }
    _logHandler.close();

    if (outRecordSize)
        *outRecordSize = localHeader.length;
    return true;
}

bool DBEngine::deleteRecord(uint32_t key) {
    uint32_t index;
    // Find the record by key.
    if (!searchIndex(key, &index)) {
        DEBUG_PRINT("deleteRecord: Key %u not found in index.\n", key);
        return false;
    }

    IndexEntry entry;
    if (!getIndexEntry(index, entry))
        return false;

    // If already deleted, nothing to do.
    if (entry.internal_status & INTERNAL_STATUS_DELETED) {
        DEBUG_PRINT("deleteRecord: Key %u already marked as deleted.\n", key);
        return true;
    }

    // Set the deletion flag in the internal status.
    uint8_t newInternalStatus = entry.internal_status | INTERNAL_STATUS_DELETED;

    // Update the log file.
    // Compute offset to the internal_status field:
    // Offset is: record offset + sizeof(recordType) + sizeof(length) + sizeof(key) + sizeof(user status)
    uint32_t recordOffset = entry.offset;
    uint32_t internalStatusFieldOffset = recordOffset +
        sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint8_t);

    if (!_logHandler.open(_logFileName, "rb+"))
        return false;
    if (!_logHandler.seek(internalStatusFieldOffset)) {
        _logHandler.close();
        return false;
    }
    size_t bytesWritten = 0;
    if (!_logHandler.write(reinterpret_cast<const uint8_t*>(&newInternalStatus), sizeof(newInternalStatus), bytesWritten) ||
        bytesWritten != sizeof(newInternalStatus)) {
        _logHandler.close();
        return false;
    }
    _logHandler.close();

    // Update the index entry's internal_status.
    entry.internal_status = newInternalStatus;
    if (!setIndexEntry(index, entry))
        return false;

    DEBUG_PRINT("deleteRecord: Key %u marked as deleted (internal_status updated).\n", key);
    return true;
}

bool DBEngine::saveDBHeader(void) {
    DBHeader header;
    header.magic = DB_MAGIC_NUMBER;   // "LOGS"
    header.version = DB_VERSION;       // Use the DB_VERSION constant (0x0001)

    // Open the log file in a mode that allows writing at the beginning.
    if (!_logHandler.open(_logFileName, "rb+")) {
        // Try creating a new file if it doesn't exist.
        if (!_logHandler.open(_logFileName, "wb+"))
            return false;
    }
    if (!_logHandler.seek(0)) {
        _logHandler.close();
        return false;
    }
    size_t bytesWritten = 0;
    if (!_logHandler.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header), bytesWritten) ||
        bytesWritten != sizeof(header)) {
        _logHandler.close();
        return false;
    }
    _logHandler.close();
    return true;
}


bool DBEngine::loadDBHeader(void) {
    DBHeader header;
    size_t bytesRead = 0;
    if (!_logHandler.open(_logFileName, "rb")) {
        DEBUG_PRINT("loadDBHeader: Could not open log file %s.\n", _logFileName);
        return false;
    }
    if (!_logHandler.read(reinterpret_cast<uint8_t*>(&header), sizeof(header), bytesRead) ||
        bytesRead != sizeof(header)) {
        DEBUG_PRINT("loadDBHeader: Failed to read header from log file.\n");
        _logHandler.close();
        return false;
    }
    _logHandler.close();

    // Validate the header.
    if (header.magic != DB_MAGIC_NUMBER) {
        DEBUG_PRINT("loadDBHeader: Invalid magic number in log file.\n");
        return false;
    }
    if (header.version != DB_VERSION) {
        DEBUG_PRINT("loadDBHeader: Unsupported version %u in log file.\n", header.version);
        return false;
    }

    // Save the header internally.
    _dbHeader = header;
    return true;
}

bool DBEngine::open(const char logFileName[MAX_FILENAME_LENGTH],
    const char indexFileName[MAX_FILENAME_LENGTH])
{
    // Copy file names as before.
    strncpy(_logFileName, logFileName, MAX_FILENAME_LENGTH - 1);
    _logFileName[MAX_FILENAME_LENGTH - 1] = '\0';
    strncpy(_indexFileName, indexFileName, MAX_FILENAME_LENGTH - 1);
    _indexFileName[MAX_FILENAME_LENGTH - 1] = '\0';

    // Reset internal variables.
    _indexCount = 0;
    _pageLoaded = false;
    _pageDirty = false;
    _currentPageNumber = 0;

    // Attempt to load and validate the DB header.
    if (!loadDBHeader()) {
        DEBUG_PRINT("dbOpenAndLoad: No valid header found. Creating new database file.\n");
        // Create a new file by saving the header.
        if (!saveDBHeader()) {
            DEBUG_PRINT("dbOpenAndLoad: Failed to create a new DB header.\n");
            return false;
        }
    }

    // Now load the in-memory index from the index file.
    if (!loadIndex()) {
        DEBUG_PRINT("open: Failed to load in-memory index.\n");
        return false;
    }

    // Optionally, you might also load the first index page or perform other initialization.
    return true;
}

// Prints database statistics: number of records, pages, records per page, and unique keys.
void DBEngine::printStats(void) const {
    // _indexCount should have been set when loading the index header from disk.
    uint32_t totalRecords = _indexCount;
    // Each in-memory index page holds MAX_INDEX_ENTRIES entries.
    uint32_t totalPages = (totalRecords + MAX_INDEX_ENTRIES - 1) / MAX_INDEX_ENTRIES;

    printf("Database Statistics:\n");
    printf("  Total records: %u\n", totalRecords);
    printf("  Total pages: %u\n", totalPages);

    // Print the number of entries on each page.
    for (uint32_t page = 0; page < totalPages; page++) {
        uint32_t start = page * MAX_INDEX_ENTRIES;
        uint32_t end = start + MAX_INDEX_ENTRIES;
        if (end > totalRecords)
            end = totalRecords;
        uint32_t countOnPage = end - start;
        //printf("    Page %u: %u entries\n", page, countOnPage);
    }

    // Count unique keys by scanning all pages.
    // Since this function is const but we need to load pages,
    // we use const_cast to temporarily call non-const loadIndexPage().
    DBEngine* self = const_cast<DBEngine*>(this);
    uint32_t uniqueCount = 0;
    uint32_t lastKey = 0;
    bool first = true;

    for (uint32_t page = 0; page < totalPages; page++) {
        //printf("printStats: Loading page %u for unique key count...\n", page);
        if (!self->loadIndexPage(page)) {
           // printf("printStats: Error loading page %u\n", page);
            continue;
        }
        // Determine number of entries on this page.
        uint32_t start = page * MAX_INDEX_ENTRIES;
        uint32_t countInPage = (page == totalPages - 1) ? (totalRecords - start) : MAX_INDEX_ENTRIES;
        for (uint32_t i = 0; i < countInPage; i++) {
            uint32_t key = self->_indexPage[i].key;
            if (first || key != lastKey) {
                uniqueCount++;
                lastKey = key;
                first = false;
            }
        }
    }
    printf("  Unique keys: %u\n", uniqueCount);
}


