// testapp.cpp : Defines the entry point for the application.

#include <iostream>
#include <chrono>
#include <cstring>
#include "DBEngine.h"
#include "FileHandler_Windows.h"  // Your Windows implementation of IFileHandler
#include "Instrumentation.h"

// A sample record type.
struct TemperatureRecord {
    float temperature;
    float humidity;
    uint32_t height;
    uint32_t width;
    char name[100];
};

// Global DBEngine instance. (No dynamic memory: created on the stack.)
WindowsFileHandler logHandler;
WindowsFileHandler indexHandler;
DBEngine db(logHandler, indexHandler);

// Test 1: Append many records and time the operation.
void testAppendRecords() {
    std::cout << "\nTest 1: Append records" << std::endl;
    const uint32_t numRecords = 1000;
    TemperatureRecord rec = { 23.5f, 45.0f, 1, 2, "This is some test data for the array"};

    auto startTime = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < numRecords; i++) {
        // For variation, adjust record values.
        rec.temperature += 0.1f;
        rec.humidity += 0.05f;
        if (!db.dbAppendRecord(1, &rec, sizeof(rec))) {
            std::cerr << "Error appending record at iteration " << i << std::endl;
            return;
        }
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;
    std::cout << "Appended " << numRecords << " records in " << diff.count() << " seconds." << std::endl;
}

// Test 2: Retrieve records by index ID and time the operation.
void testRetrieveRecords() {
    std::cout << "\nTest 2: Retrieve records by index ID" << std::endl;
    const size_t bufferSize = 200;
    char payloadBuffer[bufferSize];
    LogEntryHeader header;
    size_t count = db.dbGetIndexCount();
    if (count == 0) {
        std::cerr << "No records available." << std::endl;
        return;
    }
    auto startTime = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < count; i++) {
        if (!db.dbGetRecordByIndexId(static_cast<uint32_t>(i), header, payloadBuffer, bufferSize)) {
            std::cerr << "Error retrieving record at index " << i << std::endl;
            return;
        }
        // (Optional) Process the record.
        TemperatureRecord* temp = reinterpret_cast<TemperatureRecord*>(payloadBuffer);
        // For this test, we do not print every record.
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;
    std::cout << "Retrieved " << count << " records in " << diff.count() << " seconds." << std::endl;
}

// Test 3: Update the status of the first record and time the operation.
void testUpdateRecordStatus() {
    std::cout << "\nTest 3: Update record status" << std::endl;
    size_t count = db.dbGetIndexCount();
    if (count == 0) {
        std::cerr << "No records available for status update." << std::endl;
        return;
    }

    // Use index ID 0 directly (first record) instead of retrieving the record offset.
    auto startTime = std::chrono::high_resolution_clock::now();
    // Update status to 1 (e.g., "Sent") using the index-based update.
    if (!db.dbUpdateRecordStatusByIndexId(0, 0xff)) {
        std::cerr << "Error updating record status." << std::endl;
        return;
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;
    std::cout << "Status update completed in " << diff.count() << " seconds." << std::endl;
}


// Test 4: B-Tree–Style Search Methods.
void testBTreeSearch() {
    std::cout << "\nTest 4: B-Tree Search" << std::endl;
    size_t indexCount = db.dbGetIndexCount();
    if (indexCount == 0) {
        std::cerr << "No records for B-Tree search." << std::endl;
        return;
    }

    IndexEntry firstEntry;
    if (db.getIndexEntry(0, firstEntry)) {
        std::cout << "First index entry key: " << firstEntry.key << std::endl;
    }

    // Retrieve the key of the first record.
    uint32_t firstKey = 0;
    {
        LogEntryHeader header;
        char payloadBuffer[250];
        if (!db.dbGetRecordByIndexId(0, header, payloadBuffer, sizeof(payloadBuffer))) {
            std::cerr << "Error retrieving first record for search test." << std::endl;
            return;
        }
        firstKey = header.key;
    }

    // Search using the first key.
    {
        uint32_t foundIndex = 0;
        if (db.btreeFindKey(firstKey, &foundIndex)) {
            std::cout << "btreeFindKey: Found record with key " << firstKey << " at index " << foundIndex << std::endl;
        }
        else {
            std::cout << "btreeFindKey: Record with key " << firstKey << " not found." << std::endl;
        }

        uint32_t locatedIndex = 0;
        if (db.btreeLocateKey(firstKey, &locatedIndex)) {
            std::cout << "btreeLocateKey: Located record with key >= " << firstKey << " at index " << locatedIndex << std::endl;
        }
        else {
            std::cout << "btreeLocateKey: No record with key >= " << firstKey << " found." << std::endl;
        }
    }

    // Optionally, try a key known not to exist.
    {
        // For example, pick a key far greater than any key in the index.
        uint32_t missingKey = 1000000;
        uint32_t foundIndex = 0;
        if (db.btreeFindKey(missingKey, &foundIndex)) {
            std::cout << "btreeFindKey: Found record with key " << missingKey << " at index " << foundIndex << std::endl;
        }
        else {
            std::cout << "btreeFindKey: Record with key " << missingKey << " not found." << std::endl;
        }

        uint32_t locatedIndex = 0;
        if (db.btreeLocateKey(missingKey, &locatedIndex)) {
            std::cout << "btreeLocateKey: Located record with key >= " << missingKey << " at index " << locatedIndex << std::endl;
        }
        else {
            std::cout << "btreeLocateKey: No record with key >= " << missingKey << " found." << std::endl;
        }
    }
}

void testIndexOffsets() {
    std::cout << "\nTest 5: Validate Index Offsets" << std::endl;
    size_t count = db.dbGetIndexCount();
    if (count == 0) {
        std::cerr << "Index is empty, nothing to validate." << std::endl;
        return;
    }

    bool anyNonZero = false;
    for (size_t i = 0; i < count; i++) {
        IndexEntry entry;
        if (!db.getIndexEntry(static_cast<uint32_t>(i), entry)) {
            std::cerr << "Error retrieving index entry " << i << std::endl;
            continue;
        }
        std::cout << "Index entry " << i
            << " key=" << entry.key
            << " offset=" << entry.offset
            << " status=" << static_cast<unsigned>(entry.status)
            << std::endl;
        if (entry.offset != 0) {
            anyNonZero = true;
        }
    }

    if (anyNonZero)
        std::cout << "Validation passed: At least one index entry has a non-zero offset." << std::endl;
    else
        std::cout << "Validation failed: All index entries have offset 0!" << std::endl;
}


int main() {
    std::cout << "Starting DBEngine Test Application" << std::endl;

    // Open the database with 8.3 filenames (for example "LOGFILE.BIN", "INDEX.BIN").
    db.dbOpen("LOGFILE.BIN", "INDEX.BIN");

    // Build (or load) the in–memory index.
    if (!db.dbBuildIndex()) {
        std::cerr << "Error building index" << std::endl;
        return 1;
    }

    // Run the tests:
    testAppendRecords();
    //testRetrieveRecords();
    //testUpdateRecordStatus();
    testBTreeSearch();
    //testIndexOffsets();

    // Print the instrumentation report before exiting.
    PrintInstrumentationReport();

    return 0;
}
