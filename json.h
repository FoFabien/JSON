#ifndef JSON_H_INCLUDED
#define JSON_H_INCLUDED

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

// types definitions
typedef enum
{
    JSONUNDEF = 0,
    JSONOBJ = 1,
    JSONLIST = 2,
    JSONSTR = 3,
    JSONPRIM = 4,
    JSONFLOAT = 5,
    JSONINT = 6,
    JSONBOOL = 7
} jsontype; // type of the value in the json object

typedef struct
{
    jsontype type;
    void* ptr;
} hjson; // json object. size is only used by the list type. ptr can be anything

typedef struct
{
    void *next;
    char *key;
    hjson *item;
} jdict_entry; // dictionary key/value pair

typedef struct
{
    jdict_entry **hashtab;
} jdict; // dictionary object used for json objects, in a python like fashion

typedef struct
{
    hjson** list;
    size_t size;
    size_t reserved;
} jlist; // list object used for json lists

#define LISTSIZE 10 // default size of a json list
#define HASHSIZE 100 // size of the hash table. bigger = less risk of collision but higher memory usage

// functions declarations
static double _parse_value(const char *js, const size_t len, size_t *i, char* err);
static int _parse_hexa(const char *js, size_t *i, char** it);
static char* _parse_string(const char *js, const size_t len, size_t *i);

static void jsonFree(hjson* json);
static hjson* jsonparse_primitive(const char *js, const size_t len, size_t *i);
static hjson* jsonparse_value(const char *js, const size_t len, size_t *i);
static hjson* jsonparse_string(const char *js, const size_t len, size_t *i);
static hjson* jsonparse_array(const char *js, const size_t len, size_t *i);
static hjson* jsonparse_obj(const char *js, const size_t len, size_t *i);

static int _write_obj(FILE* f, hjson* json);
static int _write_array(FILE* f, hjson* json);
static int _write_value(FILE* f, hjson* json);
static int _write_primitive(FILE* f, hjson* json);
static int _write_string(FILE* f, hjson* json);
static int _write_obj(FILE* f, hjson* json);

// dictionary functions
// hash a string
static unsigned jdicthash(char *s)
{
    unsigned val;
    for (val = 0; *s != '\0'; s++)
        val = *s + 31 * val;
    return val % HASHSIZE;
}

// search an element in the dictionary
static jdict_entry* jdictlookup(jdict_entry** hashtab, char* s)
{
    jdict_entry* entry;
    for (entry = hashtab[jdicthash(s)]; entry != NULL; entry = entry->next)
        if(strcmp(s, entry->key) == 0)
            return entry;
    return NULL;
}

// add a new element in the dictionary. previous one with the same key is deleted.
static jdict_entry* jdictinstall(jdict_entry** hashtab, char *key, hjson *item)
{
    jdict_entry* entry = NULL;
    unsigned val;
    if((entry = jdictlookup(hashtab, key)) == NULL)
    {
        entry = malloc(sizeof(jdict_entry));
        if (entry == NULL || (entry->key = strdup(key)) == NULL)
            return NULL;
        val = jdicthash(key);
        entry->next = hashtab[val];
        hashtab[val] = entry;
    }
    else jsonFree(entry->item);
    entry->item = item;
    return entry;
}

// remove an element
static void jdictuninstall(jdict_entry** hashtab, char *key)
{
    jdict_entry* entry = NULL;
    unsigned val;
    if((entry = jdictlookup(hashtab, key)) != NULL)
    {
        jsonFree(entry->item);
        free(entry);
        val = jdicthash(key);
        hashtab[val] = NULL;
    }
}

// alloc the struct
static jdict* jdictalloc()
{
    jdict* d = malloc(sizeof(jdict));
    d->hashtab = calloc(HASHSIZE, sizeof(jdict_entry*));
    return d;
}

// free the memory
static void jdictfree(jdict* d)
{
    int i;
    for(i = 0; i < HASHSIZE; ++i)
    {
        jdict_entry* entry = NULL;
        for(entry = d->hashtab[i]; entry != NULL;)
        {
            jdict_entry* to_free = entry;
            entry = entry->next;
            jsonFree(to_free->item);
            free(to_free->key);
            free(to_free);
        }
    }
    free(d->hashtab);
    free(d);
}

