// In-memory nvs::Partition with flash semantics (programming only clears bits).
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

#include "partition.hpp"

class RamPartition : public nvs::Partition {
public:
    explicit RamPartition(size_t size) : mMem(size, 0xFF) {}

    uint8_t *data() { return mMem.data(); }

    const char *get_partition_name() override { return "nvs"; }

    esp_err_t read_raw(size_t off, void *dst, size_t size) override { return read(off, dst, size); }

    esp_err_t read(size_t off, void *dst, size_t size) override
    {
        if (off > mMem.size() || size > mMem.size() - off) {
            return ESP_ERR_INVALID_SIZE;
        }
        memcpy(dst, mMem.data() + off, size);
        return ESP_OK;
    }

    esp_err_t write_raw(size_t off, const void *src, size_t size) override { return write(off, src, size); }

    esp_err_t write(size_t off, const void *src, size_t size) override
    {
        if (off > mMem.size() || size > mMem.size() - off) {
            return ESP_ERR_INVALID_SIZE;
        }
        // Flash semantics: programming can only clear bits.
        const uint8_t *s = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < size; i++) {
            mMem[off + i] &= s[i];
        }
        return ESP_OK;
    }

    esp_err_t erase_range(size_t off, size_t size) override
    {
        if (off > mMem.size() || size > mMem.size() - off) {
            return ESP_ERR_INVALID_SIZE;
        }
        memset(mMem.data() + off, 0xFF, size);
        return ESP_OK;
    }

    uint32_t get_address() override { return 0; }
    uint32_t get_size() override { return static_cast<uint32_t>(mMem.size()); }
    bool get_readonly() override { return false; }

private:
    std::vector<uint8_t> mMem;
};
