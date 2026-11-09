/*
 * JAR 读取 / META-INF/MANIFEST.MF 解析 / 老式 ZIP data descriptor 规范化
 *
 * 前两项是原项目行为的等价实现：
 *   - 启动器从 Manifest 自动发现 MIDlet-1，兼容混淆后的类名（不硬编码）
 *   - JAR 只缓存一次到 RAM，避免慢速 NAND 随机读取
 * 第三项 normalize_game_jar 是 src/platform/bbk9588/phoneme_pcsl_file.c
 * 中同名函数的逐行移植：只在 RAM 缓存里补齐 local header、移除
 * 16-byte data descriptor、修正 central directory offset，
 * 不改动任何游戏 class / resource 字节（见 vendor/phoneme_pcsl_file.c.orig）。
 */
#include "ios_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <zlib.h>

static uint16_t rd16(const unsigned char *at) {
    return (uint16_t)((uint16_t)at[0] | ((uint16_t)at[1] << 8));
}
static uint32_t rd32(const unsigned char *at) {
    return (uint32_t)at[0] | ((uint32_t)at[1] << 8) |
           ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
}
static void wr16(unsigned char *at, uint16_t v) {
    at[0] = (unsigned char)(v & 0xffu);
    at[1] = (unsigned char)(v >> 8);
}
static void wr32(unsigned char *at, uint32_t v) {
    at[0] = (unsigned char)(v & 0xffu);
    at[1] = (unsigned char)((v >> 8) & 0xffu);
    at[2] = (unsigned char)((v >> 16) & 0xffu);
    at[3] = (unsigned char)((v >> 24) & 0xffu);
}

/* ---------- 老式 ZIP 规范化 ---------- */
int ios_jar_normalize(unsigned char **dataPtr, long *sizePtr) {
    unsigned char *source = *dataPtr;
    unsigned char *output;
    uint32_t *local_offsets;
    uint32_t size = (uint32_t)*sizePtr;
    uint32_t eocd, search_start, central_offset, central_size;
    uint32_t central_position, output_position, output_central, output_eocd;
    uint32_t tail_size, local_offset, previous_local_offset = 0u;
    uint32_t compressed_size, record_size, local_size;
    uint16_t entries, name_size, extra_size, comment_size, flags, index;
    int needs_normalization = 0;

    if (size < 22u)
        return 0;
    search_start = size > 65557u ? size - 65557u : 0u;
    eocd = size - 22u;
    for (;;) {
        if (source[eocd] == 'P' && source[eocd + 1u] == 'K' &&
            source[eocd + 2u] == 5u && source[eocd + 3u] == 6u)
            break;
        if (eocd == search_start)
            return 0;
        --eocd;
    }
    entries = rd16(source + eocd + 10u);
    central_size = rd32(source + eocd + 12u);
    central_offset = rd32(source + eocd + 16u);
    if (entries == 0u || central_offset > eocd ||
        central_size > eocd - central_offset)
        return 0;
    local_offsets = (uint32_t *)malloc((size_t)entries * sizeof(*local_offsets));
    output = (unsigned char *)malloc(size);
    if (local_offsets == NULL || output == NULL) {
        free(local_offsets);
        free(output);
        return 0;
    }
    central_position = central_offset;
    output_position = 0u;
    for (index = 0u; index < entries; ++index) {
        if (central_position + 46u > eocd ||
            memcmp(source + central_position, "PK\001\002", 4u) != 0)
            goto failed;
        flags = rd16(source + central_position + 8u);
        compressed_size = rd32(source + central_position + 20u);
        name_size = rd16(source + central_position + 28u);
        extra_size = rd16(source + central_position + 30u);
        comment_size = rd16(source + central_position + 32u);
        local_offset = rd32(source + central_position + 42u);
        record_size = 46u + name_size + extra_size + comment_size;
        if (record_size > eocd - central_position ||
            local_offset + 30u > central_offset ||
            memcmp(source + local_offset, "PK\003\004", 4u) != 0)
            goto failed;
        local_size = 30u + rd16(source + local_offset + 26u) +
                     rd16(source + local_offset + 28u) + compressed_size;
        if (local_size > central_offset - local_offset ||
            (index != 0u && local_offset < previous_local_offset))
            goto failed;
        previous_local_offset = local_offset;
        if (index == 0u && local_offset != 0u) {
            memcpy(output, source, local_offset);
            output_position = local_offset;
        }
        if (output_position + local_size > size)
            goto failed;
        memcpy(output + output_position, source + local_offset, local_size);
        local_offsets[index] = output_position;
        flags &= (uint16_t)~8u;
        wr16(output + output_position + 6u, flags);
        memcpy(output + output_position + 14u,
               source + central_position + 16u, 12u);
        output_position += local_size;
        central_position += record_size;
        if ((rd16(source + central_position - record_size + 8u) & 8u) != 0u)
            needs_normalization = 1;
    }
    if (!needs_normalization) {
        free(local_offsets);
        free(output);
        return 0;
    }
    output_central = output_position;
    central_position = central_offset;
    for (index = 0u; index < entries; ++index) {
        name_size = rd16(source + central_position + 28u);
        extra_size = rd16(source + central_position + 30u);
        comment_size = rd16(source + central_position + 32u);
        record_size = 46u + name_size + extra_size + comment_size;
        memcpy(output + output_position, source + central_position, record_size);
        flags = rd16(output + output_position + 8u) & (uint16_t)~8u;
        wr16(output + output_position + 8u, flags);
        wr32(output + output_position + 42u, local_offsets[index]);
        output_position += record_size;
        central_position += record_size;
    }
    output_eocd = output_position;
    tail_size = size - eocd;
    if (output_eocd + tail_size > size)
        goto failed;
    memcpy(output + output_eocd, source + eocd, tail_size);
    wr32(output + output_eocd + 12u, output_position - output_central);
    wr32(output + output_eocd + 16u, output_central);
    output_position += tail_size;
    free(local_offsets);
    free(source);            /* 注意：调用者的旧缓冲已被释放 */
    *dataPtr = output;
    *sizePtr = (long)output_position;
    return 1;

failed:
    free(local_offsets);
    free(output);
    return 0;
}

