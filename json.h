#ifndef JSON_H_INCLUDED
#define JSON_H_INCLUDED

#include <stdint.h>

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
	size_t size;
} cjson; // json object. size is only used by the list type. ptr can be anything

typedef struct
{
	void *next;
	char *key;
	cjson *item;
} dict_entry; // dictionary key/value pair

typedef struct
{
	dict_entry **hashtab;
	size_t size;
} dict; // dictionary object used for json objects, in a python like fashion

#define JSON_MAX 64 // max size of a json list (TODO: dynamic resizing?)
#define HASHSIZE 100 // size of the hash table. bigger = less risk of collision but higher memory usage

// functions declarations
static char* _parse_value(const char *js, const size_t len, size_t *i, int *dot);
static int _parse_hexa(const char *js, size_t *i, char** it);
static char* _parse_string(const char *js, const size_t len, size_t *i);
static cjson* jsonparse_primitive(const char *js, const size_t len, cjson* json, size_t *i);

static void jsonFree(cjson* json);
static cjson* jsonparse_value(const char *js, const size_t len, cjson* json, size_t *i);
static cjson* jsonparse_string(const char *js, const size_t len, cjson* json, size_t *i);
static cjson* jsonparse_array(const char *js, const size_t len, cjson* json, size_t *i);
static cjson* jsonparse_obj(const char *js, const size_t len, cjson* json, size_t *i);

static int _write_obj(FILE* f, cjson* json);
static int _write_array(FILE* f, cjson* json);
static int _write_value(FILE* f, cjson* json);
static int _write_primitive(FILE* f, cjson* json);
static int _write_string(FILE* f, cjson* json);
static int _write_obj(FILE* f, cjson* json);

// hash a string
static unsigned dicthash(char *s)
{
	unsigned val;
	for (val = 0; *s != '\0'; s++)
		val = *s + 31 * val;
	return val % HASHSIZE;
}

// search an element in the dictionary
static dict_entry* dictlookup(dict_entry** hashtab, char* s)
{
	dict_entry* entry;
	for (entry = hashtab[dicthash(s)]; entry != NULL; entry = entry->next)
		if(strcmp(s, entry->key) == 0)
			return entry;
	return NULL;
}

// add a new element in the dictionary. previous one with the same key is deleted.
static dict_entry* dictinstall(dict_entry** hashtab, char *key, cjson *item)
{
	dict_entry* entry = NULL;
	unsigned val;
	if((entry = dictlookup(hashtab, key)) == NULL)
	{
		entry = malloc(sizeof(dict_entry));
		if (entry == NULL || (entry->key = strdup(key)) == NULL)
			return NULL;
		val = dicthash(key);
		entry->next = hashtab[val]; // (TOCHECK)
		hashtab[val] = entry;
	}
	else
		jsonFree(entry->item);
	entry->item = item;
	return entry;
}

// remove an element
static void dictuninstall(dict_entry** hashtab, char *key)
{
	dict_entry* entry = NULL;
	unsigned val;
	if((entry = dictlookup(hashtab, key)) != NULL)
	{
		jsonFree(entry->item);
		free(entry);
		val = dicthash(key);
		hashtab[val] = NULL;
	}
}

// free the memory
static void dictfree(dict* d)
{
	int i;
	for(i = 0; i < HASHSIZE && d->size; ++i)
    {
        dict_entry* entry = NULL;
        for(entry = d->hashtab[i]; entry != NULL; entry = entry->next)
        {
            jsonFree(entry->item);
            free(entry->key);
            free(entry);
        }
    }
	free(d->hashtab);
	free(d);
}

