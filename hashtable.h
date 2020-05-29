// Copyright 2020 Maria Tîmbur & Maria Moșneag 313CAs
#include <stdlib.h>
#include <stdio.h>

typedef struct node {
    void *data;
    void *key;
    struct node *next;
} Node;

typedef struct {
    int hmax;
    int (*hash_func)(void*);
    int (*comp_func)(void*, void*);
    Node **table;
} Hashtable;

// funcția creează un hashtable
Hashtable* createHashTable(int (*hash_func)(void*),
                            int (*comp_func)(void*, void*), int hmax) {
    Hashtable* hash = (Hashtable*) calloc(1, sizeof(Hashtable));
    if (hash == NULL) return NULL;
    hash->hash_func = hash_func;
    hash->comp_func = comp_func;
    hash->hmax = hmax;
    hash->table = (Node**) calloc(hmax, sizeof(Node*));
    if (hash->table == NULL) return NULL;
    return hash;
}

// funcția adaugă un element în hashtable
void addEntry(Hashtable* hash, void *key, void* data) {
    Node* node = (Node*) calloc(1, sizeof(Node));
    node->data = data;
    node->key = key;

    int ind = hash->hash_func(key) % hash->hmax;
    node->next = hash->table[ind];
    hash->table[ind] = node;
}

// funcția extrage informația dintr-un element
void* getData(Hashtable* hash, void *key) {
    int ind = hash->hash_func(key) % hash->hmax;
    for (Node* it = hash->table[ind]; it != NULL; it = it->next) {
        if (hash->comp_func(key, it->key)) {
            return it->data;
        }
    }
    return NULL;
}
// functia adauga un ammount dorit la elementele hashtable ului
// Presupune ca valorile sunt int-uri
void add(Hashtable* hash, void *key, int ammount) {
    int *cnt = getData(hash, key);
    if (!cnt) {
        cnt = calloc(1, sizeof(int));
        addEntry(hash, key, cnt);
    }
    (*cnt) += ammount;
}

// functia returneaza informatia dintr-un hashtable
int get(Hashtable* hash, void *key) {
    int *cnt = getData(hash, key);
    if (!cnt) {
        cnt = calloc(1, sizeof(int));
        addEntry(hash, key, cnt);
    }
    return *cnt;
}

// functia incrementeaza cu 1 valorile
void inc(Hashtable* hash, void *key) {
    add(hash, key, 1);
}

// funcția eliberează memoria alocată hashtable-ului
void freeTable(Hashtable* hash, int withValues, int withKeys) {
    for (int i = 0; i < hash->hmax; i++) {
        for (Node* it = hash->table[i]; it;) {
            Node* tmp = it;
            it = it->next;
            if (withValues) {
                free(tmp->data);
            }
            if (withKeys) {
                free(tmp->key);
            }
            free(tmp);
        }
    }
    free(hash->table);
    free(hash);
}

// funcția interschimbă două elemente
void swap(int* a, int* b) {
    int t = *a;
    *a = *b;
    *b = t;
}
