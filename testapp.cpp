// testapp.cpp : Defines the entry point for the application.
// testapp.cpp : Defines the entry point for the application.

#include <iostream>
#include <chrono>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <ctime>
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

// Global DBEngine instance (allocated on the stack).
WindowsFileHandler logHandler;
WindowsFileHandler indexHandler;
DBEngine db(logHandler, indexHandler);

// Test 1: Append many records.
void testAppendRecords() {
    const uint32_t numRecords = 1000;
    TemperatureRecord rec = { 23.5f, 45.0f, 1, 2, "Test data for TemperatureRecord" };

    auto startTime = std::chrono::high_resolution_clock::now();
    for (uint32_t i = 0; i < numRecords; i++) {
        rec.temperature += 0.1f;
        rec.humidity += 0.05f;
        // Use the iteration number as the key.
        if (!db.append(i + 1, 1, &rec, sizeof(rec))) {
            std::cerr << "Error appending record at iteration " << i << std::endl;
            return;
        }
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;
    std::cout << "Test 1: Successfully appended " << numRecords << " records in "
        << diff.count() << " seconds." << std::endl;
}

// Test 2: Retrieve records by key (random access).
void testRetrieveRecords() {
    const size_t bufferSize = 300;
    char payloadBuffer[bufferSize];

    size_t count = db.indexCount();
    if (count == 0) {
        std::cerr << "Test 2: No records available." << std::endl;
        return;
    }

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    size_t numSamples = (count * 80) / 100;  // Retrieve 80% of the records randomly.

    auto startTime = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < numSamples; i++) {
        uint32_t randIndex = std::rand() % count;
        IndexEntry entry;
        if (!db.getIndexEntry(randIndex, entry)) {
            std::cerr << "Test 2: Error retrieving index entry at index " << randIndex << std::endl;
            return;
        }
        uint16_t recordSize = 0;
        if (!db.get(entry.key, payloadBuffer, bufferSize, &recordSize)) {
            std::cerr << "Test 2: Error retrieving record with key " << entry.key << std::endl;
            return;
        }
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;
    std::cout << "Test 2: Successfully retrieved " << numSamples
        << " records in " << diff.count() << " seconds." << std::endl;
}

// Test 3: Update the status of the first record.
void testUpdateRecordStatus() {
    size_t count = db.indexCount();
    if (count == 0) {
        std::cerr << "Test 3: No records available for status update." << std::endl;
        return;
    }
    auto startTime = std::chrono::high_resolution_clock::now();
    if (!db.updateStatus(0, 0xff)) {
        std::cerr << "Test 3: Error updating status of record at index 0." << std::endl;
        return;
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;
    std::cout << "Test 3: Successfully updated status of first record in "
        << diff.count() << " seconds." << std::endl;
}

// Test 4: B-Tree–Style Search Methods.
void testBTreeSearch() {
    size_t indexCount = db.indexCount();
    if (indexCount == 0) {
        std::cerr << "Test 4: No records available for B-Tree search." << std::endl;
        return;
    }

    // Use the key from the first index entry.
    IndexEntry firstEntry;
    if (!db.getIndexEntry(0, firstEntry)) {
        std::cerr << "Test 4: Error retrieving first index entry." << std::endl;
        return;
    }
    uint32_t firstKey = firstEntry.key;

    uint32_t foundIndex = 0;
    if (!db.findKey(firstKey, &foundIndex)) {
        std::cerr << "Test 4: findKey failed for key " << firstKey << std::endl;
    }

    uint32_t locatedIndex = 0;
    if (!db.locateKey(firstKey, &locatedIndex)) {
        std::cerr << "Test 4: locateKey failed for key " << firstKey << std::endl;
    }

    // Test with a missing key.
    uint32_t missingKey = 1000000;
    foundIndex = 0;
    if (db.findKey(missingKey, &foundIndex)) {
        std::cerr << "Test 4: Unexpectedly found record with missing key " << missingKey << std::endl;
    }
    locatedIndex = 0;
    if (db.locateKey(missingKey, &locatedIndex)) {
        std::cerr << "Test 4: Unexpectedly located record with missing key " << missingKey << std::endl;
    }

    std::cout << "Test 4: B-Tree search tests completed successfully." << std::endl;
}

// Test 5: Validate Index Offsets.
void testIndexOffsets() {
    size_t count = db.indexCount();
    if (count == 0) {
        std::cerr << "Test 5: Index is empty, nothing to validate." << std::endl;
        return;
    }

    bool anyNonZero = false;
    for (size_t i = 0; i < count; i++) {
        IndexEntry entry;
        if (!db.getIndexEntry(static_cast<uint32_t>(i), entry)) {
            std::cerr << "Test 5: Error retrieving index entry " << i << std::endl;
            return;
        }
        if (entry.offset != 0) {
            anyNonZero = true;
        }
    }

    if (anyNonZero)
        std::cout << "Test 5: Index offset validation passed." << std::endl;
    else
        std::cout << "Test 5: Index offset validation failed: all offsets are 0!" << std::endl;
}

// Test 6: Update the status of a random subset of records and find by status.
void testUpdateAndFindByStatus() {
    size_t totalRecords = db.indexCount();
    if (totalRecords == 0) {
        std::cerr << "Test 6: No records available for update/find test." << std::endl;
        return;
    }

    size_t numToUpdate = (totalRecords * 20) / 100;
    const uint8_t newStatus = 0xAA;

    std::srand(static_cast<unsigned>(std::time(nullptr())));
    std::vector<uint32_t> updatedIndices;
    for (size_t i = 0; i < numToUpdate; i++) {
        uint32_t randIndex = std::rand() % totalRecords;
        if (db.updateStatus(randIndex, newStatus)) {
            updatedIndices.push_back(randIndex);
        }
        else {
            std::cerr << "Test 6: Failed to update status at index " << randIndex << std::endl;
        }
    }

    const size_t maxResults = totalRecords;
    std::vector<uint32_t> foundIndices(maxResults);
    size_t foundCount = db.findByStatus(newStatus, foundIndices.data(), maxResults);

    if (foundCount != updatedIndices.size()) {
        std::cerr << "Test 6: Mismatch: " << updatedIndices.size()
            << " updated vs " << foundCount << " found." << std::endl;
    }
    else {
        bool allMatch = true;
        for (size_t i = 0; i < foundCount; i++) {
            IndexEntry entry;
            if (!db.getIndexEntry(foundIndices[i], entry)) {
                std::cerr << "Test 6: Error retrieving index entry at " << foundIndices[i] << std::endl;
                allMatch = false;
                break;
            }
            if (entry.status != newStatus) {
                std::cerr << "Test 6: Status mismatch at index " << foundIndices[i] << std::endl;
                allMatch = false;
            }
        }
        if (allMatch)
            std::cout << "Test 6: Successfully updated and found records with status "
            << static_cast<unsigned>(newStatus) << "." << std::endl;
        else
            std::cout << "Test 6: Some records did not match the expected status." << std::endl;
    }
}

int main() {
    std::cout << "Starting DBEngine Test Application" << std::endl;

    if (!db.open("LOGFILE.BIN", "INDEX.BIN")) {
        std::cerr << "Error opening database files." << std::endl;
        return 1;
    }

    // Uncomment testAppendRecords() if you need to populate the DB.
    testAppendRecords();
    testUpdateAndFindByStatus();
    testRetrieveRecords();
    testUpdateRecordStatus();
    testBTreeSearch();
    testIndexOffsets();

    PrintInstrumentationReport();

    db.printStats();

    return 0;
}