// list functions
// increase the max size
static void jlist_up_reserve(jlist* l)
{
    hjson** list = malloc(l->reserved*2*sizeof(hjson*));
    memcpy(list, l->list, l->size*sizeof(hjson*));
    free(l->list);
    l->list = list;
    l->reserved *= 2;
}

// append an element
static void jlist_append(jlist* l, hjson* json)
{
    if(l->size >= l->reserved) jlist_up_reserve(l);
    l->list[l->size] = json;
    l->size++;
}

// alloc the struct
static jlist* jlistalloc()
{
    jlist* l = malloc(sizeof(jlist));
    l->reserved = LISTSIZE;
    l->list = malloc(sizeof(hjson*)*LISTSIZE);
    l->size = 0;
    return l;
}

// free the memory
static void jlistfree(jlist* l)
{
    size_t i;
    for(i = 0; i < l->size; ++i)
        jsonFree(l->list[i]);
    free(l->list);
    free(l);
}

// create a json object of the specified type
static hjson* jsonalloc(jsontype type, void* init_value)
{
    hjson* json = malloc(sizeof(hjson));
    json->type = type;
    switch(json->type)
    {
        case JSONUNDEF:
            free(json);
            return NULL;
        case JSONOBJ:
            json->ptr = jdictalloc();
            break;
        case JSONLIST:
            json->ptr = jlistalloc();
            break;
        case JSONSTR:
        case JSONPRIM:
        case JSONFLOAT:
        case JSONINT:
        case JSONBOOL:
            json->ptr = init_value;
            break;
    }
    return json;
}

// free the object and its childs if any
static void jsonFree(hjson* json)
{
    if(json == NULL)
        return;
    switch(json->type)
    {
        case JSONUNDEF:
            break;
        case JSONOBJ:
            jdictfree(json->ptr);
            break;
        case JSONLIST:
            jlistfree(json->ptr);
            break;
        case JSONSTR:
        case JSONPRIM:
        case JSONFLOAT:
        case JSONINT:
        case JSONBOOL:
            free(json->ptr);
            break;
    }
    free(json);
}

// read a numerical value and convert it into an usable string
static double _parse_value(const char *js, const size_t len, size_t *i, char* err)
{
    double d = 0.f;
    double e = 0.f;
    int dot = 0;
    int state = 0;
    int sign = 0;
    int num = 0;
    float decimal = 10.f;
    size_t beg = *i;
    for(; *i < len && js[*i] != '\0' && state == 0; ++(*i))
    {
        char c = js[*i];
        switch(c)
        {
            case '-':
                if(*i != beg || sign == 1)
                    goto parse_value_err;
                sign = 1;
                break;
            case '.':
                if(*i == beg || dot >= 1 || num == 0)
                    goto parse_value_err;
                dot = 1;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                num = 1;
                if(dot == 0) d = d * 10 + (c - '0');
                else
                {
                    d = d + (c - '0') / decimal;
                    decimal *= 10.f;
                }
                break;
            case 'e':
            case 'E':
                if(*i == beg || num == 0)
                    goto parse_value_err;
                state = 2;
                break;
            case '\t':
            case '\r':
            case '\n':
            case ' ':
            case ',':
            case '}':
            case ']':
                state = 1;
                break;
            default:
                goto parse_value_err;
        }
    }
    if(sign) d = -d;
    if(state == 2)
    {
        sign = 0;
        num = 0;
        beg = *i;
        for(; *i < len && js[*i] != '\0' && state == 2; ++(*i))
        {
            char c = js[*i];
            switch(c)
            {
                case '+':
                    if(*i != beg || sign != 0)
                        goto parse_value_err;
                    sign = 2;
                    break;
                case '-':
                    if(*i != beg || sign != 0)
                        goto parse_value_err;
                    sign = 1;
                    break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    num = 1;
                    e = e * 10 + (c - '0');
                    break;
                case '\t':
                case '\r':
                case '\n':
                case ' ':
                case ',':
                case '}':
                case ']':
                    state = 1;
                    break;
                default:
                    goto parse_value_err;
            }
        }
        if(sign == 1) e = - e;
        d = pow(d, e);
    }
    --(*i);
    if(state == 0) goto parse_value_err;
    return d;
parse_value_err:
    *err = -1;
    return 0;
}

