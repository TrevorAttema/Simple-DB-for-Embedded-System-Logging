#include "DBEngine.h"
#include "IFileHandler.h"
#include "Instrumentation.h"   // New instrumentation header.
#include <stdio.h>
#include <string.h>
#include <time.h>


// Define our own MIN macro since STL is not permitted.
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif


//
// validateIndex()
//   A simple function to check for index corruption.
//   In this example, we load the first page and verify that the keys are in sorted order.
//   (More extensive checks—such as verifying a magic number or checksum—could be added.)
//
bool DBEngine::validateIndex(void) {
    // If the index is empty, there's nothing to validate.
    if (_indexCount == 0)
        return true;

    // Load the first page.
    if (!loadIndexPage(0)) {
        DEBUG_PRINT("validateIndex: Failed to load page 0.\n");
        return false;
    }
    // Determine how many entries are in the first page.
    uint32_t entries = MIN(MAX_INDEX_ENTRIES, _indexCount);
    for (uint32_t i = 0; i < entries - 1; i++) {
        if (_indexPage[i].key > _indexPage[i + 1].key) {
            DEBUG_PRINT("validateIndex: Corruption detected in page 0 at entry %u (key %u > %u).\n",
                i, _indexPage[i].key, _indexPage[i + 1].key);
            return false;
        }
    }
    return true;
}

//
// saveIndexHeader()
//   Writes the header (the total _indexCount) at the start of the index file.
//
bool DBEngine::saveIndexHeader(void) {
    SCOPE_TIMER("DBEngine::saveIndexHeader");
    size_t bytesWritten = 0;
    DEBUG_PRINT("saveIndexHeader: Opening file %s for update...\n", _indexFileName);
    if (!_indexHandler.open(_indexFileName, "rb+")) {
        DEBUG_PRINT("saveIndexHeader: File not openable in rb+ mode, trying wb mode.\n");
        if (!_indexHandler.open(_indexFileName, "wb"))
            return false;
    }
    if (!_indexHandler.seek(0)) {
        DEBUG_PRINT("saveIndexHeader: Seek to 0 failed.\n");
        _indexHandler.close();
        return false;
    }
    DEBUG_PRINT("saveIndexHeader: Writing _indexCount = %u bytes at offset 0...\n", _indexCount);
    if (!_indexHandler.write((const uint8_t*)&_indexCount, sizeof(_indexCount), bytesWritten) ||
        bytesWritten != sizeof(_indexCount)) {
        DEBUG_PRINT("saveIndexHeader: Write failed (wrote %zu bytes, expected %zu).\n", bytesWritten, sizeof(_indexCount));
        _indexHandler.close();
        return false;
    }
    _indexHandler.close();
    DEBUG_PRINT("saveIndexHeader: Done. _indexCount = %u\n", _indexCount);
    return true;
}

//
// loadIndexHeader()
//   Reads the header from the index file and initializes _indexCount.
//   (If the file does not exist, _indexCount is set to 0.)
//
bool DBEngine::loadIndexHeader(void) {
    SCOPE_TIMER("DBEngine::loadIndexHeader");
    size_t bytesRead = 0;
    DEBUG_PRINT("loadIndexHeader: Opening file %s in rb mode...\n", _indexFileName);
    if (!_indexHandler.open(_indexFileName, "rb")) {
        DEBUG_PRINT("loadIndexHeader: File not found. Setting _indexCount = 0.\n");
        _indexCount = 0;
        return true;
    }
    if (!_indexHandler.read((uint8_t*)&_indexCount, sizeof(_indexCount), bytesRead) ||
        bytesRead != sizeof(_indexCount)) {
        DEBUG_PRINT("loadIndexHeader: Read failed (read %zu bytes, expected %zu).\n", bytesRead, sizeof(_indexCount));
        _indexHandler.close();
        return false;
    }
    _indexHandler.close();
    DEBUG_PRINT("loadIndexHeader: _indexCount = %u\n", _indexCount);
    return true;
}

