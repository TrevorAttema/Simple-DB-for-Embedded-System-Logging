#ifndef FILEHANDLER_WINDOWS_H
#define FILEHANDLER_WINDOWS_H

#include "IFileHandler.h"
#include <stdio.h>

#define MAX_PATH_LENGTH 13  // 8.3 filename: 8+1+3+1

class WindowsFileHandler : public IFileHandler {
public:
    WindowsFileHandler();
    virtual ~WindowsFileHandler();

    // Open the file with the given mode (e.g., "rb", "wb", "rb+", "ab").
    virtual bool open(const char* filename, const char* mode) override;

    // Close the file.
    virtual void close() override;

    // Seek to the specified offset.
    virtual bool seek(uint32_t offset) override;

    // In FileHandler_Windows.h, within class WindowsFileHandler:
    virtual bool seekToEnd() override;


    // Return the current file position.
    virtual uint32_t tell() override;

    // Read 'size' bytes into buffer. Returns true if successful; bytesRead is updated.
    virtual bool read(uint8_t* buffer, size_t size, size_t& bytesRead) override;

    // Write 'size' bytes from buffer. Returns true if successful; bytesWritten is updated.
    virtual bool write(const uint8_t* buffer, size_t size, size_t& bytesWritten) override;

    // Optionally flush the file.
    bool flush();

private:
    FILE* _file;
    char _currentFilename[MAX_PATH_LENGTH];
    char _currentMode[8];  // Assume mode strings are short (e.g., "rb", "wb", etc.)
};

#endif // FILEHANDLER_WINDOWS_H