/* ---------- ZIP 内单个条目读取（只需要 MANIFEST.MF） ---------- */
static int ascii_ieq(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return 0;
    }
    return 1;
}

static char *read_zip_entry(const unsigned char *data, uint32_t size,
                            const char *wanted, long *outLen) {
    uint32_t eocd, search_start, central_offset, central_size, pos;
    uint16_t entries, i;
    if (size < 22u)
        return NULL;
    search_start = size > 65557u ? size - 65557u : 0u;
    eocd = size - 22u;
    for (;;) {
        if (data[eocd] == 'P' && data[eocd + 1u] == 'K' &&
            data[eocd + 2u] == 5u && data[eocd + 3u] == 6u)
            break;
        if (eocd == search_start)
            return NULL;
        --eocd;
    }
    entries = rd16(data + eocd + 10u);
    central_size = rd32(data + eocd + 12u);
    central_offset = rd32(data + eocd + 16u);
    if (central_offset > eocd || central_size > eocd - central_offset)
        return NULL;
    pos = central_offset;
    for (i = 0u; i < entries; ++i) {
        uint16_t method, flags, name_size, extra_size, comment_size;
        uint32_t compressed_size, uncompressed_size, local_offset;
        uint32_t record_size, data_offset;
        const unsigned char *local;
        const char *name;
        if (pos + 46u > eocd || memcmp(data + pos, "PK\001\002", 4u) != 0)
            return NULL;
        method = rd16(data + pos + 10u);
        flags = rd16(data + pos + 8u);
        compressed_size = rd32(data + pos + 20u);
        uncompressed_size = rd32(data + pos + 24u);
        name_size = rd16(data + pos + 28u);
        extra_size = rd16(data + pos + 30u);
        comment_size = rd16(data + pos + 32u);
        local_offset = rd32(data + pos + 42u);
        record_size = 46u + name_size + extra_size + comment_size;
        name = (const char *)(data + pos + 46u);
        if (name_size == strlen(wanted) &&
            ascii_ieq(name, wanted, name_size) &&
            local_offset + 30u < size &&
            memcmp(data + local_offset, "PK\003\004", 4u) == 0) {
            char *plain;
            local = data + local_offset;
            data_offset = local_offset + 30u +
                          rd16(local + 26u) + rd16(local + 28u);
            if (data_offset + compressed_size > size)
                return NULL;
            plain = (char *)malloc((size_t)uncompressed_size + 1);
            if (plain == NULL)
                return NULL;
            if (method == 0) {
                memcpy(plain, data + data_offset, compressed_size);
                plain[compressed_size] = '\0';
                *outLen = (long)compressed_size;
                return plain;
            }
            if (method == 8) {
                z_stream stream;
                int rc;
                memset(&stream, 0, sizeof(stream));
                stream.next_in = (Bytef *)(data + data_offset);
                stream.avail_in = compressed_size;
                stream.next_out = (Bytef *)plain;
                stream.avail_out = uncompressed_size;
                if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
                    free(plain);
                    return NULL;
                }
                rc = inflate(&stream, Z_FINISH);
                inflateEnd(&stream);
                if (rc != Z_STREAM_END) {
                    free(plain);
                    return NULL;
                }
                plain[uncompressed_size] = '\0';
                *outLen = (long)uncompressed_size;
                return plain;
            }
            free(plain);
            (void)flags;
            return NULL;
        }
        pos += record_size;
    }
    return NULL;
}