//
// flushIndexPage()
//   Writes the current in-memory page (_indexPage) to disk if it’s dirty.
//   (Only flush when the page is full or when required by a page switch.)
//
bool DBEngine::flushIndexPage(void) {
    SCOPE_TIMER("DBEngine::flushIndexPage");

    if (!_pageDirty) {
        DEBUG_PRINT("flushIndexPage: No flush needed; _pageDirty is false.\n");
        return true;
    }

    DEBUG_PRINT("flushIndexPage: Flushing page %u. _indexCount = %u\n", _currentPageNumber, _indexCount);
    if (!_indexHandler.open(_indexFileName, "rb+")) {
        DEBUG_PRINT("flushIndexPage: Failed to open file %s in rb+ mode.\n", _indexFileName);
        return false;
    }

    size_t pageOffset = sizeof(_indexCount) + _currentPageNumber * sizeof(IndexEntry) * MAX_INDEX_ENTRIES;
    DEBUG_PRINT("flushIndexPage: Seeking to offset %zu\n", pageOffset);
    if (!_indexHandler.seek((uint32_t)pageOffset)) {
        DEBUG_PRINT("flushIndexPage: Seek failed.\n");
        _indexHandler.close();
        return false;
    }

    // Determine how many entries are in this page.
    uint32_t pageFirstIndex = _currentPageNumber * MAX_INDEX_ENTRIES;
    uint32_t entriesInPage = 0;
    if (_indexCount > pageFirstIndex)
        entriesInPage = MIN((uint32_t)MAX_INDEX_ENTRIES, _indexCount - pageFirstIndex);
    size_t bytesToWrite = entriesInPage * sizeof(IndexEntry);

    DEBUG_PRINT("flushIndexPage: Writing %zu bytes (entriesInPage = %u)...\n", bytesToWrite, entriesInPage);
    size_t bytesWritten = 0;
    if (!_indexHandler.write((const uint8_t*)_indexPage, bytesToWrite, bytesWritten) ||
        bytesWritten != bytesToWrite) {
        DEBUG_PRINT("flushIndexPage: Write failed (wrote %zu bytes, expected %zu).\n", bytesWritten, bytesToWrite);
        _indexHandler.close();
        return false;
    }
    _indexHandler.close();

    // Now update the header to reflect the new _indexCount.
    if (!saveIndexHeader()) {
        DEBUG_PRINT("flushIndexPage: Failed to update the index header.\n");
        return false;
    }

    _pageDirty = false;

    DEBUG_PRINT("flushIndexPage: Flush successful.\n");
    return true;
}

//
// loadIndexPage()
//   Loads the specified page (0-based) into the in-memory page buffer (_indexPage).
//   If the file does not contain enough data for a full page, the remainder is zero-filled.
//
bool DBEngine::loadIndexPage(uint32_t pageNumber) {
    SCOPE_TIMER("DBEngine::loadIndexPage");
    DEBUG_PRINT("loadIndexPage: Requesting page %u. Current _indexCount = %u\n", pageNumber, _indexCount);

    if (!flushIndexPage()) {
        DEBUG_PRINT("loadIndexPage: Flush failed.\n");
        return false;
    }

    if (!_indexHandler.open(_indexFileName, "rb")) {
        DEBUG_PRINT("loadIndexPage: Failed to open file %s in rb mode.\n", _indexFileName);
        return false;
    }

    size_t pageOffset = sizeof(_indexCount) + pageNumber * sizeof(IndexEntry) * MAX_INDEX_ENTRIES;
    DEBUG_PRINT("loadIndexPage: Seeking to offset %zu\n", pageOffset);
    if (!_indexHandler.seek((uint32_t)pageOffset)) {
        DEBUG_PRINT("loadIndexPage: Seek failed.\n");
        _indexHandler.close();
        return false;
    }

    uint32_t expectedEntries = 0;
    if (_indexCount > pageNumber * MAX_INDEX_ENTRIES)
        expectedEntries = MIN((uint32_t)MAX_INDEX_ENTRIES, _indexCount - pageNumber * MAX_INDEX_ENTRIES);
    size_t bytesExpected = expectedEntries * sizeof(IndexEntry);
    DEBUG_PRINT("loadIndexPage: Expected entries: %u, bytesExpected: %zu\n", expectedEntries, bytesExpected);

    size_t bytesRead = 0;
    bool readSuccess = _indexHandler.read((uint8_t*)_indexPage, bytesExpected, bytesRead);
    _indexHandler.close();

    if (!readSuccess && bytesRead > 0) {
        DEBUG_PRINT("loadIndexPage: Partial read (read returned false) with %zu bytes read (expected %zu).\n", bytesRead, bytesExpected);
    }
    else if (!readSuccess && bytesRead == 0) {
        DEBUG_PRINT("loadIndexPage: Read returned false with 0 bytes read; assuming no data on disk yet.\n");
        bytesRead = 0;
    }

    DEBUG_PRINT("loadIndexPage: Read %zu bytes (expected %zu).\n", bytesRead, bytesExpected);
    if (bytesRead < bytesExpected) {
        DEBUG_PRINT("loadIndexPage: Partial read; zero-filling remaining %zu bytes.\n", bytesExpected - bytesRead);
        memset((uint8_t*)_indexPage + bytesRead, 0, bytesExpected - bytesRead);
    }

    _currentPageNumber = pageNumber;
    _pageLoaded = true;
    _pageDirty = false;
    DEBUG_PRINT("loadIndexPage: Page %u loaded successfully.\n", pageNumber);
    return true;
}

