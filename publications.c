// Copyright 2020 Maria Tîmbur & Maria Moșneag 313CAs

#include <stdio.h>
#include <stdlib.h>
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdint.h>
#include "publications.h"
#include "hashtable.h"
#define HMAX 339421
#define NMAX 80
#define M 250
#define N 18000

typedef struct {
    char *title;
    int64_t id;
    int year;
    char *venue;
    int num_fields;
    char **fields;
    int num_authors;
    int64_t *author_ids;
    int num_refs;
    int64_t *references;
} paper_t;

typedef struct {
    int64_t id;
    char *name;
    char *institution;
    int num_papers;
    int64_t *papers_id;
    int num_neighbours;
    int64_t *neighbours;
} author_t;

struct publications_data {
    // hashtable cu toate paper-urile; hash dupa id paper
    Hashtable *papers_ht;
    // hashtable cu numarul de citari; hash dupa id paper
    Hashtable *citation_ht;
    // dupa a tuturor articoleleor din cadrul a unui venue
    Hashtable *venue_total_ht;
    // hashtable cu numarul de articole din cadrul a unui venue
    Hashtable *venue_cnt_ht;
    // hashtable cu cei vizitati
    Hashtable *visited_ht;
    // hashtable cu toti autorii; hash dupa id autor
    Hashtable *author_ht;
    // hashtable cu autorii; hash dupa institutie+field
    Hashtable *inst_and_field_ht;
    // hashtable cu toate domeniile; hash dupa numele domeniilor
    Hashtable *author_papers_ht;
    // vector in care year_v[x] = nr de articole din anul x
    int *year_cnt;
};

extern char* strdup(const char*);

// funcție de hashing
int hash_function_int(void *key) {
  int64_t h = *((int64_t *)key);
  return h % HMAX;
}

// funcție de comparare
int comp_fun_int(void *a, void *b) {
    return (*(int *)a == *(int *)b);
}

// funcție de hashing pentru stringuri
int hash_function_string(void *a) {
    /*
     * Credits: http://www.cse.yorku.ca/~oz/hash.html
     */
    unsigned char *puchar_a = (unsigned char*) a;
    int hash = 5381;
    int c;
    while ((c = *puchar_a++)) {
        hash = ((hash << 5u) + hash) + c; /* hash * 33 + c */
    }
    if (hash < 0) hash *=-1;
    return hash;
}

// funcție de comparare pentru stringuri
int comp_func_string(void* a, void* b) {
    if (strcmp((char*)a, (char*)b) == 0) return 1;
    return 0;
}

// funcția alocă memorie și inițializează PublData
PublData* init_publ_data(void) {
    PublData *new = calloc(1, sizeof(PublData));
    DIE(new == NULL, "PublData malloc");

    new->papers_ht = createHashTable(hash_function_int, comp_fun_int, HMAX);
    DIE(new->papers_ht == NULL, "calloc error");

    new->citation_ht = createHashTable(hash_function_int, comp_fun_int, HMAX);
    DIE(new->citation_ht == NULL, "calloc error");

    new->venue_total_ht = createHashTable(hash_function_string,
                                            comp_func_string, HMAX);
    DIE(new->venue_total_ht == NULL, "calloc error");

    new->venue_cnt_ht = createHashTable(hash_function_string,
                                            comp_func_string, HMAX);
    DIE(new->venue_cnt_ht == NULL, "calloc error");

    new->inst_and_field_ht = createHashTable(hash_function_string,
                                                comp_func_string, HMAX);
    DIE(new->inst_and_field_ht == NULL, "calloc error");

    new->author_ht = createHashTable(hash_function_int, comp_fun_int, HMAX);
    DIE(new->author_ht == NULL, "calloc error");

    new->year_cnt = calloc(3021, sizeof(int));
    DIE(new->year_cnt == NULL, "calloc error");

    return new;
}

