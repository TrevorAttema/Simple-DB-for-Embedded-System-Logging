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

void DBEngine::dbOpen(const char logFileName[MAX_FILENAME_LENGTH],
    const char indexFileName[MAX_FILENAME_LENGTH])
{
    strncpy(_logFileName, logFileName, MAX_FILENAME_LENGTH - 1);
    _logFileName[MAX_FILENAME_LENGTH - 1] = '\0';
    strncpy(_indexFileName, indexFileName, MAX_FILENAME_LENGTH - 1);
    _indexFileName[MAX_FILENAME_LENGTH - 1] = '\0';
    _indexCount = 0;
    _pageLoaded = false;
    _pageDirty = false;
    _currentPageNumber = 0;
}

// ---------------------------------------------------------------------------
// Record Handling Functions
// ---------------------------------------------------------------------------

// --- dbAppendRecord ---
// Appends a new record to the log file and creates an index entry.
bool DBEngine::dbAppendRecord(uint8_t recordType, const void* record, uint16_t recordSize) {
    size_t bytesWritten = 0;

    // Generate a new key.
    uint32_t key = getMillis();  // Use current time (in millis) as key.

    // Check for key collision in the index.
    uint32_t dummyIndex;
    if (searchIndex(key, &dummyIndex)) {
        DEBUG_PRINT("dbAppendRecord: Duplicate key detected (key=%u). Aborting append.\n", key);
        return false;  // Collision found: do not append the record.
    }

    // Open log file in read/write mode. If the file does not exist, create it.
    if (!_logHandler.open(_logFileName, "r+b")) {
        // If opening in read/write fails, try to create the file.
        if (!_logHandler.open(_logFileName, "wb+"))
            return false;
    }

    // Seek to the end to get the current file size (the offset).
    if (!_logHandler.seekToEnd()) {  // You may need to implement this in your IFileHandler.
        _logHandler.close();
        return false;
    }
    uint32_t offset = _logHandler.tell();

    // Build the header with the new record information.
    LogEntryHeader header;
    header.recordType = recordType;
    header.length = recordSize;
    header.key = key;      // Use the key we generated (and already verified).
    header.status = 0;     // Default status.

    // Write the header.
    if (!_logHandler.write((const uint8_t*)&header, sizeof(header), bytesWritten) ||
        bytesWritten != sizeof(header)) {
        _logHandler.close();
        return false;
    }

    // Write the actual record data.
    if (!_logHandler.write((const uint8_t*)record, recordSize, bytesWritten) ||
        bytesWritten != recordSize) {
        _logHandler.close();
        return false;
    }
    _logHandler.close();

    // Insert the index entry.
    // Note: insertIndexEntry() should also check for collisions and fail if one exists.
    if (!insertIndexEntry(header.key, offset, header.status)) {
        DEBUG_PRINT("dbAppendRecord: Failed to insert index entry (possible collision).\n");
        return false;
    }

    return true;
}



// --- dbUpdateRecordStatus ---
// Updates the status field in the log file and in the corresponding index entry.
// --- dbUpdateRecordStatus ---
// Updates the status field in the log file and in the corresponding index entry,
// using the index ID to locate the record.
bool DBEngine::dbUpdateRecordStatusByIndexId(uint32_t indexId, uint8_t newStatus) {
    // Validate the index ID.
    if (indexId >= _indexCount) {
        DEBUG_PRINT("dbUpdateRecordStatus: Invalid indexId %u (max %u).\n", indexId, _indexCount);
        return false;
    }

    // Retrieve the index entry using the index ID.
    IndexEntry entry;
    if (!getIndexEntry(indexId, entry)) {
        DEBUG_PRINT("dbUpdateRecordStatus: Failed to get index entry for indexId %u.\n", indexId);
        return false;
    }

    // Get the offset of the log record from the index entry.
    uint32_t recordOffset = entry.offset;

    // Open the log file in read-write mode.
    if (!_logHandler.open(_logFileName, "rb+")) {
        DEBUG_PRINT("dbUpdateRecordStatus: Failed to open log file %s.\n", _logFileName);
        return false;
    }

    // Calculate the offset of the status field.
    // The LogEntryHeader layout is:
    //   uint8_t  recordType;   // 1 byte
    //   uint16_t length;       // 2 bytes
    //   uint32_t key;          // 4 bytes
    //   uint8_t  status;       // 1 byte (this field)
    //
    // We calculate the status field offset as:
    //   recordOffset + sizeof(recordType) + sizeof(length) + sizeof(key)
    uint32_t statusFieldOffset = recordOffset
        + sizeof(uint8_t)    // recordType
        + sizeof(uint16_t)   // length
        + sizeof(uint32_t);  // key

    if (!_logHandler.seek(statusFieldOffset)) {
        DEBUG_PRINT("dbUpdateRecordStatus: Seek to offset %u failed.\n", statusFieldOffset);
        _logHandler.close();
        return false;
    }

    // Write the new status value.
    size_t bytesWritten = 0;
    if (!_logHandler.write(reinterpret_cast<const uint8_t*>(&newStatus),
        sizeof(newStatus), bytesWritten) ||
        bytesWritten != sizeof(newStatus)) {
        DEBUG_PRINT("dbUpdateRecordStatus: Write of newStatus failed (wrote %zu bytes).\n", bytesWritten);
        _logHandler.close();
        return false;
    }
    _logHandler.close();

    // Update the status in the index entry.
    entry.status = newStatus;
    if (!setIndexEntry(indexId, entry)) {
        DEBUG_PRINT("dbUpdateRecordStatus: Failed to update index entry for indexId %u.\n", indexId);
        return false;
    }

    DEBUG_PRINT("dbUpdateRecordStatus: Successfully updated status for indexId %u.\n", indexId);
    return true;
}

// --- dbGetRecordByIndexId ---
// Retrieves a record from the log file given its index ID.
bool DBEngine::dbGetRecordByIndexId(uint32_t indexId, LogEntryHeader& header,
    void* payloadBuffer, uint16_t bufferSize)
{
    if (indexId >= _indexCount)
        return false;
    IndexEntry entry;
    if (!getIndexEntry(indexId, entry))
        return false;
    uint32_t offset = entry.offset;
    size_t bytesRead = 0;
    if (!_logHandler.open(_logFileName, "rb"))
        return false;
    if (!_logHandler.seek(offset)) {
        _logHandler.close();
        return false;
    }
    if (!_logHandler.read((uint8_t*)&header, sizeof(header), bytesRead) ||
        bytesRead != sizeof(header)) {
        _logHandler.close();
        return false;
    }
    if (header.length > bufferSize) {
        _logHandler.close();
        return false;
    }
    if (!_logHandler.read((uint8_t*)payloadBuffer, header.length, bytesRead) ||
        bytesRead != header.length) {
        _logHandler.close();
        return false;
    }
    _logHandler.close();
    return true;
}

// --- dbGetRecordByKey ---
// Retrieves a record by searching the index for the given key.
bool DBEngine::dbGetRecordByKey(uint32_t key, LogEntryHeader& header,
    void* payloadBuffer, uint16_t bufferSize)
{
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
    if (!_logHandler.read((uint8_t*)&header, sizeof(header), bytesRead) ||
        bytesRead != sizeof(header)) {
        _logHandler.close();
        return false;
    }
    if (header.length > bufferSize) {
        _logHandler.close();
        return false;
    }
    if (!_logHandler.read((uint8_t*)payloadBuffer, header.length, bytesRead) ||
        bytesRead != header.length) {
        _logHandler.close();
        return false;
    }
    _logHandler.close();
    return true;
}