// read hexadecimal characters (used by \u)
static int _parse_hexa(const char *js, size_t *i, char** it)
{
    int n;
    unsigned char r;
    for(n = 0; n < 4; ++n)
    {
        ++(*i);
        char c = js[*i];
        switch(c)
        {
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                if(n % 2 == 0)
                    r = c - 'a' + 10;
                else
                    r = (r << 4) +  c - 'a' + 10;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if(n % 2 == 0)
                    r = c - '0';
                else
                    r = (r << 4) +  c - '0';
                break;
            default:
                return -1;
        }
        if((n == 1 && r >= 0xc0) || n == 3)
        {
            **it = r;
            ++(*it);
        }
    }
    return 0;
}

// read a json string
static char* _parse_string(const char *js, const size_t len, size_t *i)
{
    int escape = 0;
    size_t beg = *i;
    int state = 0;
    char* str = NULL;
    char* res = NULL;
    char* it = NULL;
    // get the string length to prepare a buffer
    for(; *i < len && js[*i] != '\0' && state == 0; ++(*i))
    {
        char c = js[*i];
        switch(c)
        {
            case '\\':
                if(escape == 0)
                    escape = 1;
                else
                    escape = 0;
                break;
            case '"':
                if(escape == 0)
                    state = 1;
                else
                    escape = 0;
                break;
            default:
                escape = 0;
                break;
        }
    }
    if(escape != 0 || state == 0)
        return NULL;
    str = malloc(sizeof(char)*(*i-beg+1));
    it = &(str[0]);
    for(; beg < *i - 1 && state != 2; ++beg)
    {
        char c = js[beg];
        switch(c)
        {
            case '\\':
                ++beg;
                if(beg < *i - 1)
                {
                    c = js[beg];
                    switch(c)
                    {
                        case '"':
                        case '\\':
                        case '/':
                            *it = c;
                            break;
                        case 'b':
                            *it = '\b';
                            break;
                        case 'f':
                            *it = '\f';
                            break;
                        case 'n':
                            *it = '\n';
                            break;
                        case 'r':
                            *it = '\r';
                            break;
                        case 't':
                            *it = '\t';
                            break;
                        case 'u':
                            if(beg >= *i - 5 || _parse_hexa(js, &beg, &it) != 0)
                                state = 2;
                            --it;
                            break;
                        default:
                            state = 2;
                            break;
                    }
                    ++it;
                }
                break;
            default:
                *it = c;
                ++it;
                break;
        }
    }
    if(state == 2)
    {
        free(str);
        return NULL;
    }
    *it = '\0';
    res = strdup(str);
    free(str);
    --(*i);
    return res;
}

// read either a json boolean or null value
static hjson* jsonparse_primitive(const char *js, const size_t len, size_t *i)
{
    switch(js[*i])
    {
        case 't':
            if(strncmp(js+(*i)*sizeof(char), "true", 4) == 0)
            {
                char* b = malloc(sizeof(char));
                *b = 1;
                *i+=3;
                return jsonalloc(JSONBOOL, b);
            }
            else
                return NULL;
            break;
        case 'f':
            if(strncmp(js+(*i)*sizeof(char), "false", 4) == 0)
            {
                char* b = malloc(sizeof(char));
                *b = 0;
                *i+=4;
                return jsonalloc(JSONBOOL, b);
            }
            else
                return NULL;
            break;
        case 'n':
            if(strncmp(js+(*i)*sizeof(char), "null", 4) == 0)
            {
                char* b = malloc(sizeof(char));
                *b = 0;
                *i+=3;
                return jsonalloc(JSONPRIM, b);
            }
            else
                return NULL;
            break;
        default:
            return NULL;
    }
    return NULL;
}