// funcția eliberează memoria alocată structrii PublData
void destroy_publ_data(PublData* data) {
    for (int i = 0; i < data->papers_ht->hmax; i++) {
        for (Node* it = data->papers_ht->table[i]; it;) {
            paper_t *aux = (paper_t *) it->data;
            if (aux != NULL) {
                free(aux->title);
                free(aux->venue);
                for (int j = 0; j < aux->num_fields; j++) {
                    free(aux->fields[j]);
                }
                free(aux->fields);
                free(aux->author_ids);
                free(aux->references);
                free(aux);
            }
            Node* tmp = it;
            it = it->next;
            free(tmp);
        }
    }
    free(data->papers_ht->table);
    free(data->papers_ht);

    if (data->visited_ht) freeTable(data->visited_ht, 0, 0);
    freeTable(data->citation_ht, 1, 0);
    freeTable(data->venue_total_ht, 1, 0);
    freeTable(data->venue_cnt_ht, 1, 0);
    freeTable(data->inst_and_field_ht, 1, 1);

    for (int i = 0; i < data->author_ht->hmax; i++) {
        for (Node* it = data->author_ht->table[i]; it;) {
            author_t *aux = (author_t *) it->data;
            if (aux != NULL) {
                free(aux->name);
                free(aux->institution);
                free(aux->papers_id);
                free(aux->neighbours);
                free(aux);
            }
            Node* tmp = it;
            it = it->next;
            free(tmp);
        }
    }
    free(data->author_ht->table);
    free(data->author_ht);

    free(data->year_cnt);
    free(data);
}

// funcția creează un element de tip paper_t
// care conține informațiile corespunzătoare unui articol
paper_t *create_paper(const char* title, const char* venue,
    const int year, const int64_t* author_ids, const int num_authors,
    const char** fields, const int num_fields, const int64_t id,
    const int64_t* references, const int num_refs) {

    DIE(title == NULL, "no title");
    DIE(venue == NULL, "no venue");

    paper_t *data = calloc(1, sizeof(paper_t));
    DIE(data == NULL, "data calloc");

    data->title = strdup(title);
    DIE(data->title == NULL, "title malloc");

    data->id = id;
    data->year = year;

    data->venue = strdup(venue);
    DIE(data->venue == NULL, "venue malloc");

    data->num_fields = num_fields;
    data->fields = calloc(num_fields, sizeof(char *));
    DIE(data->fields == NULL, "fields calloc");

    for (int i = 0; i < num_fields; i++) {
        data->fields[i] = strdup(fields[i]);
        DIE(data->fields[i] == NULL, "fields malloc");
    }

    data->num_authors = num_authors;

    data->author_ids = calloc(num_authors, sizeof(int64_t));
    DIE(data->author_ids == NULL, "author_ids calloc");

    memcpy(data->author_ids, author_ids, sizeof(int64_t) * num_authors);

    data->num_refs = num_refs;
    data->references = calloc(num_refs, sizeof(int64_t));
    DIE(data->references == NULL, "references calloc");
    memcpy(data->references, references, sizeof(int64_t) * num_refs);

    return data;
}

// funcția creează un element de tip author_t
// care conține informații despre un anumit autor
author_t *create_author(int n, const char** author_names,
    const int64_t* author_ids, const char** institutions) {

    author_t *new = calloc(1, sizeof(author_t));
    DIE(new == NULL, "calloc error");

    new->id = author_ids[n];
    new->name = strdup(author_names[n]);
    DIE(new->name == NULL, "malloc error");
    new->institution = strdup(institutions[n]);
    DIE(new->institution == NULL, "malloc error");

    new->num_papers = 0;
    new->papers_id = calloc(NMAX, sizeof(int64_t));
    DIE(new->papers_id == NULL, "calloc error");

    new->num_neighbours = 0;
    new->neighbours = calloc(NMAX, sizeof(int64_t));
    DIE(new->neighbours == NULL, "calloc error");

    return new;
}

static int get_ciations(PublData* data, int64_t id) {
    int *citations = getData(data->citation_ht, &id);
    return (citations ? *citations : 0);
}