// create a json object of the specified type
static cjson* jsonalloc(jsontype type, void* init_value)
{
	cjson* json = malloc(sizeof(cjson));
	json->type = type;
	switch(json->type)
	{
		case JSONUNDEF:
			free(json);
			return NULL;
		case JSONOBJ:
			json->ptr = malloc(sizeof(dict));
			((dict*)(json->ptr))->size = 0;
			((dict*)(json->ptr))->hashtab = calloc(HASHSIZE, sizeof(dict_entry*));
			break;
		case JSONLIST:
			json->ptr = malloc(sizeof(cjson*)*JSON_MAX);
			json->size = 0;
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
static void jsonFree(cjson* json)
{
	int i;
	cjson** array;
	if(json == NULL)
		return;
	switch(json->type)
	{
		case JSONUNDEF:
			break;
		case JSONOBJ:
			dictfree(json->ptr);
			break;
		case JSONLIST:
			array = (cjson**)json->ptr;
			for(i = 0; i < JSON_MAX && i < json->size; ++i)
				jsonFree(array[i]);
			free(json->ptr);
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
// (TODO: exponant support)
static char* _parse_value(const char *js, const size_t len, size_t *i, int *dot)
{
	int sign = 0;
	int num = 0;
	size_t beg = *i;
	int stop = 0;
	char* str = NULL;
	for(; *i < len && js[*i] != '\0' && stop == 0; ++(*i))
	{
		char c = js[*i];
		switch(c)
		{
			case '-':
				if(*i != beg || sign == 1)
					return NULL;
				sign = 1;
				break;
			case '.':
				if(*i == beg || *dot == 1 || num == 0)
					return NULL;
				*dot = 1;
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
				break;
			default:
				if(num == 0)
					return NULL;
				stop = 1;
				break;
		}
	}
	--(*i);
	if(*i != beg)
	{
		str = malloc(sizeof(char)*(*i-beg+1));
		str[*i-beg] = '\0';
		memcpy(str, js+(beg*sizeof(char)), *i-beg);
		return str;
	}
	else
		return NULL;
}

// read hexadecimal characters (used by \u)
static int _parse_hexa(const char *js, size_t *i, char** it)
{
	int n;
	char r = 0;
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
static cjson* jsonparse_primitive(const char *js, const size_t len, cjson* json, size_t *i)
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
static cjson* jsonparse_obj(const char *js, const size_t len, cjson* json, size_t *i)
{
	cjson* obj = jsonalloc(JSONOBJ, NULL);
	dict* d = obj->ptr;
	char* key = NULL;
	cjson* item = NULL;
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
				if(d->size > 0 && state != 3)
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
				item = jsonparse_obj(js, len, obj, i);
				if(item == NULL || dictinstall(d->hashtab, key, item) == NULL)
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
				item = jsonparse_array(js, len, obj, i);
				if(item == NULL || dictinstall(d->hashtab, key, item) == NULL)
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
				item = jsonparse_value(js, len, obj, i);
				if(item == NULL || dictinstall(d->hashtab, key, item) == NULL)
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
				item = jsonparse_primitive(js, len, obj, i);
				if(item == NULL || dictinstall(d->hashtab, key, item) == NULL)
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
						if(item == NULL || dictinstall(d->hashtab, key, item) == NULL)
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
static cjson* jsonparse_array(const char *js, const size_t len, cjson* json, size_t *i)
{
	cjson* list = jsonalloc(JSONLIST, NULL);
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
				if(list->size > 0 && comma == 0)
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
				((cjson**)(list->ptr))[list->size] = jsonparse_obj(js, len, list, i);
				if(((cjson**)(list->ptr))[list->size] == NULL)
                    goto list_err;
				++list->size;
				comma = 1;
				break;
			case '[':
				++(*i);
				((cjson**)(list->ptr))[list->size] = jsonparse_array(js, len, list, i);
				if(((cjson**)(list->ptr))[list->size] == NULL)
                    goto list_err;
				++list->size;
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
				((cjson**)(list->ptr))[list->size] = jsonparse_value(js, len, list, i);
				if(((cjson**)(list->ptr))[list->size] == NULL)
                    goto list_err;
				++list->size;
				comma = 1;
				--(*i);
				break;
			case 't':
			case 'f':
			case 'n':
				((cjson**)(list->ptr))[list->size] = jsonparse_primitive(js, len, list, i);
				if(((cjson**)(list->ptr))[list->size] == NULL)
                    goto list_err;
				++list->size;
				comma = 1;
				break;
			case '\"':
				++(*i);
				((cjson**)(list->ptr))[list->size] = jsonparse_string(js, len, list, i);
				if(((cjson**)(list->ptr))[list->size] == NULL)
                    goto list_err;
				++list->size;
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
static cjson* jsonparse_value(const char *js, const size_t len, cjson* json, size_t *i)
{
	int dot = 0;
	char* str = _parse_value(js, len, i, &dot);
	if(str == NULL)
		return NULL;
	else
	{
		if(dot)
		{
			float *f = malloc(sizeof(float));
			*f = strtof(str, NULL);
			free(str);
			return jsonalloc(JSONFLOAT, f);
		}
		else
		{
			int64_t *d = malloc(sizeof(int64_t));
			*d = strtoull(str, NULL, 10);
			free(str);
			return jsonalloc(JSONINT, d);
		}
	}
}

// call _parse_string and create either a string json object
static cjson* jsonparse_string(const char *js, const size_t len, cjson* json, size_t *i)
{
	char *str = _parse_string(js, len, i);
	if(str == NULL)
		return NULL;
	return jsonalloc(JSONSTR, str);
}


// transform your string into a json object
static cjson* jsonParse(const char *js, const size_t len)
{
	cjson* root = NULL;
	cjson* current = NULL;
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
				current = jsonparse_obj(js, len, current, &i);
				if(current == NULL)
					goto parse_err;
				if(root == NULL)
					root = current;
				break;
			case '[':
				++i;
				current = jsonparse_array(js, len, current, &i);
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
				current = jsonparse_value(js, len, current, &i);
				if(current == NULL)
					goto parse_err;
				if(root == NULL)
					root = current;
				break;
			case 't':
			case 'f':
			case 'n':
				current = jsonparse_primitive(js, len, current, &i);
				if(current == NULL)
					goto parse_err;
				if(root == NULL)
					root = current;
				break;
			case '\"':
				++i;
				current = jsonparse_string(js, len, current, &i);
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
static int64_t* jsonGetInt(cjson* json)
{
	if(json == NULL || json->type != JSONINT)
		return NULL;
	return json->ptr;
}

// get a pointer to the boolean (for boolean json objects)
static char* jsonGetBool(cjson* json)
{
	if(json == NULL || json->type != JSONBOOL)
		return NULL;
	return json->ptr;
}

// get a pointer to the float (for float json objects)
static float* jsonGetFloat(cjson* json)
{
	if(json == NULL || json->type != JSONFLOAT)
		return NULL;
	return json->ptr;
}

// get the string (for string json objects)
static const char* jsonGetStr(cjson* json)
{
	if(json == NULL || json->type != JSONSTR)
		return NULL;
	return json->ptr;
}

// get the list (for list json objects)
static cjson** jsonGetList(cjson* json)
{
	if(json == NULL || json->type != JSONLIST)
		return NULL;
	return json->ptr;
}

// get the dictionary (for json objects)
static dict* jsonGetObj(cjson* json)
{
	if(json == NULL || json->type != JSONOBJ)
		return NULL;
	return json->ptr;
}

// return 1 if it's a null value
static int jsonIsNull(cjson* json)
{
	if(json == NULL || json->type != JSONPRIM)
		return 0;
	return 1;
}

// return the list size (0 if not a list)
static size_t jsonListSize(cjson* json)
{
	if(json == NULL || json->type != JSONLIST)
		return 0;
	return json->size;
}

// append a json object to a json list (in the limit defined by JSON_MAX)
static int jsonListAppend(cjson* json, cjson* app)
{
	if(json == NULL || json->type != JSONLIST || json->size >= JSON_MAX)
		return -1;
	((cjson**)(json->ptr))[json->size] = app;
	json->size++;
	return 0;
}

// set an element in a json list (previous one will be free)
static int jsonListSet(cjson* json, int index, cjson* add)
{
	if(json == NULL || json->type != JSONLIST || index >= json->size || index < 0)
		return -1;
	jsonFree(((cjson**)(json->ptr))[index]);
	((cjson**)(json->ptr))[index] = add;
	return 0;
}

// get the item in a json object
static cjson* jsonObjGet(cjson* json, char* key)
{
	if(json == NULL || json->type != JSONOBJ)
		return NULL;
	dict *d = json->ptr;
	dict_entry* de = dictlookup(d->hashtab, key);
	if(de == NULL)
		return NULL;
	return de->item;
}

// set an item in a json object
static int jsonObjSet(cjson* json, char* key, cjson* set)
{
	if(json == NULL || json->type != JSONOBJ)
		return -1;
	dict *d = json->ptr;
	if(dictinstall(d->hashtab, key, set) == NULL)
		return -1;
	return 0;
}

// delete an item in a json object
static void jsonObjDel(cjson* json, char* key)
{
	if(json == NULL || json->type != JSONOBJ)
		return;
	dict *d = json->ptr;
	dictuninstall(d->hashtab, key);
}

// read a file on disk and create a json object
static cjson* jsonRead(const char* filename)
{
	FILE* f = fopen(filename, "rb");
	long int fs;
	char* buffer;
	cjson* json;

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
static int _write_obj(FILE* f, cjson* json)
{
	dict* d = jsonGetObj(json);
	dict_entry* elem;
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
static int _write_array(FILE* f, cjson* json)
{
	cjson** l = jsonGetList(json);
	size_t i;
	fwrite("[", 1, 1, f);
	for (i = 0; i < json->size; ++i)
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
		if(i != json->size-1)
			fwrite(", ", 1, 2, f);
	}
	fwrite("]", 1, 1, f);
	return 0;
}

// write either a float or int value
static int _write_value(FILE* f, cjson* json)
{
	char buf[60];
	switch(json->type)
	{
		case JSONFLOAT:
			{
				float *f = json->ptr;
				sprintf(buf, "%f", *f);
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
static int _write_primitive(FILE* f, cjson* json)
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

static int _write_string(FILE* f, cjson* json)
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
				// to do: \u
				fwrite(&c, 1, 1, f);
				break;
		}
	}
	fwrite("\"", 1, 1, f);
	return 0;
}

// write a file on disk from a json object
static int jsonWrite(const char* filename, cjson* json)
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