// read a json object
static hjson* jsonparse_obj(const char *js, const size_t len, size_t *i)
{
    hjson* obj = jsonalloc(JSONOBJ, NULL);
    jdict* d = obj->ptr;
    char* key = NULL;
    hjson* item = NULL;
    int state = 0;
    for(; *i < len && js[*i] != '\0'; ++(*i))
    {
        char c = js[*i];
        switch (c)
        {
            case '\t':
            case '\r':
            case '\n':
            case ' ':
                break;
            case '}':
                if(state != 3)
                    goto obj_err;
                return obj;
                break;
            case ',':
                if(state != 3)
                    goto obj_err;
                state = 0;
                break;
            case ':':
                if(state != 1)
                    goto obj_err;
                state = 2;
                break;
            case '{':
                if(state != 2)
                    goto obj_err;
                ++(*i);
                item = jsonparse_obj(js, len, i);
                if(item == NULL || jdictinstall(d->hashtab, key, item) == NULL)
                    goto obj_err;
                free(key);
                key = NULL;
                item = NULL;
                state = 3;
                break;
            case '[':
                if(state != 2)
                    goto obj_err;
                ++(*i);
                item = jsonparse_array(js, len, i);
                if(item == NULL || jdictinstall(d->hashtab, key, item) == NULL)
                    goto obj_err;
                free(key);
                key = NULL;
                item = NULL;
                state = 3;
                break;
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if(state != 2)
                    goto obj_err;
                item = jsonparse_value(js, len, i);
                if(item == NULL || jdictinstall(d->hashtab, key, item) == NULL)
                    goto obj_err;
                --(*i);
                free(key);
                key = NULL;
                item = NULL;
                state = 3;
                break;
            case 't':
            case 'f':
            case 'n':
                if(state != 2)
                    goto obj_err;
                item = jsonparse_primitive(js, len, i);
                if(item == NULL || jdictinstall(d->hashtab, key, item) == NULL)
                    goto obj_err;
                free(key);
                key = NULL;
                item = NULL;
                state = 3;
                break;
            case '\"':
                {
                    if(state % 2 == 1)
                        goto obj_err;
                    ++(*i);
                    char* tmp = _parse_string(js, len, i);
                    if(tmp == NULL)
                        goto obj_err;
                    if(state == 0)
                    {
                        key = tmp;
                        state = 1;
                    }
                    else if(state == 2)
                    {
                        item = jsonalloc(JSONSTR, tmp);
                        if(item == NULL || jdictinstall(d->hashtab, key, item) == NULL)
                            goto obj_err;
                        free(key);
                        key = NULL;
                        item = NULL;
                        state = 3;
                    }
                    break;
                }
            default:
                goto obj_err;
        }
    }
obj_err:
    free(key);
    jsonFree(item);
    jsonFree(obj);
    return NULL;
}

// read a json list
static hjson* jsonparse_array(const char *js, const size_t len, size_t *i)
{
    hjson* list = jsonalloc(JSONLIST, NULL);
    jlist* l = list->ptr;
    int comma = 0;
    for(; *i < len && js[*i] != '\0'; ++(*i))
    {
        char c = js[*i];
        switch (c)
        {
            case '\t':
            case '\r':
            case '\n':
            case ' ':
                break;
            case ']':
                if(l->size > 0 && comma == 0)
                    goto list_err;
                return list;
                break;
            case ',':
                if(comma != 1)
                    goto list_err;
                comma = 0;
                break;
            case '{':
                ++(*i);
                jlist_append(l, jsonparse_obj(js, len, i));
                if(l->list[l->size-1] == NULL)
                    goto list_err;
                comma = 1;
                break;
            case '[':
                ++(*i);
                jlist_append(l, jsonparse_array(js, len, i));
                if(l->list[l->size-1] == NULL)
                    goto list_err;
                comma = 1;
                break;
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                jlist_append(l, jsonparse_value(js, len, i));
                if(l->list[l->size-1] == NULL)
                    goto list_err;
                comma = 1;
                --(*i);
                break;
            case 't':
            case 'f':
            case 'n':
                jlist_append(l, jsonparse_primitive(js, len, i));
                if(l->list[l->size-1] == NULL)
                    goto list_err;
                comma = 1;
                break;
            case '\"':
                ++(*i);
                jlist_append(l, jsonparse_string(js, len, i));
                if(l->list[l->size-1] == NULL)
                    goto list_err;
                comma = 1;
                break;
            default:
                goto list_err;
        }
    }
list_err:
    jsonFree(list);
    return NULL;
}