// funcția populează cu informații structura PublData
// pe baza datelor primite
void add_paper(PublData* data, const char* title, const char* venue,
    const int year, const char** author_names, const int64_t* author_ids,
    const char** institutions, const int num_authors, const char** fields,
    const int num_fields, const int64_t id, const int64_t* references,
    const int num_refs) {

    // hashtable-ul de paper-uri
    paper_t *new = create_paper(title, venue, year, author_ids,
            num_authors, fields, num_fields, id, references, num_refs);

    addEntry(data->papers_ht, &new->id, new);
    // hashtable-ul de jurnale
    for (int i = 0; i < num_refs; i++) {
        inc(data->citation_ht, &new->references[i]);
        paper_t *ciuvac = getData(data->papers_ht, &new->references[i]);
        if (ciuvac) {
            inc(data->venue_total_ht, ciuvac->venue);
        }
    }

    inc(data->venue_cnt_ht, new->venue);
    // adauga daca articolul apare la un moment dat de timp
    add(data->venue_total_ht, new->venue, get(data->citation_ht, &new->id));

    // hashtable-ul de autori
    for (int i = 0; i < num_authors; i++) {
        author_t *new_auth = getData(data->author_ht, &new->author_ids[i]);
        if (!new_auth) {
            new_auth = create_author(i, author_names, author_ids, institutions);
            addEntry(data->author_ht, &new->author_ids[i], new_auth);
        }
        new_auth->papers_id[new_auth->num_papers] = id;
        new_auth->num_papers++;
        for (int j = 0; j < num_authors; j++) {
            if (i != j) {
                new_auth->neighbours[new_auth->num_neighbours] = author_ids[j];
                new_auth->num_neighbours++;
            }
        }
        // hashtable-ul de institution+field
        for (int j = 0; j < num_fields; j++) {
            char *inst_field = calloc(strlen(institutions[i])
                + strlen(fields[j]) + 1, sizeof(char));
            strncat(inst_field, institutions[i], M);
            strncat(inst_field, fields[j], M);

            int64_t* authors = getData(data->inst_and_field_ht, inst_field);
            if (!authors) {
                authors = calloc(200, sizeof(int64_t));
                addEntry(data->inst_and_field_ht, inst_field, authors);
            } else {
                free(inst_field);
            }

            int k = 0;
            while (authors[k]) {
                if (authors[k] == new_auth->id) {
                    k = -1;
                    break;
                }
                k++;
            }

            if (k >= 0) {
                authors[k] = new_auth->id;
            }
        }
    }
    // vectorul de ani
    data->year_cnt[year]++;
}
// functia creeaza hashtable ul de vizitati, dar
// initial elibereaza toata memoria
static void clearVisited(PublData *data) {
    if (data->visited_ht) freeTable(data->visited_ht, 0, 0);
    data->visited_ht = createHashTable(hash_function_int, comp_fun_int, NMAX);
}

// funcția arată dacă un anumit element a fost sau nu vizitat
static int isVisited(PublData *data, int64_t id) {
    if (getData(data->visited_ht, &id)) {
        return 1;
    } else {
        return 0;
    }
}

// funcția marchează un element ca fiind vizitat
static void setVisited(PublData *data, int64_t *id) {
    addEntry(data->visited_ht, id, id);
}

// funcția întoarce articolul mai vechi dintre cele două primite
static paper_t* get_oldest_paper(PublData* data, paper_t* a, paper_t* b) {
    if (!a) return b;
    if (!b) return a;
    // alegere in functie de anul aparitiei articolului
    if (a->year > b->year) return b;
    if (a->year < b->year) return a;
    // alegere in functie de numărul maxim de citări
    int cit_a = get_ciations(data, a->id);
    int cit_b = get_ciations(data, b->id);
    if (cit_a > cit_b) return a;
    if (cit_a < cit_b) return b;
    // alegere in functie de id-ul articolului
    return a->id > b->id ? b : a;
}

// funcția găsește cea mai veche influență
static paper_t* get_oldest_inf_helper(PublData* data, paper_t* paper) {
    setVisited(data, &paper->id);
    paper_t* oldest_paper = NULL;
    for (int i = 0; i < paper->num_refs; i++) {
        paper_t* paper_ref = getData(data->papers_ht, &paper->references[i]);
        if (paper_ref && !isVisited(data, paper_ref->id)) {
            oldest_paper = get_oldest_paper(data, oldest_paper, paper_ref);
            setVisited(data, &paper_ref->id);
            paper_t* tmp = get_oldest_inf_helper(data, paper_ref);
            oldest_paper = get_oldest_paper(data, oldest_paper, tmp);
        }
    }
    return oldest_paper;
}

// funcția găsește cea mai veche influență a articolului
// al cărui id este primit ca parametru
char* get_oldest_influence(PublData* data, const int64_t id_paper) {
    clearVisited(data);
    int64_t x = id_paper;
    paper_t* oldest_paper = get_oldest_inf_helper(data,
                                getData(data->papers_ht, &x));
    return (oldest_paper ? oldest_paper->title : "None");
}

