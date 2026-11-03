// Fuzz the wpa_supplicant JSON parser (used by DPP) and the base64url member decoder.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "utils/common.h"
#include "utils/json.h"
#include "utils/wpabuf.h"

static void walk(struct json_token *t, int depth)
{
    for (; t; t = t->sibling) {
        if (t->name) {
            (void)strlen(t->name);
            (void)json_get_member(t->parent, t->name);
        }
        if (t->string) {
            (void)strlen(t->string);
        }
        if (depth < 64 && t->child) {
            walk(t->child, depth + 1);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    struct json_token *root = json_parse((const char *)data, size);
    if (!root) {
        return -1;
    }
    walk(root, 0);
    char buf[512];
    json_print_tree(root, buf, sizeof(buf));
    static const char *const names[] = {"kty", "crv", "x", "y", "a", "b"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        struct wpabuf *b = json_get_member_base64url(root, names[i]);
        wpabuf_free(b);
    }
    json_free(root);
    return 0;
}