//
// getIndexEntry()
//   Retrieves the index entry at the given global index.
//   Loads the appropriate page if necessary.
//
bool DBEngine::getIndexEntry(uint32_t globalIndex, IndexEntry& entry) {
    SCOPE_TIMER("DBEngine::getIndexEntry");
    uint32_t page = globalIndex / MAX_INDEX_ENTRIES;
    uint32_t offset = globalIndex % MAX_INDEX_ENTRIES;
    DEBUG_PRINT("getIndexEntry: globalIndex = %u, page = %u, offset = %u\n", globalIndex, page, offset);
    if (!_pageLoaded || _currentPageNumber != page) {
        DEBUG_PRINT("getIndexEntry: Loading page %u...\n", page);
        if (!loadIndexPage(page))
            return false;
    }
    entry = _indexPage[offset];
    DEBUG_PRINT("getIndexEntry: Retrieved entry: key=%u, offset=%u, status=%u\n", entry.key, entry.offset, entry.status);
    return true;
}

//
// setIndexEntry()
//   Updates the index entry at the given global index in the in-memory page,
//   marks the page as dirty so it will be flushed later.
//
bool DBEngine::setIndexEntry(uint32_t globalIndex, const IndexEntry& entry) {
    SCOPE_TIMER("DBEngine::setIndexEntry");
    uint32_t page = globalIndex / MAX_INDEX_ENTRIES;
    uint32_t offset = globalIndex % MAX_INDEX_ENTRIES;
    DEBUG_PRINT("setIndexEntry: globalIndex = %u, page = %u, offset = %u, new key=%u\n", globalIndex, page, offset, entry.key);
    if (!_pageLoaded || _currentPageNumber != page) {
        DEBUG_PRINT("setIndexEntry: Loading page %u...\n", page);
        if (!loadIndexPage(page))
            return false;
    }
    _indexPage[offset] = entry;
    _pageDirty = true;
    DEBUG_PRINT("setIndexEntry: Entry set; marking page dirty.\n");
    return true;
}