// funcția calculează factorul de impact al jurnalului primit ca parametru
float get_venue_impact_factor(PublData* data, const char* venue) {
    return 1.0 * get(data->venue_total_ht, (void *)venue) /
            (1.0 * get(data->venue_cnt_ht, (void *)venue));
}

int get_number_of_influenced_papers(__attribute__((unused)) PublData* data,
    __attribute__((unused)) const int64_t id_paper,
    __attribute__((unused)) const int distance) {
    /* TODO: implement get_number_of_influenced_papers */

    return -1;
}

// funcția calculează distanța erdos dintre doi autori
int get_erdos_distance(PublData* data, const int64_t id1, const int64_t id2) {
    int first = 0, last = 0, dist, i;
    int64_t x = id1;
    int64_t *queue = calloc(N, sizeof(int64_t));
    int *v = calloc(N, sizeof(int)), index = 0;
    if (id1 == id2) {
        return 0;
    }

    Hashtable *visited_ht = createHashTable(hash_function_int,
                                            comp_fun_int, NMAX);
    DIE(visited_ht == NULL, "calloc error");

    while (first <= last) {
        author_t *author = getData(data->author_ht, &x);

        if (first == 0 && last == 0 && author->id == id1) {
            v[index] = 1;
            index++;
            addEntry(visited_ht, &author->id, &v[index - 1]);
            dist = 1;
        } else {
            dist = *(int *)getData(visited_ht, &author->id) + 1;
        }

        for (i = 0; i < author->num_neighbours && last < N; i++) {
            // cazul în care este găsit autorul căutat
            if (author->neighbours[i] == id2) {
                free(queue);
                free(v);
                for (int k = 0; k < visited_ht->hmax; k++) {
                    for (Node* it = visited_ht->table[k]; it;) {
                        Node* tmp = it;
                        it = it->next;
                        free(tmp);
                    }
                }
                free(visited_ht->table);
                free(visited_ht);

                return dist;
            }
            // adăugarea vecinilor autorului curent
            // (care nu au fost deja vizitați) în coadă
            author_t *aux = getData(data->author_ht, &author->neighbours[i]);
            int *ok = getData(visited_ht, &aux->id);
            if (ok == NULL) {
                v[index] = dist;
                index++;
                addEntry(visited_ht, &aux->id, &v[index - 1]);
                queue[last] = author->neighbours[i];
                last++;
            }
        }

        x = queue[first];
        first++;
    }

    free(v);
    free(queue);
    for (int k = 0; k < visited_ht->hmax; k++) {
        for (Node* it = visited_ht->table[k]; it;) {
            Node* tmp = it;
            it = it->next;
            free(tmp);
        }
    }
    free(visited_ht->table);
    free(visited_ht);

    return -1;
}

char** get_most_cited_papers_by_field(__attribute__((unused)) PublData* data,
    __attribute__((unused)) const char* field,
    __attribute__((unused)) int* num_papers) {
    /* TODO: implement get_most_cited_papers_by_field */

    return NULL;
}

// funcția calculează numărul de articole publicate într-o perioadă stabilită
int get_number_of_papers_between_dates(PublData* data, const int early_date,
    const int late_date) {
    int n = 0, i;
    for (i = early_date; i <= late_date; i++) {
        n += data->year_cnt[i];
    }
    return n;
}
// functia returneaza numarul de autori dintr-o institutie care au scris
// un articol intr-un anumit domeniu
int get_number_of_authors_with_field(PublData* data, const char* institution,
    const char* field) {
    // am concatenat institutia si domeniul
    char *inst_field = calloc(strlen(institution) + strlen(field) + 1,
                                sizeof(char));
    snprintf(inst_field, M, "%s", institution);
    snprintf(inst_field + strlen(institution), M, "%s", field);

    int64_t *authors = getData(data->inst_and_field_ht, inst_field);
    free(inst_field);
    // verific daca autorul din institutia respectiva a scris
    // macar un articol in ales domeniu si exista in hashtable
    if (authors == NULL) {
        return 0;
    }

    int k = 0;
    while (authors[k++]) {
        continue;
    }
    // returnez numarul de autori
    return k - 1;
}