// call _parse_value and create either an integer or float json object
static hjson* jsonparse_value(const char *js, const size_t len, size_t *i)
{
    char err = 0;
    double d = _parse_value(js, len, i, &err);
    if(err != 0)
        return NULL;
    else
    {
        if(ceil(d) != d)
        {
            double *dm = malloc(sizeof(double));
            *dm = d;
            return jsonalloc(JSONFLOAT, dm);
        }
        else
        {
            int64_t *llu = malloc(sizeof(int64_t));
            *llu = d;
            return jsonalloc(JSONINT, llu);
        }
    }
}

// call _parse_string and create either a string json object
static hjson* jsonparse_string(const char *js, const size_t len, size_t *i)
{
    char *str = _parse_string(js, len, i);
    if(str == NULL)
        return NULL;
    return jsonalloc(JSONSTR, str);
}


// transform your string into a json object
static hjson* jsonParse(const char *js, const size_t len)
{
    hjson* root = NULL;
    hjson* current = NULL;
    size_t i;

    for(i = 0; i < len && js[i] != '\0'; ++i)
    {
        char c = js[i];
        switch (c)
        {
            case '\t':
            case '\r':
            case '\n':
            case ' ':
                break;
            case '{':
                ++i;
                current = jsonparse_obj(js, len, &i);
                if(current == NULL)
                    goto parse_err;
                if(root == NULL)
                    root = current;
                break;
            case '[':
                ++i;
                current = jsonparse_array(js, len, &i);
                if(current == NULL)
                    goto parse_err;
                if(root == NULL)
                    root = current;
                break;
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                current = jsonparse_value(js, len, &i);
                if(current == NULL)
                    goto parse_err;
                if(root == NULL)
                    root = current;
                break;
            case 't':
            case 'f':
            case 'n':
                current = jsonparse_primitive(js, len, &i);
                if(current == NULL)
                    goto parse_err;
                if(root == NULL)
                    root = current;
                break;
            case '\"':
                ++i;
                current = jsonparse_string(js, len, &i);
                if(current == NULL)
                    goto parse_err;
                if(root == NULL)
                    root = current;
                break;
            default:
                goto parse_err;
        }
    }
    return root;
parse_err:
    jsonFree(root);
    return NULL;
}

// get a pointer to the integer (for integer json objects)
static int64_t* jsonGetInt(hjson* json)
{
    if(json == NULL || json->type != JSONINT)
        return NULL;
    return json->ptr;
}

// get a pointer to the boolean (for boolean json objects)
static char* jsonGetBool(hjson* json)
{
    if(json == NULL || json->type != JSONBOOL)
        return NULL;
    return json->ptr;
}

// get a pointer to the double (for floating json objects)
static double* jsonGetFloat(hjson* json)
{
    if(json == NULL || json->type != JSONFLOAT)
        return NULL;
    return json->ptr;
}

// get the string (for string json objects)
static const char* jsonGetStr(hjson* json)
{
    if(json == NULL || json->type != JSONSTR)
        return NULL;
    return json->ptr;
}

// get the list (for list json objects)
static jlist* jsonGetList(hjson* json)
{
    if(json == NULL || json->type != JSONLIST)
        return NULL;
    return json->ptr;
}

// get the dictionary (for json objects)
static jdict* jsonGetObj(hjson* json)
{
    if(json == NULL || json->type != JSONOBJ)
        return NULL;
    return json->ptr;
}

// return 1 if it's a null value
static int jsonIsNull(hjson* json)
{
    if(json == NULL || json->type != JSONPRIM)
        return 0;
    return 1;
}

// return the list size (0 if not a list)
static size_t jsonListSize(hjson* json)
{
    if(json == NULL || json->type != JSONLIST)
        return 0;
    jlist* l = json->ptr;
    return l->size;
}

// append a json object to a json list (in the limit defined by JSON_MAX)
static void jsonListAppend(hjson* json, hjson* app)
{
    if(json == NULL || json->type != JSONLIST)
        return;
    jlist* l = json->ptr;
    jlist_append(l, app);
}