//
// splitPageAndInsert()
//   Handles the case where the target page is full by splitting the page
//   into two. The current page is split at a fixed split point (half full).
//   Then, depending on the insertion offset, the new entry is inserted
//   into either the first half (current page) or the new page. Both pages
//   are then flushed and the global index count is updated.
//
bool DBEngine::splitPageAndInsert(uint32_t targetPage, uint32_t offsetInPage,
    uint32_t key, uint32_t recordOffset, uint8_t status) {
    SCOPE_TIMER("DBEngine::splitPageAndInsert");
    DEBUG_PRINT("splitPageAndInsert: Splitting page %u at offset %u for new key=%u\n", targetPage, offsetInPage, key);

    // Assume the target page is loaded and is full.
    uint32_t fullCount = MAX_INDEX_ENTRIES;  // current page is full.
    uint32_t splitIndex = MAX_INDEX_ENTRIES / 2;

    // Prepare a temporary buffer for the new page.
    IndexEntry newPageBuffer[MAX_INDEX_ENTRIES];
    // Copy the second half of the target page into the new page buffer.
    memcpy(newPageBuffer, &_indexPage[splitIndex], (MAX_INDEX_ENTRIES - splitIndex) * sizeof(IndexEntry));

    // Determine in which half the new entry should be inserted.
    if (offsetInPage < splitIndex) {
        // New entry belongs in the current page.
        size_t numToShift = splitIndex - offsetInPage;
        if (numToShift > 0) {
            memmove(&_indexPage[offsetInPage + 1], &_indexPage[offsetInPage], numToShift * sizeof(IndexEntry));
        }
        _indexPage[offsetInPage].key = key;
        _indexPage[offsetInPage].offset = recordOffset;
        _indexPage[offsetInPage].status = status;
    }
    else {
        // New entry belongs in the new page.
        uint32_t newOffset = offsetInPage - splitIndex;
        size_t numToShift = (MAX_INDEX_ENTRIES - splitIndex) - newOffset;
        if (numToShift > 0) {
            memmove(&newPageBuffer[newOffset + 1], &newPageBuffer[newOffset], numToShift * sizeof(IndexEntry));
        }
        newPageBuffer[newOffset].key = key;
        newPageBuffer[newOffset].offset = recordOffset;
        newPageBuffer[newOffset].status = status;
    }

    // Update the global index count.
    _indexCount++;  // one new entry inserted.

    // Flush the current (split) page.
    _pageDirty = true;
    if (!flushIndexPage()) {
        DEBUG_PRINT("splitPageAndInsert: Failed to flush current page after split.\n");
        return false;
    }

    // Write out the new page.
    // New page number is targetPage + 1.
    uint32_t newPageNumber = targetPage + 1;
    if (!_indexHandler.open(_indexFileName, "rb+")) {
        if (!_indexHandler.open(_indexFileName, "wb"))
            return false;
    }
    size_t newPageOffset = sizeof(_indexCount) + newPageNumber * sizeof(IndexEntry) * MAX_INDEX_ENTRIES;
    if (!_indexHandler.seek((uint32_t)newPageOffset)) {
        DEBUG_PRINT("splitPageAndInsert: Seek failed for new page offset.\n");
        _indexHandler.close();
        return false;
    }
    size_t bytesToWrite = (MAX_INDEX_ENTRIES - splitIndex) * sizeof(IndexEntry);
    size_t bytesWritten = 0;
    if (!_indexHandler.write((const uint8_t*)newPageBuffer, bytesToWrite, bytesWritten) ||
        bytesWritten != bytesToWrite) {
        DEBUG_PRINT("splitPageAndInsert: Write failed for new page (wrote %zu bytes, expected %zu).\n", bytesWritten, bytesToWrite);
        _indexHandler.close();
        return false;
    }
    _indexHandler.close();

    // After splitting, update header with the new _indexCount.
    if (!saveIndexHeader())
        return false;

    // The current page (_indexPage) still holds the first half (plus any shifted entries).
    _pageDirty = false;
    DEBUG_PRINT("splitPageAndInsert: Split and insertion successful. New _indexCount = %u\n", _indexCount);
    return true;
}

