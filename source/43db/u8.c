#define _XOPEN_SOURCE 600L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "u8.h"

#define ERROR(str, ...) fprintf(stderr, "%s: \n" str "\n", __PRETTY_FUNCTION__, ##__VA_ARGS__)

int U8Init(void* ptr, size_t fsize, U8Context* ctx) {
    assert(ptr != NULL && ctx != NULL);

    memset(ctx, 0, sizeof(U8Context));

    U8Header* header = (U8Header*)ptr;

    if (header->magic != U8_MAGIC) {
        return -1;
    }
    
    U8Node* nodes = (U8Node*)(ptr + header->root_node_offset);
    U8Node* root_node = &nodes[0];
    unsigned int node_count = root_node->size;

    const char* str_table = (const char*)(&nodes[node_count]);
    const char* str_table_end = ptr + header->root_node_offset + header->meta_size;
    unsigned int str_table_size = (unsigned int)(str_table_end - str_table);

    if (root_node->type != 0x01) {
        ERROR("Root node is not a directory? What?");
        return -2;
    }

    for (U8Node* node = &nodes[1]; node < nodes + node_count; node++) {
        unsigned int node_index = (node - nodes);

        if (node->type != 0x00 && node->type != 0x01) {
            ERROR("Node #%i has invalid node type (%#x)", node_index, node->type);
            return -3;
        }

        const char* node_name = str_table + node->name_offset;
        if (node_name >= str_table_end) {
            ERROR("Name for node #%i is out of range (name_offset=%#x)", node_index, node->name_offset);
            return -4;
        }

        // OSReport("Node #%u, name: +%#x '%s', type=%#x, ofs=%u", node_index, node->name_offset, node_name, node->type, node->offset);

        if (node->type == 0x00) {
            if (node->offset < header->data_offset || (fsize && node->offset > fsize)) {
                ERROR("Data offset for file node #%i (%s) is out of bounds (=%#x)", node_index, node_name, node->offset);
                return -5;
            }       
        } else {
            if (node->offset > node_count) {
                ERROR("End marker for directory node #%i (%s) is out of bounds (%#x>%#x)", node_index, node_name, node->offset, node_count);
                return -5;
            }
        }

    }

    ctx->header = *header;
    ctx->nodes = nodes;
    ctx->node_count = node_count;
    ctx->str_table = str_table;
    ctx->str_table_size = str_table_size;

    ctx->ptr = ptr;
    ctx->fsize = fsize;
    return 0;
}

static U8Node* U8FindChild(U8Context* ctx, U8Node* parent, const char* name) {
    if (parent->type != 0x01) {
        ERROR("Tried to find child in a non-directory node");
        return NULL;
    }

    U8Node* dir_ptr = parent + 1;
    U8Node* dir_end = &ctx->nodes[parent->size];

    while (dir_ptr < dir_end) {
        const char* node_name = ctx->str_table + dir_ptr->name_offset;

        if (!strcmp(name, node_name))
            return dir_ptr;

        // Next child node
        if (dir_ptr->type == 0x01)
            dir_ptr = &ctx->nodes[dir_ptr->size];
        else
            dir_ptr++;
    }

    return NULL;
}

int U8OpenFile(U8Context* ctx, const char* filepath, U8File* out) {
    assert(ctx != NULL && filepath != NULL); // out? Eh not so much

    char* _filepath = strdup(filepath);
    if (!_filepath) //??
        return -1;

    char* saveptr;
    char* elem = strtok_r(_filepath, "/", &saveptr);
    U8Node* node = ctx->root_node;

    if (!elem) {
        ERROR("strtok() came back with nothing on the first run?\n" "'%s' <-- Does this path have a slash?", filepath);
        free(_filepath);
        return -2;
    }

    while (elem) {
        node = U8FindChild(ctx, node, elem);
        if (!node)
            break;

        elem = strtok_r(NULL, "/", &saveptr);
    }

    free(_filepath);
    if (!node)
        return -2;

    if (node->type != 0x00)
        return -3;

    if (out) {
        out->ctx = ctx;
        out->node_index = node - ctx->nodes;
        out->data_ptr = ctx->ptr + node->offset;
        out->fsize = node->size;
    }

    return 0;
}