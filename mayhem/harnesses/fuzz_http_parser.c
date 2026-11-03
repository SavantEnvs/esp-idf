// Fuzz the http_parser component (request/response/URL parsing) used by esp_http_server and esp_http_client.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "http_parser.h"

struct state {
    size_t bytes;
    unsigned calls;
};

static int touch(http_parser *p, const char *at, size_t len)
{
    struct state *s = p->data;
    s->calls++;
    for (size_t i = 0; i < len; i++) {
        s->bytes += (unsigned char)at[i];
    }
    return 0;
}

static int note(http_parser *p)
{
    struct state *s = p->data;
    s->calls++;
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2) {
        return -1;
    }
    const uint8_t mode = data[0];
    const size_t chunk = data[1];
    data += 2;
    size -= 2;

    http_parser_settings settings;
    http_parser_settings_init(&settings);
    settings.on_message_begin = note;
    settings.on_url = touch;
    settings.on_status = touch;
    settings.on_header_field = touch;
    settings.on_header_value = touch;
    settings.on_headers_complete = note;
    settings.on_body = touch;
    settings.on_message_complete = note;
    settings.on_chunk_header = note;
    settings.on_chunk_complete = note;

    if (mode & 0x80) {
        struct http_parser_url url;
        http_parser_url_init(&url);
        if (http_parser_parse_url((const char *)data, size, (mode >> 6) & 1, &url) == 0) {
            size_t sum = 0;
            for (int f = 0; f < UF_MAX; f++) {
                if (url.field_set & (1 << f)) {
                    const char *p = (const char *)data + url.field_data[f].off;
                    for (size_t i = 0; i < url.field_data[f].len; i++) {
                        sum += (unsigned char)p[i];
                    }
                }
            }
            (void)sum;
        }
        return 0;
    }

    enum http_parser_type type = (mode & 3) == 0 ? HTTP_REQUEST : (mode & 3) == 1 ? HTTP_RESPONSE : HTTP_BOTH;
    struct state st = {0, 0};
    http_parser parser;
    http_parser_init(&parser, type);
    parser.data = &st;

    const size_t step = chunk ? chunk : (size ? size : 1);
    size_t off = 0;
    while (off < size) {
        size_t n = size - off < step ? size - off : step;
        size_t used = http_parser_execute(&parser, &settings, (const char *)data + off, n);
        if (used != n || HTTP_PARSER_ERRNO(&parser) != HPE_OK) {
            break;
        }
        off += n;
    }
    http_parser_execute(&parser, &settings, NULL, 0);
    (void)http_should_keep_alive(&parser);
    (void)http_method_str((enum http_method)parser.method);
    (void)http_errno_name(HTTP_PARSER_ERRNO(&parser));
    (void)http_errno_description(HTTP_PARSER_ERRNO(&parser));
    (void)http_body_is_final(&parser);
    return 0;
}
