// NVS persistence known-answer check: write typed values, drop the Storage, re-open the same flash
// image and read them back.
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

#include "nvs_storage.hpp"
#include "ram_partition.hpp"

extern "C" int nvs_selftest(void)
{
    const size_t sectors = 4;
    RamPartition part(sectors * 4096);
    uint8_t ns = 0;
    {
        nvs::Storage s(&part);
        esp_err_t e = s.init(0, sectors, true);
        printf("nvs-init: err=%d valid=%d\n", (int)e, (int)s.isValid());
        e = s.createOrOpenNamespace("cfg", true, ns);
        uint32_t u = 0xC0FFEE;
        int16_t i16 = -1234;
        e = s.writeItem(ns, nvs::ItemType::U32, "u32", &u, sizeof(u), false);
        e = s.writeItem(ns, nvs::ItemType::I16, "i16", &i16, sizeof(i16), false);
        e = s.writeItem(ns, nvs::ItemType::SZ, "str", "hello nvs", 10, false);
        uint8_t blob[200];
        for (size_t i = 0; i < sizeof(blob); i++) blob[i] = static_cast<uint8_t>(i * 7);
        e = s.writeItem(ns, nvs::ItemType::BLOB, "blob", blob, sizeof(blob), false);
        printf("nvs-write: err=%d ns=%u\n", (int)e, ns);
    }
    nvs::Storage s(&part);
    esp_err_t e = s.init(0, sectors, true);
    uint8_t ns2 = 0;
    esp_err_t en = s.createOrOpenNamespace("cfg", false, ns2);
    uint32_t u = 0;
    int16_t i16 = 0;
    char str[16] = {0};
    uint8_t blob[200] = {0};
    esp_err_t e1 = s.readItem(ns2, nvs::ItemType::U32, "u32", &u, sizeof(u));
    esp_err_t e2 = s.readItem(ns2, nvs::ItemType::I16, "i16", &i16, sizeof(i16));
    esp_err_t e3 = s.readItem(ns2, nvs::ItemType::SZ, "str", str, 10);
    esp_err_t e4 = s.readItem(ns2, nvs::ItemType::BLOB, "blob", blob, sizeof(blob));
    unsigned sum = 0;
    for (size_t i = 0; i < sizeof(blob); i++) sum += blob[i];
    printf("nvs-reopen: init=%d ns=%d same_ns=%d u32=0x%x i16=%d str=%s blobsum=%u errs=%d%d%d%d\n", (int)e,
           (int)en, ns2 == ns, u, i16, str, sum, (int)e1, (int)e2, (int)e3, (int)e4);
    esp_err_t em = s.readItem(ns2, nvs::ItemType::U32, "missing", &u, sizeof(u));
    printf("nvs-missing: err=0x%x\n", (unsigned)em);
    nvs_stats_t st;
    s.fillStats(st);
    printf("nvs-stats: used=%u namespaces=%u\n", (unsigned)st.used_entries, (unsigned)st.namespace_count);
    return 0;
}