// set an element in a json list (previous one will be free)
static int jsonListSet(hjson* json, int index, hjson* add)
{
    if(json == NULL || json->type != JSONLIST)
        return -1;
    jlist* l = json->ptr;
    if(index >= l->size || index < 0)
        return -1;
    l->list[index] = add;
    return 0;
}

// get the item in a json object
static hjson* jsonObjGet(hjson* json, char* key)
{
    if(json == NULL || json->type != JSONOBJ)
        return NULL;
    jdict *d = json->ptr;
    jdict_entry* de = jdictlookup(d->hashtab, key);
    if(de == NULL)
        return NULL;
    return de->item;
}

// set an item in a json object
static int jsonObjSet(hjson* json, char* key, hjson* set)
{
    if(json == NULL || json->type != JSONOBJ)
        return -1;
    jdict *d = json->ptr;
    if(jdictinstall(d->hashtab, key, set) == NULL)
        return -1;
    return 0;
}

// delete an item in a json object
static void jsonObjDel(hjson* json, char* key)
{
    if(json == NULL || json->type != JSONOBJ)
        return;
    jdict *d = json->ptr;
    jdictuninstall(d->hashtab, key);
}

// read a file on disk and create a json object
static hjson* jsonRead(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    long int fs;
    char* buffer;
    hjson* json;

    if(f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    fs = ftell (f);

    if(fs <= 0)
        return NULL;
    fseek(f, 0, SEEK_SET);
    buffer = malloc(sizeof(char)*fs);
    if(fread(buffer, 1, fs, f) != fs)
    {
        free(buffer);
        return NULL;
    }

    json = jsonParse(buffer, fs);
    free(buffer);
    fclose(f);

    return json;
}

// write an object
static int _write_obj(FILE* f, hjson* json)
{
    jdict* d = jsonGetObj(json);
    jdict_entry* elem;
    size_t i;
    size_t first = 0;
    fwrite("{", 1, 1, f);
    for(i = 0; i < HASHSIZE; ++i)
    {
        for(elem = d->hashtab[i]; elem != NULL; elem = elem->next)
        {
            if(first != 0)
                fwrite(", ", 1, 2, f);
            first = 1;

            fwrite("\"", 1, 1, f);
            fwrite(elem->key, 1, strlen(elem->key), f);
            fwrite("\"", 1, 1, f);
            fwrite(":", 1, 1, f);
            switch(elem->item->type)
            {
                case JSONUNDEF:
                    return -1;
                case JSONOBJ:
                    if(_write_obj(f, elem->item) != 0)
                        return -1;
                    break;
                case JSONLIST:
                    if(_write_array(f, elem->item) != 0)
                        return -1;
                    break;
                case JSONSTR:
                    if(_write_string(f, elem->item) != 0)
                        return -1;
                    break;
                case JSONBOOL:
                case JSONPRIM:
                    if(_write_primitive(f, elem->item) != 0)
                        return -1;
                    break;
                case JSONFLOAT:
                case JSONINT:
                    if(_write_value(f, elem->item) != 0)
                        return -1;
                    break;
                default:
                    return -1;
            }
        }
    }
    fwrite("}", 1, 1, f);
    return 0;
}

// write a list
static int _write_array(FILE* f, hjson* json)
{
    jlist* ptr = jsonGetList(json);
    hjson** l = ptr->list;
    size_t i;
    fwrite("[", 1, 1, f);
    for (i = 0; i < ptr->size; ++i)
    {
        switch(l[i]->type)
        {
            case JSONUNDEF:
                return -1;
            case JSONOBJ:
                if(_write_obj(f, l[i]) != 0)
                    return -1;
                break;
            case JSONLIST:
                if(_write_array(f, l[i]) != 0)
                    return -1;
                break;
            case JSONSTR:
                if(_write_string(f, l[i]) != 0)
                    return -1;
                break;
            case JSONBOOL:
            case JSONPRIM:
                if(_write_primitive(f, l[i]) != 0)
                    return -1;
                break;
            case JSONFLOAT:
            case JSONINT:
                if(_write_value(f, l[i]) != 0)
                    return -1;
                break;
            default:
                return -1;
        }
        if(i != ptr->size-1)
            fwrite(", ", 1, 2, f);
    }
    fwrite("]", 1, 1, f);
    return 0;
}

// write either a float or int value
static int _write_value(FILE* f, hjson* json)
{
    char buf[60];
    switch(json->type)
    {
        case JSONFLOAT:
            {
                double *d = json->ptr;
                sprintf(buf, "%f", *d);
                break;
            }
        case JSONINT:
            {
                int64_t *l = json->ptr;
                sprintf(buf, "%lld", *l);
                break;
            }
        default:
            return -1;
    }
    fwrite(buf, 1, strlen(buf), f);
    return 0;
}

// write either a boolean or null value
static int _write_primitive(FILE* f, hjson* json)
{
    switch(json->type)
    {
        case JSONBOOL:
            {
                char *b = jsonGetBool(json);
                if(*b != 0)
                    fwrite("true", 1, 4, f);
                else
                    fwrite("false", 1, 5, f);
                break;
            }
        case JSONPRIM:
            {
                fwrite("null", 1, 4, f);
                break;
            }
        default:
            return -1;
    }
    return 0;
}

// write an unicode character
static int _write_hexa(FILE* f, unsigned char byte1, unsigned char byte2) // unsigned cast is important
{
    char c[4];
    fwrite("\\u", 1, 2, f);
    c[0] = (byte1 >> 4) & 0x0f;
    if(c[0] >= 10) c[0] += 'a' - 10;
    else c[0] += '0';
    c[1] = byte1 & 0x0f;
    if(c[1] >= 10) c[1] += 'a' - 10;
    else c[1] += '0';
    c[2] = (byte2 >> 4) & 0x0f;
    if(c[2] >= 10) c[2] += 'a' - 10;
    else c[2] += '0';
    c[3] = byte2 & 0x0f;
    if(c[3] >= 10) c[3] += 'a' - 10;
    else c[3] += '0';
    fwrite(c, 1, 4, f);
    return 0;
}

static int _write_string(FILE* f, hjson* json)
{
    char* str = json->ptr;
    size_t ss = strlen(str);
    int i;
    fwrite("\"", 1, 1, f);
    for(i = 0; i < ss; ++i)
    {
        char c = str[i];
        switch(c)
        {
            case '"':
                fwrite("\\\"", 1, 2, f);
                break;
            case '\\':
                fwrite("\\\\", 1, 2, f);
                break;
            case '/':
                fwrite("\\/", 1, 2, f);
                break;
            case '\b':
                fwrite("\\b", 1, 2, f);
                break;
            case '\f':
                fwrite("\\f", 1, 2, f);
                break;
            case '\n':
                fwrite("\\n", 1, 2, f);
                break;
            case '\r':
                fwrite("\\r", 1, 2, f);
                break;
            case '\t':
                fwrite("\\t", 1, 2, f);
                break;
            default:
                if((c & 0xff) >= 0xc0)
                {
                    if(i >= ss - 1) return -1;
                    ++i; // we need 2 characters
                    _write_hexa(f, c, str[i]);
                }
                else fwrite(&c, 1, 1, f);
                break;
        }
    }
    fwrite("\"", 1, 1, f);
    return 0;
}

// write a file on disk from a json object
static int jsonWrite(const char* filename, hjson* json)
{
    FILE* f = fopen(filename, "wb");

    if(f == NULL || json == NULL)
        return -1;

    switch(json->type)
    {
        case JSONUNDEF:
            return -1;
        case JSONOBJ:
            if(_write_obj(f, json) != 0)
                return -1;
            break;
        case JSONLIST:
            if(_write_array(f, json) != 0)
                return -1;
            break;
        case JSONSTR:
            if(_write_string(f, json) != 0)
                return -1;
            break;
        case JSONBOOL:
        case JSONPRIM:
            if(_write_primitive(f,json) != 0)
                return -1;
            break;
        case JSONFLOAT:
        case JSONINT:
            if(_write_value(f, json) != 0)
                return -1;
            break;
        default:
            return -1;
    }
    fclose(f);
    return 0;
}

#endif