// funcția realizează o histogramă a citărilor articolelor unui autor
int* get_histogram_of_citations(PublData* data, const int64_t id_author,
    int* num_years) {
    int64_t x = id_author;
    author_t* author = getData(data->author_ht, &x);
    // verific daca exista acel autor
    if (author == NULL) {
        *num_years = 0;
        return NULL;
    }
    // presupun ca incep de la 2021
    int minYear = 2021;
    paper_t* papers[author->num_papers];
    // extrag informatia despre articolele scrise de autor
    for (int i = 0; i < author->num_papers; i++) {
        papers[i] = getData(data->papers_ht, &author->papers_id[i]);
        if (minYear > papers[i]->year) {
            minYear = papers[i]->year;
        }
    }
    // actualizez vectorul de ani
    *num_years = 2021 - minYear;
    int *cnt_years = calloc(*num_years, sizeof(int));
    // formez vectorul final
    for (int i = 0; i < author->num_papers; i++) {
        cnt_years[2020 - papers[i]->year] += get_ciations(data, papers[i]->id);
    }

    return cnt_years;
}

// funcția determină o ordine de citire a articolelor
// care l-au influențat pe cel al cărui id este primit ca parametru
char** get_reading_order(PublData* data, const int64_t id_paper,
    const int distance, int* num_papers) {
    *num_papers = 0;
    paper_t **queue = calloc(NMAX, sizeof(paper_t *)), *curr = NULL;
    int64_t x = id_paper;
    int first = 0, last = 0, dist = 0, ok = 1, ind;
    int *v = calloc(5000, sizeof(int)), index = 0;
    Hashtable *visited_ht = createHashTable(hash_function_int,
                                            comp_fun_int, NMAX);
    DIE(visited_ht == NULL, "calloc error");
    paper_t **refs_curr = calloc(NMAX, sizeof(paper_t *));
    DIE(refs_curr == NULL, "calloc error");
    curr = getData(data->papers_ht, &x);

    while (first <= last && dist <= distance && (last != 0
            || (first == 0 && last == 0 && ok))) {
        ok = 0;
        if (curr) {
            if (first == 0 && last == 0 && curr->id == id_paper) {
                v[index] = 1;
                index++;
                addEntry(visited_ht, &curr->id, &v[index - 1]);
                dist = 1;
            } else {
                dist = *(int *)getData(visited_ht, &curr->id) + 1;
            }

            ind = 0;
            for (int i = 0; i < curr->num_refs; i++) {
                paper_t *a = getData(data->papers_ht, &curr->references[i]);
                if (a) {
                    refs_curr[ind] = a;
                    ind++;
                }
            }

            for (int i = 0; i < ind; i++) {
                if (refs_curr[i] && !getData(visited_ht, &refs_curr[i]->id)) {
                    *num_papers = *num_papers + 1;
                    queue[last] = refs_curr[i];
                    last++;
                    v[index++] = dist;
                    addEntry(visited_ht, &refs_curr[i]->id, &v[index - 1]);
                }
            }
        }
        curr = queue[first];
        first++;
    }

    char **out = calloc(*num_papers, sizeof(char *));
    DIE(out == NULL, "calloc error");
    for (int i = 0; i < last - 1; i++) {
        for (int j = i + 1; j < last; j++) {
            if ((queue[i]->year < queue[j]->year) ||
                (queue[i]->year == queue[j]->year &&
                queue[i]->id < queue[j]->id)) {
                paper_t *aux = queue[i];
                queue[i] = queue[j];
                queue[j] = aux;
            }
        }
    }
    int out_i = 0;
    for (int i = last; i >= 0; i--) {
        if (queue[i]) {
            out[out_i] = strdup(queue[i]->title);
            out_i++;
            DIE(out[out_i - 1] == NULL, "malloc error");
        }
    }
    free(refs_curr);
    free(v);
    free(queue);
    for (int k = 0; k < visited_ht->hmax; k++) {
        for (Node* it = visited_ht->table[k]; it;) {
            Node* tmp = it;
            it = it->next;
            free(tmp);
        }
    }
    free(visited_ht->table);
    free(visited_ht);
    return out;
}

char* find_best_coordinator(__attribute__((unused)) PublData* data,
    __attribute__((unused)) const int64_t id_author) {
    /* TODO: implement find_best_coordinator */

    return NULL;
}
