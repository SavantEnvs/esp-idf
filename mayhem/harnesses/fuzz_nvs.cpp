// Fuzz NVS: the input is the raw content of a flash partition. NVS parses and repairs it on init,
// then the harness walks every entry and reads each value back through the Storage API.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "nvs_storage.hpp"
#include "ram_partition.hpp"

namespace {

constexpr size_t kSector = 4096;
constexpr size_t kMaxSectors = 6;

void read_all(nvs::Storage &storage)
{
    nvs_opaque_iterator_t it{};
    it.storage = &storage;
    it.type = NVS_TYPE_ANY;
    if (!storage.findEntry(&it, nullptr)) {
        return;
    }
    size_t guard = 0;
    do {
        nvs_entry_info_t &info = it.entry_info;
        info.key[sizeof(info.key) - 1] = '\0';
        uint8_t ns = it.nsIndex;
        size_t sz = 0;
        if (info.type == NVS_TYPE_STR || info.type == NVS_TYPE_BLOB) {
            nvs::ItemType t = info.type == NVS_TYPE_STR ? nvs::ItemType::SZ : nvs::ItemType::BLOB;
            if (storage.getItemDataSize(ns, t, info.key, sz, false) == ESP_OK && sz > 0 && sz <= (1u << 16)) {
                std::vector<uint8_t> buf(sz);
                storage.readItem(ns, t, info.key, buf.data(), sz, false);
            }
        } else {
            uint64_t v = 0;
            storage.readItem(ns, static_cast<nvs::ItemType>(info.type), info.key, &v, 8, false);
        }
    } while (++guard < 4096 && storage.nextEntry(&it));
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) {
        return -1;
    }
    size_t sectors = 3 + data[0] % (kMaxSectors - 2);
    data++;
    size--;

    RamPartition part(sectors * kSector);
    memcpy(part.data(), data, size < sectors * kSector ? size : sectors * kSector);

    nvs::Storage *storage = new (std::nothrow) nvs::Storage(&part);
    if (!storage) {
        return -1;
    }
    if (storage->init(0, sectors, true) != ESP_OK) {
        delete storage;
        return -1;
    }

    read_all(*storage);

    nvs_stats_t stats;
    storage->fillStats(stats);

    uint8_t ns;
    if (storage->createOrOpenNamespace("fz", true, ns) == ESP_OK) {
        storage->writeItem(ns, nvs::ItemType::U32, "k", &stats.used_entries, 4, false);
        const char blob[300] = {1, 2, 3};
        storage->writeItem(ns, nvs::ItemType::BLOB, "b", blob, sizeof(blob), false);
        storage->eraseItem(ns, nvs::ItemType::ANY, "k", false);
        storage->eraseNamespace(ns, false);
    }
    read_all(*storage);
    delete storage;
    return 0;
}