bool DBEngine::insertIndexEntry(uint32_t key, uint32_t offset, uint8_t status) {
    SCOPE_TIMER("DBEngine::insertIndexEntry");
    DEBUG_PRINT("insertIndexEntry: Inserting new entry with key=%u, offset=%u, status=%u\n", key, offset, status);

    // Use binary search to find the global insertion position.
    uint32_t low = 0, high = _indexCount;
    while (low < high) {
        uint32_t mid = (low + high) / 2;
        IndexEntry entry;
        if (!getIndexEntry(mid, entry))
            return false;
        if (entry.key < key)
            low = mid + 1;
        else
            high = mid;
    }
    uint32_t pos = low;
    DEBUG_PRINT("insertIndexEntry: Global insertion position = %u\n", pos);

    // *** Uniqueness check ***
    // If pos is within bounds, check if the key already exists.
    if (pos < _indexCount) {
        IndexEntry entry;
        if (!getIndexEntry(pos, entry))
            return false;
        if (entry.key == key) {
            DEBUG_PRINT("insertIndexEntry: Duplicate key detected (key=%u at index %u).\n", key, pos);
            // Return false or an error code to signal the collision.
            return false;
        }
    }
    // Alternatively, if _indexCount > 0 and pos > 0, you may also want to check the previous entry:
    if (pos > 0) {
        IndexEntry prevEntry;
        if (!getIndexEntry(pos - 1, prevEntry))
            return false;
        if (prevEntry.key == key) {
            DEBUG_PRINT("insertIndexEntry: Duplicate key detected (key=%u at index %u).\n", key, pos - 1);
            return false;
        }
    }
    // *** End Uniqueness Check ***

    // Determine target page and offset within that page.
    uint32_t targetPage = pos / MAX_INDEX_ENTRIES;
    uint32_t offsetInPage = pos % MAX_INDEX_ENTRIES;

    // Load the target page if not already loaded.
    if (!_pageLoaded || _currentPageNumber != targetPage) {
        if (!loadIndexPage(targetPage))
            return false;
    }
    // Determine how many entries are currently in the target page.
    uint32_t pageFirstIndex = targetPage * MAX_INDEX_ENTRIES;
    uint32_t entriesInPage = MIN(MAX_INDEX_ENTRIES, _indexCount - pageFirstIndex);

    if (entriesInPage < MAX_INDEX_ENTRIES) {
        // There is room in the current page.
        size_t numToShift = entriesInPage - offsetInPage;
        if (numToShift > 0) {
            memmove(&_indexPage[offsetInPage + 1],
                    &_indexPage[offsetInPage],
                    numToShift * sizeof(IndexEntry));
        }
        // Insert the new entry.
        _indexPage[offsetInPage].key = key;
        _indexPage[offsetInPage].offset = offset;
        _indexPage[offsetInPage].status = status;
        _indexCount++;
        _pageDirty = true;
        // If after insertion the page becomes full, flush it.
        if (entriesInPage + 1 == MAX_INDEX_ENTRIES) {
            if (!flushIndexPage())
                return false;
        }
    }
    else {
        // The target page is full: perform a page split and insert the new entry.
        if (!splitPageAndInsert(targetPage, offsetInPage, key, offset, status))
            return false;
    }

    DEBUG_PRINT("insertIndexEntry: New entry inserted at global position %u\n", pos);
    return true;
}


//
// searchIndex()
//   Performs a binary search (using paging) for an exact key.
//   If found, sets *foundIndex to the matching global index.
//
bool DBEngine::searchIndex(uint32_t key, uint32_t* foundIndex) const {
    SCOPE_TIMER("DBEngine::searchIndex");
    uint32_t low = 0;
    uint32_t high = _indexCount;
    DEBUG_PRINT("searchIndex: Searching for key=%u in range [0, %u)\n", key, _indexCount);
    while (low < high) {
        uint32_t mid = (low + high) / 2;
        IndexEntry entry;
        if (!const_cast<DBEngine*>(this)->getIndexEntry(mid, entry))
            return false;
        DEBUG_PRINT("searchIndex: mid=%u, entry.key=%u\n", mid, entry.key);
        if (entry.key == key) {
            *foundIndex = mid;
            DEBUG_PRINT("searchIndex: Found key at index %u\n", mid);
            return true;
        }
        else if (entry.key < key) {
            low = mid + 1;
        }
        else {
            high = mid;
        }
    }
    DEBUG_PRINT("searchIndex: Key not found.\n");
    return false;
}

//
// findIndexEntry()
//   Searches for an index entry by key and, if found, returns its file offset.
//
bool DBEngine::findIndexEntry(uint32_t key, uint32_t& offset) const {
    SCOPE_TIMER("DBEngine::findIndexEntry");
    uint32_t idx;
    if (searchIndex(key, &idx)) {
        IndexEntry entry;
        if (!const_cast<DBEngine*>(this)->getIndexEntry(idx, entry))
            return false;
        offset = entry.offset;
        DEBUG_PRINT("findIndexEntry: Found key=%u at index %u with offset=%u\n", key, idx, offset);
        return true;
    }
    DEBUG_PRINT("findIndexEntry: Key=%u not found.\n", key);
    return false;
}

//
// B-Tree–Style Search Methods
//
bool DBEngine::btreeFindKey(uint32_t key, uint32_t* index) {
    SCOPE_TIMER("DBEngine::btreeFindKey");
    uint32_t low = 0, high = _indexCount;
    DEBUG_PRINT("btreeFindKey: Searching for key=%u\n", key);
    while (low < high) {
        uint32_t mid = (low + high) / 2;
        IndexEntry entry;
        if (!getIndexEntry(mid, entry))
            return false;
        if (entry.key == key) {
            *index = mid;
            DEBUG_PRINT("btreeFindKey: Found key at index %u\n", mid);
            return true;
        }
        else if (entry.key < key) {
            low = mid + 1;
        }
        else {
            high = mid;
        }
    }
    DEBUG_PRINT("btreeFindKey: Key not found.\n");
    return false;
}

