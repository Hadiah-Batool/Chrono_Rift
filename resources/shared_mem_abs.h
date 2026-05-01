#pragma once
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <algorithm>

class SharedMem {
    int fd;
    size_t size;
    void* ptr;
    bool owner;
    std::string name;

public:
    SharedMem(const char* name, size_t size, bool owner, bool create = false)
        : size(size), owner(owner), name(name) { // FIX 1: initialise stored name
        if (create) {
            fd = shm_open(name, O_CREAT | O_RDWR, 0666);
            if (fd == -1) throw std::runtime_error("Failed to create shared memory");
            ftruncate(fd, size);
        } else {
            fd = shm_open(name, O_RDWR, 0666);
            if (fd == -1) throw std::runtime_error("Failed to open shared memory");
        }
        ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("Failed to map shared memory");
        }
    }

    bool isOwner() const {
        return owner;
    }

    void addData(const void* data, size_t dataSize) {
        if (dataSize > size) {
            throw std::runtime_error("Data size exceeds shared memory size");
        }
        std::memcpy(ptr, data, dataSize);
    }

    void readData(void* buffer, size_t bufferSize) const {
        if (bufferSize > size) {
            throw std::runtime_error("Buffer size exceeds shared memory size");
        }
        std::memcpy(buffer, ptr, bufferSize);
    }

    // Sets the shared memory to zero, effectively clearing all data
    void clearAllData() {
        std::memset(ptr, 0, size);
    }

    void resetSize(size_t newSize) {
        if (!owner) {
            throw std::runtime_error("Only owner can reset size");
        }
        munmap(ptr, size);
        if (ftruncate(fd, newSize) == -1) {
            throw std::runtime_error("Failed to resize shared memory");
        }
        ptr = mmap(NULL, newSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            throw std::runtime_error("Failed to remap shared memory");
        }
        size = newSize;
    }

    size_t getSize() const {
        return size;
    }

    void* getPtr() const {
        return ptr;
    }

    ~SharedMem() {
        munmap(ptr, size);
        close(fd);
        if (owner) {
            shm_unlink(name.c_str());
        }
    }
};


class SharedMemPipe {
    SharedMem sharedMem;
    size_t writeOffset;
    size_t readOffset;

public:
    SharedMemPipe(const char* name, size_t size, bool owner, bool create = false)
        : sharedMem(name, size, owner, create), writeOffset(0), readOffset(0) {}

    ~SharedMemPipe() {
        writeOffset = 0;
        readOffset = 0;
    }

    bool write_to_pipe(const void* data_to_insert, size_t size_of_data) {

        if (writeOffset + size_of_data > sharedMem.getSize()) {
            if (!sharedMem.isOwner()) {
                return false;
            }
            size_t newSize = std::max(sharedMem.getSize() * 2,
                                      writeOffset + size_of_data);
            sharedMem.resetSize(newSize);
        }

        char* curr_ptr = (char*)sharedMem.getPtr();
        memcpy(curr_ptr + writeOffset, data_to_insert, size_of_data);
        writeOffset += size_of_data;
        return true;
    }

    bool read_from_pipe(void* to_save, size_t size_of_data) {
        if (readOffset >= writeOffset) {
            return false;
        }

        size_t unread = writeOffset - readOffset;
        size_t currSize = sharedMem.getSize();
        if (sharedMem.isOwner() && currSize > 1024 && unread < currSize / 4) {
            char* curr_ptr = (char*)sharedMem.getPtr();
            memmove(curr_ptr, curr_ptr + readOffset, unread);
            writeOffset = unread;
            readOffset = 0;
            sharedMem.resetSize(unread == 0 ? 1024 : unread * 2);
        }

        if (size_of_data > writeOffset - readOffset) {
            return false;
        }

        char* curr_ptr = (char*)sharedMem.getPtr();
        memcpy(to_save, curr_ptr + readOffset, size_of_data);
        readOffset += size_of_data;
        return true;
    }
};
