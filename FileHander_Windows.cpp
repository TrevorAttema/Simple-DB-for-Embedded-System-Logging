#include "FileHandler_Windows.h"

#include <string.h>

WindowsFileHandler::WindowsFileHandler() : _file(NULL) {
    _currentFilename[0] = '\0';
    _currentMode[0] = '\0';
}

WindowsFileHandler::~WindowsFileHandler() {
    close();
}

bool WindowsFileHandler::open(const char* filename, const char* mode) {
    // If a file is already open and the filename matches, reuse it.
    if (_file != NULL && strcmp(_currentFilename, filename) == 0) {
        // Optionally check mode compatibility.
        return true;
    }
    // Otherwise, if a file is already open, close it.
    if (_file != NULL) {
        close();
    }
    _file = fopen(filename, mode);
    if (_file != NULL) {
        strncpy(_currentFilename, filename, MAX_PATH_LENGTH - 1);
        _currentFilename[MAX_PATH_LENGTH - 1] = '\0';
        strncpy(_currentMode, mode, sizeof(_currentMode) - 1);
        _currentMode[sizeof(_currentMode) - 1] = '\0';
        return true;
    }
    return false;
}

void WindowsFileHandler::close() {
    if (_file != NULL) {
        fflush(_file);
        fclose(_file);
        _file = NULL;
        _currentFilename[0] = '\0';
        _currentMode[0] = '\0';
    }
}

bool WindowsFileHandler::seek(uint32_t offset) {
    if (_file == NULL)
        return false;
    return (fseek(_file, offset, SEEK_SET) == 0);
}

uint32_t WindowsFileHandler::tell() {
    if (_file == NULL)
        return 0;
    return (uint32_t)ftell(_file);
}

bool WindowsFileHandler::read(uint8_t* buffer, size_t size, size_t& bytesRead) {
    if (_file == NULL)
        return false;
    bytesRead = fread(buffer, 1, size, _file);
    return (bytesRead == size);
}

bool WindowsFileHandler::write(const uint8_t* buffer, size_t size, size_t& bytesWritten) {
    if (_file == NULL)
        return false;
    bytesWritten = fwrite(buffer, 1, size, _file);
    return (bytesWritten == size);
}

bool WindowsFileHandler::flush() {
    if (_file == NULL)
        return false;
    return (fflush(_file) == 0);
}

bool WindowsFileHandler::seekToEnd() {
    if (_file == NULL)
        return false;
    return (fseek(_file, 0, SEEK_END) == 0);
}