bool DBEngine::btreeLocateKey(uint32_t key, uint32_t* index) {
    SCOPE_TIMER("DBEngine::btreeLocateKey");
    uint32_t low = 0, high = _indexCount;
    uint32_t result = _indexCount;
    DEBUG_PRINT("btreeLocateKey: Locating key=%u\n", key);
    while (low < high) {
        uint32_t mid = (low + high) / 2;
        IndexEntry entry;
        if (!getIndexEntry(mid, entry))
            return false;
        if (entry.key >= key) {
            result = mid;
            high = mid;
        }
        else {
            low = mid + 1;
        }
    }
    if (result < _indexCount) {
        *index = result;
        DEBUG_PRINT("btreeLocateKey: Located key at index %u\n", result);
        return true;
    }
    DEBUG_PRINT("btreeLocateKey: Key not located.\n");
    return false;
}

bool DBEngine::btreeNextKey(uint32_t currentIndex, uint32_t* nextIndex) {
    SCOPE_TIMER("DBEngine::btreeNextKey");
    if (currentIndex + 1 < _indexCount) {
        *nextIndex = currentIndex + 1;
        DEBUG_PRINT("btreeNextKey: Next key index = %u\n", *nextIndex);
        return true;
    }
    DEBUG_PRINT("btreeNextKey: No next key exists.\n");
    return false;
}

bool DBEngine::btreePrevKey(uint32_t currentIndex, uint32_t* prevIndex) {
    SCOPE_TIMER("DBEngine::btreePrevKey");
    if (currentIndex > 0) {
        *prevIndex = currentIndex - 1;
        DEBUG_PRINT("btreePrevKey: Previous key index = %u\n", *prevIndex);
        return true;
    }
    DEBUG_PRINT("btreePrevKey: No previous key exists.\n");
    return false;
}

//
// dbFindRecordsByStatus()
//   Searches the index for records with the specified status.
//   Fills the provided results array with the global index positions.
//
size_t DBEngine::dbFindRecordsByStatus(uint8_t status, uint32_t results[], size_t maxResults) const {
    SCOPE_TIMER("DBEngine::dbFindRecordsByStatus");
    size_t count = 0;
    DEBUG_PRINT("dbFindRecordsByStatus: Searching for status=%u\n", status);
    for (uint32_t i = 0; i < _indexCount && count < maxResults; i++) {
        IndexEntry entry;
        if (!const_cast<DBEngine*>(this)->getIndexEntry(i, entry))
            break;
        if (entry.status == status) {
            results[count++] = i;
            DEBUG_PRINT("dbFindRecordsByStatus: Found status at index %u\n", i);
        }
    }
    DEBUG_PRINT("dbFindRecordsByStatus: Found %zu matching records.\n", count);
    return count;
}

size_t DBEngine::dbGetIndexCount(void) const {
    SCOPE_TIMER("DBEngine::dbGetIndexCount");
    DEBUG_PRINT("dbGetIndexCount: _indexCount = %u\n", _indexCount);
    return _indexCount;
}

//
// dbBuildIndex()
//   Initializes the in-memory index state by loading the header.
//   (Pages will be loaded on demand. Also performs a simple validation to detect corruption.)
//
bool DBEngine::dbBuildIndex(void) {
    SCOPE_TIMER("DBEngine::dbBuildIndex");
    DEBUG_PRINT("dbBuildIndex: Building index...\n");
    bool result = loadIndexHeader();
    // If the file did not exist, _indexCount was set to 0.
    // Create an empty index file by saving the header.
    if (_indexCount == 0) {
        if (!saveIndexHeader()) {
            DEBUG_PRINT("dbBuildIndex: Failed to create empty index file.\n");
            return false;
        }
    }
    // Validate the index to detect corruption.
    if (!validateIndex()) {
        DEBUG_PRINT("dbBuildIndex: Index corruption detected.\n");
        // Here you might trigger a rebuild from the log file.
        return false;
    }
    DEBUG_PRINT("dbBuildIndex: _indexCount = %u\n", _indexCount);
    return result;
}

