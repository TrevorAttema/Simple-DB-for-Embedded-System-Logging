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

// ANSI color codes for tick and cross.
#define GREEN_TICK "\033[32m[OK]\033[0m"
#define RED_CROSS   "\033[31m[FAIL]\033[0m"

#define STATUS_UPLOADED   0x01  // Record has been uploaded to the cloud.
#define STATUS_CONFIRMED  0x02  // Cloud confirmed receipt.


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

// Test 1: Comprehensive Append Records Test.
//   - Appends a defined number of records (numRecords).
//   - Verifies that the index count is updated correctly after each append.
//   - Verifies that each record can be retrieved and that its size matches expectations.
//   - Prints summary messages (indented) with a green tick if the case passes, or a red cross if it fails.
void testAppendRecords() {
    const uint32_t numRecords = 1000;
    TemperatureRecord rec = { 23.5f, 45.0f, 1, 2, "Test data for TemperatureRecord" };

    std::cout << "Test Comprehensive Append Records Test" << std::endl;

    bool appendSuccess = true;
    auto startTime = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < numRecords; i++) {
        // Update record data for variability.
        rec.temperature += 0.1f;
        rec.humidity += 0.05f;
        // Use the iteration number as the key.
        uint32_t key = i + 1;
        if (!db.append(key, 1, &rec, sizeof(rec))) {
            std::cerr << "    [Append] ERROR: Failed appending record with key " << key << " " << RED_CROSS << std::endl;
            appendSuccess = false;
            break;
        }
        // Verify that the index count increases as expected.
        if (db.indexCount() != i + 1) {
            std::cerr << "    [Append] ERROR: Expected index count " << i + 1
                << " but got " << db.indexCount() << " after inserting key " << key << " " << RED_CROSS << std::endl;
            appendSuccess = false;
            break;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;

    if (appendSuccess)
        std::cout << "    [Append] SUCCESS: Appended " << numRecords << " records in "
        << diff.count() << " seconds. " << GREEN_TICK << std::endl;
    else {
        std::cerr << "    [Append] FAIL: Append stage failed. " << RED_CROSS << std::endl;
        return;
    }

    // -------- Stage 2: Verify Retrieval --------
    bool retrievalSuccess = true;
    for (uint32_t key = 1; key <= numRecords; key++) {
        TemperatureRecord retrieved;
        uint16_t recordSize = 0;
        if (!db.get(key, &retrieved, sizeof(retrieved), &recordSize)) {
            std::cerr << "    [Retrieve] ERROR: Failed retrieving record with key " << key << " " << RED_CROSS << std::endl;
            retrievalSuccess = false;
            break;
        }
        if (recordSize != sizeof(retrieved)) {
            std::cerr << "    [Retrieve] ERROR: Record size mismatch for key " << key
                << " (expected " << sizeof(retrieved) << ", got " << recordSize << ") " << RED_CROSS << std::endl;
            retrievalSuccess = false;
            break;
        }
    }

    if (retrievalSuccess)
        std::cout << "    [Retrieve] SUCCESS: All " << numRecords
        << " records successfully retrieved and verified. " << GREEN_TICK << std::endl;
    else
        std::cerr << "    [Retrieve] FAIL: Retrieval verification failed. " << RED_CROSS << std::endl;
}



// Test 2: Comprehensive Random Retrieval Test
//   - Verifies that the database contains records.
//   - Performs random access retrieval on 80% of the records.
//   - Confirms that the record size is as expected.
//   - Prints summary messages for each stage (indented) with a green tick if successful, or a red cross if failed.
void testRetrieveRecords() {
    std::cout << "Test Comprehensive Random Retrieval Test" << std::endl;

    const size_t bufferSize = 300;
    char payloadBuffer[bufferSize];

    size_t count = db.indexCount();
    if (count == 0) {
        std::cerr << "    [Setup] FAIL: No records available. " << RED_CROSS << std::endl;
        return;
    }
    else {
        std::cout << "    [Setup] SUCCESS: " << count << " records available. " << GREEN_TICK << std::endl;
    }

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    size_t numSamples = (count * 80) / 100;  // Retrieve 80% of the records randomly.

    bool retrievalSuccess = true;
    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < numSamples; i++) {
        uint32_t randIndex = std::rand() % count;
        IndexEntry entry;
        if (!db.getIndexEntry(randIndex, entry)) {
            std::cerr << "    [Random Access] FAIL: Could not retrieve index entry at index " << randIndex << " " << RED_CROSS << std::endl;
            retrievalSuccess = false;
            break;
        }

        uint16_t recordSize = 0;
        if (!db.get(entry.key, payloadBuffer, bufferSize, &recordSize)) {
            std::cerr << "    [Random Access] FAIL: Could not retrieve record with key " << entry.key << " " << RED_CROSS << std::endl;
            retrievalSuccess = false;
            break;
        }
        // Optionally, you might verify recordSize matches an expected size.
        // For this test, we expect it to match the size of TemperatureRecord.
        if (recordSize != sizeof(TemperatureRecord)) {
            std::cerr << "    [Random Access] FAIL: Record size mismatch for key " << entry.key
                << " (expected " << sizeof(TemperatureRecord) << ", got " << recordSize << ") " << RED_CROSS << std::endl;
            retrievalSuccess = false;
            break;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;

    if (retrievalSuccess)
        std::cout << "    [Random Access] SUCCESS: Retrieved " << numSamples
        << " records in " << diff.count() << " seconds. " << GREEN_TICK << std::endl;
    else
        std::cerr << "    [Random Access] FAIL: Random retrieval test failed. " << RED_CROSS << std::endl;
}


// Test 3: Update Record Status Test
//   - Checks that at least one record exists.
//   - Attempts to update the status of the first record to 0xff.
//   - Measures the time taken for the update.
//   - Prints summary messages (indented) with a green tick if successful, or a red cross if failed.
void testUpdateRecordStatus() {
    std::cout << "Test Update Record Status Test" << std::endl;

    size_t count = db.indexCount();
    if (count == 0) {
        std::cerr << "    [Update] FAIL: No records available for status update. " << RED_CROSS << std::endl;
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    if (!db.updateStatus(0, 0xff)) {
        std::cerr << "    [Update] FAIL: Error updating status of record at index 0. " << RED_CROSS << std::endl;
        return;
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = endTime - startTime;

    std::cout << "    [Update] SUCCESS: Updated status of first record in "
        << diff.count() << " seconds. " << GREEN_TICK << std::endl;
}


// Test 4: B-Tree–Style Search Methods
//   - Verifies that the database contains records for searching.
//   - Tests findKey and locateKey for a known key (from the first index entry).
//   - Tests that searching for a missing key fails as expected.
//   - Prints summary messages for each stage with a green tick if successful, or a red cross if failed.
void testBTreeSearch() {
    std::cout << "Test B-Tree–Style Search Methods" << std::endl;

    // -------- Stage 1: Setup --------
    size_t indexCount = db.indexCount();
    if (indexCount == 0) {
        std::cerr << "    [Setup] FAIL: No records available for B-Tree search. " << RED_CROSS << std::endl;
        return;
    }
    else {
        std::cout << "    [Setup] SUCCESS: " << indexCount << " records available. " << GREEN_TICK << std::endl;
    }

    // -------- Stage 2: Retrieve First Record and its Key --------
    IndexEntry firstEntry;
    if (!db.getIndexEntry(0, firstEntry)) {
        std::cerr << "    [First Record] FAIL: Error retrieving first index entry. " << RED_CROSS << std::endl;
        return;
    }
    else {
        std::cout << "    [First Record] SUCCESS: Retrieved first index entry with key "
            << firstEntry.key << ". " << GREEN_TICK << std::endl;
    }
    uint32_t firstKey = firstEntry.key;

    // -------- Stage 3: findKey for First Key --------
    uint32_t foundIndex = 0;
    if (!db.findKey(firstKey, &foundIndex)) {
        std::cerr << "    [FindKey] FAIL: findKey failed for key " << firstKey << ". " << RED_CROSS << std::endl;
    }
    else {
        std::cout << "    [FindKey] SUCCESS: findKey found key " << firstKey << " at index "
            << foundIndex << ". " << GREEN_TICK << std::endl;
    }

    // -------- Stage 4: locateKey for First Key --------
    uint32_t locatedIndex = 0;
    if (!db.locateKey(firstKey, &locatedIndex)) {
        std::cerr << "    [LocateKey] FAIL: locateKey failed for key " << firstKey << ". " << RED_CROSS << std::endl;
    }
    else {
        std::cout << "    [LocateKey] SUCCESS: locateKey found key " << firstKey << " at index "
            << locatedIndex << ". " << GREEN_TICK << std::endl;
    }

    // -------- Stage 5: Searching for a Missing Key --------
    uint32_t missingKey = 1000000;
    foundIndex = 0;
    if (db.findKey(missingKey, &foundIndex)) {
        std::cerr << "    [MissingKey] FAIL: Unexpectedly found record with missing key "
            << missingKey << ". " << RED_CROSS << std::endl;
    }
    else {
        std::cout << "    [MissingKey] SUCCESS: findKey did not find missing key "
            << missingKey << ". " << GREEN_TICK << std::endl;
    }

    locatedIndex = 0;
    if (db.locateKey(missingKey, &locatedIndex)) {
        std::cerr << "    [MissingKey] FAIL: Unexpectedly located record with missing key "
            << missingKey << ". " << RED_CROSS << std::endl;
    }
    else {
        std::cout << "    [MissingKey] SUCCESS: locateKey did not locate missing key "
            << missingKey << ". " << GREEN_TICK << std::endl;
    }
}


// Test 5: Validate Index Offsets
//   - Checks that the index contains at least one entry.
//   - Iterates through all index entries to confirm that at least one offset is non‑zero.
//   - Prints summary messages (indented) with a green tick for success or a red cross for failure.
void testIndexOffsets() {
    std::cout << "Test & Validate Index Offsets" << std::endl;

    size_t count = db.indexCount();
    if (count == 0) {
        std::cerr << "    [Setup] FAIL: Index is empty, nothing to validate. " << RED_CROSS << std::endl;
        return;
    }
    else {
        std::cout << "    [Setup] SUCCESS: " << count << " index entries available. " << GREEN_TICK << std::endl;
    }

    bool anyNonZero = false;
    for (size_t i = 0; i < count; i++) {
        IndexEntry entry;
        if (!db.getIndexEntry(static_cast<uint32_t>(i), entry)) {
            std::cerr << "    [Retrieve] FAIL: Error retrieving index entry " << i << ". " << RED_CROSS << std::endl;
            return;
        }
        if (entry.offset != 0) {
            anyNonZero = true;
        }
    }

    if (anyNonZero)
        std::cout << "    [Validation] SUCCESS: Index offset validation passed. " << GREEN_TICK << std::endl;
    else
        std::cout << "    [Validation] FAIL: Index offset validation failed: all offsets are 0! " << RED_CROSS << std::endl;
}


// Test IoT Usage: Record Insert, Upload, and Cloud Confirmation
//   - Generates a unique key to avoid clashes with other test records.
//   - Inserts a sensor record (TemperatureRecord) using that key.
//   - Searches for the record to obtain its index.
//   - Updates the record's user-defined status to indicate that it has been uploaded.
//   - Updates the record's status again to indicate that cloud receipt has been confirmed.
//   - Verifies after each update that the status is as expected.
//   - Prints summary messages (indented) with a green tick (✓) on success or a red cross (✗) on failure.


void testUpdateAndFindByStatus() {
    std::cout << "Test IoT Usage: Record Insert, Upload, and Cloud Confirmation" << std::endl;

    // -------- Stage 1: Generate a Unique Key and Insert Record --------
    // Generate a unique key by adding an offset to the current index count.
    uint32_t uniqueKey = static_cast<uint32_t>(db.indexCount() + 10000);
    TemperatureRecord rec = { 23.5f, 45.0f, 1, 2, "IoT Sensor Reading" };

    if (!db.append(uniqueKey, 1, &rec, sizeof(rec))) {
        std::cerr << "    [Insert] FAIL: Unable to insert record with key " << uniqueKey << " " << RED_CROSS << std::endl;
        return;
    }
    std::cout << "    [Insert] SUCCESS: Record inserted with unique key " << uniqueKey << " " << GREEN_TICK << std::endl;

    // -------- Stage 2: Locate the Record by Key --------
    uint32_t foundIndex = 0;
    if (!db.searchIndex(uniqueKey, &foundIndex)) {
        std::cerr << "    [Locate] FAIL: Could not locate record with key " << uniqueKey << " " << RED_CROSS << std::endl;
        return;
    }
    std::cout << "    [Locate] SUCCESS: Record with key " << uniqueKey << " located at index "
        << foundIndex << " " << GREEN_TICK << std::endl;

    // -------- Stage 3: Update Status to 'Uploaded' --------
    // Update the record's status to indicate it has been uploaded.
    if (!db.updateStatus(foundIndex, STATUS_UPLOADED)) {
        std::cerr << "    [Upload] FAIL: Unable to update record status to 'Uploaded' for key "
            << uniqueKey << " " << RED_CROSS << std::endl;
        return;
    }
    IndexEntry entry;
    if (!db.getIndexEntry(foundIndex, entry)) {
        std::cerr << "    [Upload] FAIL: Unable to retrieve index entry for key " << uniqueKey << " " << RED_CROSS << std::endl;
        return;
    }
    if (entry.status != STATUS_UPLOADED) {
        std::cerr << "    [Upload] FAIL: Record status is not 'Uploaded' (expected 0x"
            << std::hex << static_cast<unsigned>(STATUS_UPLOADED) << std::dec
            << ", got 0x" << std::hex << static_cast<unsigned>(entry.status) << std::dec << ") " << RED_CROSS << std::endl;
        return;
    }
    std::cout << "    [Upload] SUCCESS: Record status updated to 'Uploaded' (0x"
        << std::hex << static_cast<unsigned>(STATUS_UPLOADED) << std::dec << ") " << GREEN_TICK << std::endl;

    // -------- Stage 4: Update Status to 'Confirmed' --------
    // Now simulate that the cloud confirmed receipt of the record.
    if (!db.updateStatus(foundIndex, STATUS_CONFIRMED)) {
        std::cerr << "    [Confirm] FAIL: Unable to update record status to 'Confirmed' for key "
            << uniqueKey << " " << RED_CROSS << std::endl;
        return;
    }
    if (!db.getIndexEntry(foundIndex, entry)) {
        std::cerr << "    [Confirm] FAIL: Unable to retrieve index entry for key " << uniqueKey
            << " after confirmation " << RED_CROSS << std::endl;
        return;
    }
    if (entry.status != STATUS_CONFIRMED) {
        std::cerr << "    [Confirm] FAIL: Record status is not 'Confirmed' (expected 0x"
            << std::hex << static_cast<unsigned>(STATUS_CONFIRMED) << std::dec
            << ", got 0x" << std::hex << static_cast<unsigned>(entry.status) << std::dec << ") " << RED_CROSS << std::endl;
        return;
    }
    std::cout << "    [Confirm] SUCCESS: Record status updated to 'Confirmed' (0x"
        << std::hex << static_cast<unsigned>(STATUS_CONFIRMED) << std::dec << ") " << GREEN_TICK << std::endl;

    std::cout << "Test IoT Usage: Completed successfully." << std::endl;
}

void testDeleteRecordsComprehensive() {
    std::cout << "Comprehensive Delete Records Test Summary" << std::endl;

    bool overallSuccess = true;

    // -------- Case 1: Delete a non-existent key --------
    uint32_t nonExistentKey = 999999; // key assumed not to exist
    if (!db.deleteRecord(nonExistentKey)) {
        std::cout << "    Case 1: Delete non-existent key (" << nonExistentKey
            << "): SUCCESS (delete failed as expected) " << GREEN_TICK << std::endl;
    }
    else {
        std::cout << "    Case 1: Delete non-existent key (" << nonExistentKey
            << "): FAIL (delete succeeded unexpectedly) " << RED_CROSS << std::endl;
        overallSuccess = false;
    }

    // -------- Case 2: Delete the first and last records --------
    size_t totalRecords = db.indexCount();
    bool case2Success = true;
    if (totalRecords == 0) {
        std::cout << "    Case 2: Delete first/last record: FAIL (no records available) " << RED_CROSS << std::endl;
        case2Success = false;
    }
    else {
        // First record
        IndexEntry firstEntry;
        if (db.getIndexEntry(0, firstEntry) && db.deleteRecord(firstEntry.key)) {
            std::cout << "    Case 2: Delete first record (key " << firstEntry.key
                << "): SUCCESS " << GREEN_TICK << std::endl;
        }
        else {
            std::cout << "    Case 2: Delete first record: FAIL (unable to delete key "
                << firstEntry.key << ") " << RED_CROSS << std::endl;
            case2Success = false;
        }
        // Last record
        IndexEntry lastEntry;
        if (db.getIndexEntry(static_cast<uint32_t>(totalRecords - 1), lastEntry) && db.deleteRecord(lastEntry.key)) {
            std::cout << "    Case 2: Delete last record (key " << lastEntry.key
                << "): SUCCESS " << GREEN_TICK << std::endl;
        }
        else {
            std::cout << "    Case 2: Delete last record: FAIL (unable to delete key "
                << lastEntry.key << ") " << RED_CROSS << std::endl;
            case2Success = false;
        }
    }
    if (!case2Success)
        overallSuccess = false;

    // -------- Case 3: Delete a set of arbitrary records --------
    std::vector<uint32_t> keysToDelete = { 10, 20, 30, 40, 50 };
    bool case3Success = true;
    for (auto key : keysToDelete) {
        if (!db.deleteRecord(key)) {
            std::cout << "    Case 3: Delete record with key " << key << ": FAIL " << RED_CROSS << std::endl;
            case3Success = false;
        }
    }
    if (case3Success)
        std::cout << "    Case 3: Delete arbitrary records: SUCCESS " << GREEN_TICK << std::endl;
    else {
        std::cout << "    Case 3: Delete arbitrary records: FAIL (one or more keys not deleted) " << RED_CROSS << std::endl;
        overallSuccess = false;
    }

    // -------- Case 4: Delete the same record twice --------
    bool case4Success = true;
    if (!keysToDelete.empty()) {
        uint32_t dupKey = keysToDelete.front();
        // The key was deleted in Case 3. Trying to delete it again should succeed as a no-op.
        if (db.deleteRecord(dupKey)) {
            std::cout << "    Case 4: Re-delete record with key " << dupKey
                << ": SUCCESS (no-op as expected) " << GREEN_TICK << std::endl;
        }
        else {
            std::cout << "    Case 4: Re-delete record with key " << dupKey
                << ": FAIL (unexpected error) " << RED_CROSS << std::endl;
            case4Success = false;
        }
    }
    else {
        std::cout << "    Case 4: Re-delete record: SKIPPED (no key available)" << std::endl;
    }
    if (!case4Success)
        overallSuccess = false;

    // -------- Case 5: Reinsert a record with a deleted key --------
    bool case5Success = true;
    TemperatureRecord rec = { 25.0f, 50.0f, 100, 200, "Reinserted record" };
    uint32_t reinsertKey = keysToDelete.empty() ? 10 : keysToDelete.front();
    if (db.append(reinsertKey, 1, &rec, sizeof(rec))) {
        // Verify that the internal deletion flag is cleared.
        uint32_t idx;
        if (db.searchIndex(reinsertKey, &idx)) {
            IndexEntry entry;
            if (db.getIndexEntry(idx, entry)) {
                if ((entry.internal_status & INTERNAL_STATUS_DELETED) == 0) {
                    std::cout << "    Case 5: Reinsert record with key " << reinsertKey
                        << ": SUCCESS (internal deletion flag cleared) " << GREEN_TICK << std::endl;
                }
                else {
                    std::cout << "    Case 5: Reinsert record with key " << reinsertKey
                        << ": FAIL (internal deletion flag not cleared) " << RED_CROSS << std::endl;
                    case5Success = false;
                }
            }
            else {
                std::cout << "    Case 5: Reinsert record with key " << reinsertKey
                    << ": FAIL (could not retrieve index entry) " << RED_CROSS << std::endl;
                case5Success = false;
            }
        }
        else {
            std::cout << "    Case 5: Reinsert record with key " << reinsertKey
                << ": FAIL (key not found after reinsert) " << RED_CROSS << std::endl;
            case5Success = false;
        }
    }
    else {
        std::cout << "    Case 5: Reinsert record with key " << reinsertKey
            << ": FAIL (append failed) " << RED_CROSS << std::endl;
        case5Success = false;
    }
    if (!case5Success)
        overallSuccess = false;

    // -------- Case 6: Verify that internal_status flags are as expected --------
    // For this test, we check that each index entry that was deleted remains marked,
    // and that the record we reinserted now has internal_status == 0.
    bool case6Success = true;
    for (size_t i = 0; i < db.indexCount(); i++) {
        IndexEntry entry;
        if (!db.getIndexEntry(static_cast<uint32_t>(i), entry)) {
            std::cout << "    Case 6: FAIL (could not retrieve index entry at position " << i << ") " << RED_CROSS << std::endl;
            case6Success = false;
            break;
        }
        // For the record that was reinserted, internal_status should be 0.
        if (entry.key == reinsertKey) {
            if (entry.internal_status != 0) {
                std::cout << "    Case 6: FAIL (reinserted key " << reinsertKey << " still marked deleted) " << RED_CROSS << std::endl;
                case6Success = false;
                break;
            }
        }
    }
    if (case6Success)
        std::cout << "    Case 6: Verify internal_status flags: SUCCESS " << GREEN_TICK << std::endl;
    else {
        std::cout << "    Case 6: Verify internal_status flags: FAIL (one or more index entries are incorrect) " << RED_CROSS << std::endl;
        overallSuccess = false;
    }

}


int main() {
    std::cout << "Starting DBEngine Test Application" << std::endl;

    // Delete existing database files to ensure each test run starts fresh.
    // The tests use sequential key values. If the files already exist from a previous
    // run, the same key values will be inserted again and the DB will reject duplicates.
    if (std::remove("LOGFILE.BIN") == 0) {
        std::cout << "Deleted existing LOGFILE.BIN" << std::endl;
    }
    if (std::remove("INDEX.BIN") == 0) {
        std::cout << "Deleted existing INDEX.BIN" << std::endl;
    }

    //DBEngine db(/* appropriate logHandler */, /* appropriate indexHandler */);

    if (!db.open("LOGFILE.BIN", "INDEX.BIN")) {
        std::cerr << "Error opening database files." << std::endl;
        return 1;
    }

    // Uncomment testAppendRecords() if you need to initially populate the DB.
    testAppendRecords();
    testUpdateAndFindByStatus();
    testRetrieveRecords();
    testUpdateRecordStatus();
    testBTreeSearch();
    testIndexOffsets();
    testDeleteRecordsComprehensive();

    PrintInstrumentationReport();

    // Optionally, print database statistics.
    // db.printStats();

    return 0;
}