char *ios_jar_read_manifest(const unsigned char *data, long size, long *outLen) {
    if (outLen != NULL)
        *outLen = 0;
    if (data == NULL || size < 22)
        return NULL;
    return read_zip_entry(data, (uint32_t)size,
                          "META-INF/MANIFEST.MF", outLen);
}

/* ---------- Manifest: MIDlet-1 ---------- */
static char *trim(char *s) {
    char *end;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        ++s;
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\r' || end[-1] == '\n'))
        --end;
    *end = '\0';
    return s;
}

int ios_jar_parse_midlet1(const unsigned char *data, long size,
                          char *outClass, size_t classCap,
                          char *outName, size_t nameCap) {
    long manifestLen = 0;
    char *manifest = ios_jar_read_manifest(data, size, &manifestLen);
    char *joined, *cursor, *line, *jp;
    char *value = NULL;
    int found = 0;

    if (outClass != NULL && classCap > 0) outClass[0] = '\0';
    if (outName != NULL && nameCap > 0) outName[0] = '\0';
    if (manifest == NULL || manifestLen <= 0)
        return -1;

    /* Manifest 续行（以空格开头）需要拼回上一行 */
    joined = (char *)malloc((size_t)manifestLen + 2u);
    if (joined == NULL) { free(manifest); return -1; }
    cursor = manifest;
    jp = joined;
    while (*cursor != '\0') {
        char *eol = strpbrk(cursor, "\r\n");
        size_t chunk = eol ? (size_t)(eol - cursor) : strlen(cursor);
        if (chunk == 0 || (cursor[0] != ' ' && cursor[0] != '\t')) {
            if (jp != joined) *jp++ = '\n';
        }
        memcpy(jp, cursor, chunk);
        jp += chunk;
        if (eol == NULL) break;
        cursor = eol;
        while (*cursor == '\r' || *cursor == '\n') ++cursor;
    }
    *jp = '\0';
    free(manifest);

    line = strtok(joined, "\n");
    while (line != NULL) {
        char *key = line;
        char *colon = strchr(line, ':');
        if (colon != NULL) {
            *colon = '\0';
            key = trim(key);
            if (strcasecmp(key, "MIDlet-1") == 0) {
                value = trim(colon + 1);
                found = 1;
                break;
            }
        }
        line = strtok(NULL, "\n");
    }
    if (!found || value == NULL) {
        free(joined);
        return -1;
    }

    /* "显示名, /icon.png, 主类" —— 取最后一段为主类、第一段为显示名。
     * 混淆后的类名（如 "b"）同样直接取用，不做任何校验。 */
    {
        char *copy = (char *)malloc(strlen(value) + 1u);
        char *parts[8];
        int count = 0;
        char *p;
        if (copy == NULL) { free(joined); return -1; }
        strcpy(copy, value);
        p = copy;
        parts[count++] = p;
        while ((p = strchr(p, ',')) != NULL && count < 8) {
            *p++ = '\0';
            parts[count++] = p;
        }
        if (outName != NULL && nameCap > 0 && count > 0) {
            const char *n = trim(parts[0]);
            snprintf(outName, nameCap, "%s", n);
        }
        if (outClass != NULL && classCap > 0) {
            const char *c = trim(parts[count - 1]);
            snprintf(outClass, classCap, "%s", c);
        }
        free(copy);
    }
    free(joined);
    return 0;
}

/* ---------- 打开 / 关闭 ---------- */
int ios_jar_open(const char *pathUtf8, IOSJar *out, long maxBytes) {
    FILE *file;
    long size;
    unsigned char *data;
    long limit = maxBytes > 0 ? maxBytes : 32L * 1024L * 1024L;
    if (out == NULL || pathUtf8 == NULL)
        return -1;
    memset(out, 0, sizeof(*out));
    file = fopen(pathUtf8, "rb");
    if (file == NULL)
        return -1;
    if (fseek(file, 0L, SEEK_END) != 0) { fclose(file); return -1; }
    size = ftell(file);
    rewind(file);
    if (size <= 0 || size > limit) { fclose(file); return -1; }
    data = (unsigned char *)malloc((size_t)size);
    if (data == NULL) { fclose(file); return -1; }
    if (fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data); fclose(file); return -1;
    }
    fclose(file);
    out->data = data;
    out->size = size;
    out->normalized = ios_jar_normalize(&out->data, &out->size);
    if (ios_jar_parse_midlet1(out->data, out->size,
                              out->midlet1, sizeof(out->midlet1),
                              out->midletName, sizeof(out->midletName)) != 0) {
        snprintf(out->midlet1, sizeof(out->midlet1), "%s", "");
        snprintf(out->midletName, sizeof(out->midletName), "%s", "");
    }
    return 0;
}

void ios_jar_close(IOSJar *jar) {
    if (jar == NULL)
        return;
    free(jar->data);
    memset(jar, 0, sizeof(*jar));
}
