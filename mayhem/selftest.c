// Known-answer checks over the code the harnesses fuzz. Prints one line per check; mayhem/test.sh
// diffs the output against mayhem/selftest.golden.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_parser.h"
#include "utils/common.h"
#include "utils/json.h"
#include "utils/wpabuf.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "wps/wps_attr_parse.h"

int nvs_selftest(void);

struct http_state {
    char url[64];
    char body[64];
    int headers;
    int complete;
};

static int on_url(http_parser *p, const char *at, size_t len)
{
    struct http_state *s = p->data;
    strncat(s->url, at, len < 63 - strlen(s->url) ? len : 63 - strlen(s->url));
    return 0;
}

static int on_header_field(http_parser *p, const char *at, size_t len)
{
    (void)at; (void)len;
    ((struct http_state *)p->data)->headers++;
    return 0;
}

static int on_body(http_parser *p, const char *at, size_t len)
{
    struct http_state *s = p->data;
    strncat(s->body, at, len < 63 - strlen(s->body) ? len : 63 - strlen(s->body));
    return 0;
}

static int on_complete(http_parser *p)
{
    ((struct http_state *)p->data)->complete = 1;
    return 0;
}

static void test_http(void)
{
    static const char req[] =
        "POST /api/v1/nvs?key=a HTTP/1.1\r\nHost: esp.local\r\nContent-Length: 5\r\nX-A: b\r\n\r\nhello";
    http_parser_settings st;
    http_parser_settings_init(&st);
    st.on_url = on_url;
    st.on_header_field = on_header_field;
    st.on_body = on_body;
    st.on_message_complete = on_complete;
    struct http_state s;
    memset(&s, 0, sizeof(s));
    http_parser p;
    http_parser_init(&p, HTTP_REQUEST);
    p.data = &s;
    size_t n = http_parser_execute(&p, &st, req, sizeof(req) - 1);
    printf("http: consumed=%zu method=%s url=%s headers=%d body=%s complete=%d major=%u minor=%u errno=%s\n",
           n, http_method_str(p.method), s.url, s.headers, s.body, s.complete, p.http_major, p.http_minor,
           http_errno_name(HTTP_PARSER_ERRNO(&p)));

    static const char bad[] = "GET / HTTX/1.1\r\n\r\n";
    http_parser_init(&p, HTTP_REQUEST);
    p.data = &s;
    http_parser_execute(&p, &st, bad, sizeof(bad) - 1);
    printf("http-bad: errno=%s\n", http_errno_name(HTTP_PARSER_ERRNO(&p)));

    static const char url[] = "http://user@example.com:8080/p/a?q=1#frag";
    struct http_parser_url u;
    http_parser_url_init(&u);
    int r = http_parser_parse_url(url, sizeof(url) - 1, 0, &u);
    printf("http-url: rc=%d port=%u host=%.*s path=%.*s query=%.*s\n", r, u.port,
           u.field_data[UF_HOST].len, url + u.field_data[UF_HOST].off,
           u.field_data[UF_PATH].len, url + u.field_data[UF_PATH].off,
           u.field_data[UF_QUERY].len, url + u.field_data[UF_QUERY].off);
}

static void test_json(void)
{
    static const char doc[] = "{\"kty\":\"EC\",\"crv\":\"P-256\",\"x\":\"aGVsbG8\",\"n\":[1,2,{\"z\":true}]}";
    struct json_token *root = json_parse(doc, sizeof(doc) - 1);
    if (!root) {
        printf("json: parse failed\n");
        return;
    }
    struct json_token *kty = json_get_member(root, "kty");
    struct json_token *crv = json_get_member(root, "crv");
    struct wpabuf *x = json_get_member_base64url(root, "x");
    printf("json: kty=%s crv=%s x=%.*s missing=%d\n", kty && kty->string ? kty->string : "?",
           crv && crv->string ? crv->string : "?", x ? (int)wpabuf_len(x) : 0,
           x ? (const char *)wpabuf_head(x) : "", json_get_member(root, "nope") == NULL);
    char buf[256];
    json_print_tree(root, buf, sizeof(buf));
    printf("json-tree-bytes=%zu\n", strlen(buf));
    wpabuf_free(x);
    json_free(root);
    printf("json-bad: %s\n", json_parse("{\"a\":", 5) ? "accepted" : "rejected");
}

static void test_ie(void)
{
    static const u8 ies[] = {
        0x00, 0x04, 'e', 's', 'p', '!',                      /* SSID */
        0x03, 0x01, 0x06,                                     /* DS params */
        0x30, 0x04, 0x01, 0x00, 0x00, 0x0f,                   /* RSN (truncated body) */
        0xdd, 0x07, 0x00, 0x50, 0xf2, 0x04, 0xaa, 0xbb, 0xcc, /* WPS vendor IE */
    };
    struct ieee802_11_elems elems;
    ParseRes r = ieee802_11_parse_elems(ies, sizeof(ies), &elems, 0);
    printf("ie: parse=%d count=%d rsn_len=%u\n", (int)r, ieee802_11_ie_count(ies, sizeof(ies)), elems.rsn_ie_len);
    const u8 *ssid = get_ie(ies, sizeof(ies), 0);
    printf("ie-get: ssid_len=%u ssid=%.*s ds=%u missing=%d\n", ssid ? ssid[1] : 0, ssid ? ssid[1] : 0,
           ssid ? (const char *)ssid + 2 : "", get_ie(ies, sizeof(ies), 3)[2], get_ie(ies, sizeof(ies), 0x7f) == NULL);
    const u8 *v = get_vendor_ie(ies, sizeof(ies), 0x0050f204);
    printf("ie-vendor: found=%d len=%u\n", v != NULL, v ? v[1] : 0);
    struct wpabuf *vb = ieee802_11_vendor_ie_concat(ies, sizeof(ies), 0x0050f204);
    printf("ie-concat: len=%zu\n", vb ? wpabuf_len(vb) : 0);
    wpabuf_free(vb);
    printf("ie-truncated: parse=%d\n", (int)ieee802_11_parse_elems(ies, 5, &elems, 0));
}

static void test_wps(void)
{
    static const u8 msg[] = {
        0x10, 0x4a, 0x00, 0x01, 0x10,                         /* Version 0x10 */
        0x10, 0x22, 0x00, 0x01, 0x04,                         /* Message type M4 */
        0x10, 0x47, 0x00, 0x10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, /* UUID-E */
    };
    struct wpabuf *b = wpabuf_alloc_copy(msg, sizeof(msg));
    struct wps_parse_attr attr;
    int r = wps_parse_msg(b, &attr);
    printf("wps: rc=%d version=0x%02x msg_type=%u uuid_e0=%u\n", r, attr.version ? *attr.version : 0,
           attr.msg_type ? *attr.msg_type : 0, attr.uuid_e ? attr.uuid_e[0] : 0);
    wpabuf_free(b);
    static const u8 trunc[] = {0x10, 0x4a, 0x00, 0x09, 0x10};
    b = wpabuf_alloc_copy(trunc, sizeof(trunc));
    printf("wps-truncated: rc=%d\n", wps_parse_msg(b, &attr));
    wpabuf_free(b);
}

int main(void)
{
    test_http();
    test_json();
    test_ie();
    test_wps();
    return nvs_selftest();
}
