#ifndef IFILEHANDLER_H
#define IFILEHANDLER_H

#include <stddef.h>
#include <stdint.h>

class IFileHandler {
public:
    virtual ~IFileHandler() {}

    // Open the file with the given mode (e.g., "rb", "wb", "rb+", "ab").
    virtual bool open(const char* filename, const char* mode) = 0;

    // Close the file.
    virtual void close() = 0;

    // Seek to the specified offset.
    virtual bool seek(uint32_t offset) = 0;

    // Seek to the end of the file.
    virtual bool seekToEnd() = 0;

    // Return the current file position.
    virtual uint32_t tell() = 0;

    // Read 'size' bytes into buffer. Returns true if successful;
    // bytesRead is updated.
    virtual bool read(uint8_t* buffer, size_t size, size_t& bytesRead) = 0;

    // Write 'size' bytes from buffer. Returns true if successful;
    // bytesWritten is updated.
    virtual bool write(const uint8_t* buffer, size_t size, size_t& bytesWritten) = 0;
};


#endif // IFILEHANDLER_H
