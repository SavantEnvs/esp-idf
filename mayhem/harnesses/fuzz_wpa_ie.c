// Fuzz the 802.11 information-element parsers and the WPS TLV parser of wpa_supplicant:
// the code that handles attacker-controlled beacon/probe-response/association frames.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "utils/common.h"
#include "utils/wpabuf.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "wps/wps_attr_parse.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) {
        return -1;
    }
    const uint8_t op = data[0];
    data++;
    size--;

    switch (op % 6) {
    case 0: {
        struct ieee802_11_elems elems;
        ParseRes r = ieee802_11_parse_elems(data, size, &elems, 0);
        if (r == ParseFailed) {
            return -1;
        }
        (void)ieee802_11_ie_count(data, size);
        if (elems.rsnxe) {
            (void)ieee802_11_rsnx_capab_len(elems.rsnxe, elems.rsnxe_len, op & 7);
        }
        break;
    }
    case 1: {
        if (size < 1) {
            return -1;
        }
        const uint8_t *ie = get_ie(data + 1, size - 1, data[0]);
        if (ie) {
            (void)ie[1];
        }
        break;
    }
    case 2: {
        if (size < 4) {
            return -1;
        }
        u32 oui_type = WPA_GET_BE32(data);
        const uint8_t *ie = get_vendor_ie(data + 4, size - 4, oui_type);
        if (ie) {
            (void)ie[1];
        }
        struct wpabuf *buf = ieee802_11_vendor_ie_concat(data + 4, size - 4, oui_type);
        wpabuf_free(buf);
        break;
    }
    case 3: {
        struct wpabuf *buf = ieee802_11_defrag(data, size, op & 0x80);
        wpabuf_free(buf);
        break;
    }
    case 4: {
        struct wpabuf *msg = wpabuf_alloc_copy(data, size);
        if (!msg) {
            return -1;
        }
        struct wps_parse_attr attr;
        int r = wps_parse_msg(msg, &attr);
        wpabuf_free(msg);
        if (r < 0) {
            return -1;
        }
        break;
    }
    default: {
        char *str = malloc(size + 1);
        if (!str) {
            return -1;
        }
        memcpy(str, data, size);
        str[size] = '\0';
        u8 nei[256];
        (void)ieee802_11_parse_candidate_list(str, nei, sizeof(nei));
        free(str);
        break;
    }
    }
    return 0;
}
