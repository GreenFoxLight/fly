#include "fly.h"
#include "parser.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#define TYPE_LEAF 0
#define TYPE_TAG 1
#define TYPE_PAIR 2
#define TYPE_LIST 4

int init_syntree(syntree_t* tree) {
    tree->entries = NULL;
    tree->num_entries = 0;
    return 1;
}

void release_syntree(syntree_t* tree) {
    for (size_t i = 0; i < tree->num_entries; i++) {
        if (tree->entries[i].type == TYPE_LIST) {
            free(tree->entries[i].value.list.list);
        }
    }
    free(tree->entries);
    tree->num_entries = 0;
}

synentry_t* syntree_get_entry(syntree_t* tree, ast_id id) {
    assert(id <= tree->num_entries);
    assert(id != 0);
    return &tree->entries[id - 1];
}

void syntree_traverse(syntree_t* tree, ast_id root, syntree_traverse_fnc fnc) {
    synentry_t* current = syntree_get_entry(tree, root);
    if (!current)
        return;
    int visit_children = fnc(current);
    if (!visit_children)
        return;
    if (current->type == TYPE_TAG) {
        syntree_traverse(tree, current->value.tag, fnc);
    } else if (current->type == TYPE_PAIR) {
        syntree_traverse(tree, current->value.pair.first, fnc);
        syntree_traverse(tree, current->value.pair.second, fnc);
    } else if (current->type == TYPE_LIST) {
        for (size_t i = 0; i < current->value.list.length; i++) {
            syntree_traverse(tree, current->value.list.list[i], fnc);
        }
    }
}

internal ast_id add_entry(syntree_t* tree, synentry_t entry) {
    tree->num_entries += 1;
    tree->entries = realloc(tree->entries, tree->num_entries * sizeof(entry));
    if (!tree->entries) {
        fprintf(stderr, "Out of memory!\n");
        exit(255);
    }
    tree->entries[tree->num_entries - 1] = entry;
    return (ast_id)tree->num_entries;
}

ast_id syntree_add_int(syntree_t* tree, i32 i) {
    synentry_t node;
    node.tag = AST_CONST_INT;
    node.type = TYPE_LEAF;
    node.value.integer = i;
    return add_entry(tree, node);
}

ast_id syntree_add_uint(syntree_t* tree, u32 u) {
    synentry_t node;
    node.tag = AST_CONST_UINT;
    node.type = TYPE_LEAF;
    node.value.unsigned_int = u;
    return add_entry(tree, node);
}

ast_id syntree_add_long(syntree_t* tree, i64 l) {
    synentry_t node;
    node.tag = AST_CONST_INTL;
    node.type = TYPE_LEAF;
    node.value.long_int = l;
    return add_entry(tree, node);
}

ast_id syntree_add_ulong(syntree_t* tree, u64 u) {
    synentry_t node;
    node.tag = AST_CONST_UINTL;
    node.type = TYPE_LEAF;
    node.value.unsigned_long = u;
    return add_entry(tree, node);
}

ast_id syntree_add_f32(syntree_t* tree, f32 f) {
    synentry_t node;
    node.tag = AST_CONST_FLOAT32;
    node.type = TYPE_LEAF;
    node.value.float32 = f;
    return add_entry(tree, node);
}

ast_id syntree_add_f64(syntree_t* tree, f64 f) {
    synentry_t node;
    node.tag = AST_CONST_FLOAT64;
    node.type = TYPE_LEAF;
    node.value.float64 = f;
    return add_entry(tree, node);
}

ast_id syntree_add_bool(syntree_t* tree, bool b) {
    synentry_t node;
    node.tag = AST_CONST_BOOL;
    node.type = TYPE_LEAF;
    node.value.boolean = b;
    return add_entry(tree, node);
}

ast_id syntree_add_char(syntree_t* tree, char c) {
    synentry_t node;
    node.tag = AST_CONST_CHAR;
    node.type = TYPE_LEAF;
    node.value.character = c;
    return add_entry(tree, node);
}

ast_id syntree_add_string(syntree_t* tree, const char* s) {
    synentry_t node;
    node.tag = AST_CONST_STRING;
    node.type = TYPE_LEAF;
    node.value.string = malloc(strlen(s) + 1);
    if (!node.value.string) {
        fprintf(stderr, "Out of memory!\n");
        exit(255);
    }
    return add_entry(tree, node);
}

ast_id syntree_add_id(syntree_t* tree, const char* id) {
    synentry_t node;
    node.tag = AST_ID;
    node.type = TYPE_LEAF;
    node.value.string = malloc(strlen(id) + 1);
    if (!node.value.string) {
        fprintf(stderr, "Out of memory!\n");
        exit(255);
    }
    strcpy(node.value.string, id);
    return add_entry(tree, node);
}

ast_id syntree_add_operator(syntree_t* tree, token_tag_t op) {
    synentry_t node;
    node.tag = AST_OPERATOR;
    node.type = TYPE_LEAF;
    node.value.operator = op;
    return add_entry(tree, node);
}

ast_id syntree_add_tag(syntree_t* tree, synentry_tag_t tag, ast_id contained) {
    synentry_t node;
    node.tag = tag;
    node.type = TYPE_TAG;
    node.value.tag = contained;
    return add_entry(tree, node);
}

ast_id syntree_add_pair(syntree_t* tree, synentry_tag_t tag,
        ast_id first, ast_id second) {
    synentry_t node;
    node.tag = tag;
    node.type = TYPE_TAG;
    node.value.pair.first = first;
    node.value.pair.second = second;
    return add_entry(tree, node);
}

ast_id syntree_add_list(syntree_t* tree, synentry_tag_t tag,
        size_t length, ...) {
    
    synentry_t node;
    node.tag = tag;
    node.type = TYPE_LIST;
    node.value.list.length = length;
    node.value.list.list = malloc(sizeof(ast_id) * length);
    if (!node.value.list.list) {
        fprintf(stderr, "Out of memory!\n");
        exit(255);
    }
    va_list args;
    va_start(args, length);
    for (size_t i = 0; i < length; i++) {
        ast_id child = va_arg(args, ast_id);
        node.value.list.list[i] = child;
    }
    va_end(args);
    return add_entry(tree, node);
}
