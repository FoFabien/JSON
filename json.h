#ifndef JSON_H_INCLUDED
#define JSON_H_INCLUDED

#include <stdint.h>

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
} jsontype;

typedef struct
{
	void* parent;
	jsontype type;
	void* ptr;
	size_t size;
} cjson;

typedef struct
{
	struct dict_entry *next;
	char *key;
	cjson *item;
} dict_entry;

typedef struct
{
	void* parent;
	dict_entry **hashtab;
	size_t size;
} dict;
#define JSON_MAX 64
#define HASHSIZE 100

static void jsonfree(cjson* json);
static cjson* jsonparse_value(const char *js, const size_t len, cjson* json, size_t *i);
static cjson* jsonparse_string(const char *js, const size_t len, cjson* json, size_t *i);
static cjson* jsonparse_array(const char *js, const size_t len, cjson* json, size_t *i);
static cjson* jsonparse_obj(const char *js, const size_t len, cjson* json, int *i);

static unsigned dicthash(char *s)
{
	unsigned val;
	for (val = 0; *s != '\0'; s++)
		val = *s + 31 * val;
	return val % HASHSIZE;
}

static dict_entry* dictlookup(dict_entry** hashtab, char* s)
{
	dict_entry* np;
	for (np = hashtab[dicthash(s)]; np != NULL; np = np->next)
		if(strcmp(s, np->key) == 0)
			return np;
	return NULL;
}

static dict_entry* dictinstall(dict_entry** hashtab, char *key, cjson *item)
{
	dict_entry* np = NULL;
	unsigned val;
	if ((np = dictlookup(hashtab, key)) == NULL)
	{
		np = malloc(sizeof(dict_entry));
		if (np == NULL || (np->key = strdup(key)) == NULL)
			return NULL;
		val = dicthash(key);
		np->next = hashtab[val];
		hashtab[val] = np;
	}
	else
		jsonfree(np->item);
	np->item = item;
	return np;
}

static void dictfree(dict* d)
{
	int i;
	for(i = 0; i < HASHSIZE && d->size; ++i)
		jsonfree(d->hashtab[i]);
	free(d->hashtab);
	free(d);
}

static cjson* jsonalloc(jsontype type, cjson* parent, void* init_value)
{
	cjson* json = malloc(sizeof(cjson));
	json->type = type;
	json->parent = parent;
	switch(json->type)
	{
		case JSONUNDEF:
			free(json);
			return NULL;
		case JSONOBJ:
			json->ptr = malloc(sizeof(dict));
			((dict*)(json->ptr))->size = 0;
			((dict*)(json->ptr))->hashtab = calloc(HASHSIZE, sizeof(dict_entry*));
			((dict*)(json->ptr))->parent = json;
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

static void jsonfree(cjson* json)
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
				jsonfree(array[i]);
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

static int _parse_hexa(const char *js, size_t *i, char** it)
{
	int n;
	char r;
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

static cjson* jsonparse_primitive(const char *js, const size_t len, cjson* json, size_t *i)
{
	switch(js[*i])
	{
		case 't':
			if(strncmp(js+(*i)*sizeof(char), "true", 4) == 0)
			{
				int* b = malloc(sizeof(int));
				*b = 1;
				*i+=3;
				return jsonalloc(JSONBOOL, json, b);
			}
			else
				return NULL;
			break;
		case 'f':
			if(strncmp(js+(*i)*sizeof(char), "false", 4) == 0)
			{
				int* b = malloc(sizeof(int));
				*b = 0;
				*i+=4;
				return jsonalloc(JSONBOOL, json, b);
			}
			else
				return NULL;
			break;
		case 'n':
			if(strncmp(js+(*i)*sizeof(char), "null", 4) == 0)
			{
				int* b = malloc(sizeof(int));
				*b = 0;
				*i+=3;
				return jsonalloc(JSONPRIM, json, b);
			}
			else
				return NULL;
			break;
		default:
			return NULL;
	}
	return NULL;
}

static cjson* jsonparse_obj(const char *js, const size_t len, cjson* json, int *i)
{
	cjson* obj = jsonalloc(JSONOBJ, json, NULL);
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
				{
					jsonfree(obj);
					return NULL;
				}
				return obj;
				break;
			case ',':
				if(state != 3)
				{
					jsonfree(obj);
					return NULL;
				}
				state = 0;
				break;
			case ':':
				if(state != 1)
				{
					jsonfree(obj);
					return NULL;
				}
				state = 2;
				break;
			case '{':
				if(state != 2)
				{
					jsonfree(obj);
					return NULL;
				}
				++(*i);
				item = jsonparse_obj(js, len, obj, i);
				if(item == NULL || dictinstall(d->hashtab, key, item) == NULL)
				{
					jsonfree(obj);
					return NULL;
				}
				state = 3;
				break;
			case '[':
				if(state != 2)
				{
					jsonfree(obj);
					return NULL;
				}
				++(*i);
				item = jsonparse_array(js, len, obj, i);
				if(item == NULL || dictinstall(d->hashtab, key, item) == NULL)
				{
					jsonfree(obj);
					return NULL;
				}
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
				{
					free(key);
					jsonfree(item);
					jsonfree(obj);
					return NULL;
				}
				item = jsonparse_value(js, len, obj, i);
				if(item == NULL || dictinstall(d->hashtab, key, item) == NULL)
				{
					free(key);
					jsonfree(item);
					jsonfree(obj);
					return NULL;
				}
				--(*i);
				key = NULL;
				item = NULL;
				state = 3;
				break;
			case 't':
			case 'f':
			case 'n':
				if(state != 2)
				{
					free(key);
					jsonfree(item);
					jsonfree(obj);
					return NULL;
				}
				item = jsonparse_primitive(js, len, obj, i);
				if(item == NULL || dictinstall(d->hashtab, key, item) == NULL)
				{
					free(key);
					jsonfree(item);
					jsonfree(obj);
					return NULL;
				}
				key = NULL;
				item = NULL;
				state = 3;
				break;
			case '\"':
				{
					if(state % 2 == 1)
					{
						free(key);
						jsonfree(obj);
						jsonfree(item);
						return NULL;
					}
					++(*i);
					char* tmp = _parse_string(js, len, i);
					if(tmp == NULL)
					{
						free(key);
						jsonfree(obj);
						jsonfree(item);
						return NULL;
					}
					if(state == 0)
					{
						key = tmp;
						state = 1;
					}
					else if(state == 2)
					{
						item = jsonalloc(JSONSTR, obj, tmp);
						if(item == NULL || dictinstall(d->hashtab, key, item) == NULL)
						{
							free(key);
							jsonfree(item);
							jsonfree(obj);
							return NULL;
						}
						key = NULL;
						item = NULL;
						state = 3;
					}
					break;
				}
			default:
				jsonfree(obj);
				return NULL;
		}
	}
	jsonfree(obj);
	return NULL;
}

static cjson* jsonparse_array(const char *js, const size_t len, cjson* json, size_t *i)
{
	cjson* list = jsonalloc(JSONLIST, json, NULL);
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
				{
					jsonfree(list);
					return NULL;
				}
				return list;
				break;
			case ',':
				if(comma != 1)
				{
					jsonfree(list);
					return NULL;
				}
				comma = 0;
				break;
			case '{':
				++(*i);
				((cjson**)(list->ptr))[list->size] = jsonparse_obj(js, len, list, i);
				++list->size;
				comma = 1;
				break;
			case '[':
				++(*i);
				((cjson**)(list->ptr))[list->size] = jsonparse_array(js, len, list, i);
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
				++list->size;
				comma = 1;
				--(*i);
				break;
			case 't':
			case 'f':
			case 'n':
				((cjson**)(list->ptr))[list->size] = jsonparse_primitive(js, len, list, i);
				if(((cjson**)(list->ptr))[list->size] == NULL)
				{
					jsonfree(list);
					return NULL;
				}
				++list->size;
				comma = 1;
				break;
			case '\"':
				++(*i);
				((cjson**)(list->ptr))[list->size] = jsonparse_string(js, len, list, i);
				if(((cjson**)(list->ptr))[list->size] == NULL)
				{
					jsonfree(list);
					return NULL;
				}
				++list->size;
				comma = 1;
				break;
			default:
				jsonfree(list);
				return NULL;
		}
	}
	jsonfree(list);
	return NULL;
}

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
			return jsonalloc(JSONFLOAT, json, f);
		}
		else
		{
			int64_t *d = malloc(sizeof(int64_t));
			*d = strtoull(str, NULL, 10);
			free(str);
			return jsonalloc(JSONINT, json, d);
		}
	}
}

static cjson* jsonparse_string(const char *js, const size_t len, cjson* json, size_t *i)
{
	char *str = _parse_string(js, len, i);
	if(str == NULL)
		return NULL;
	return jsonalloc(JSONSTR, json, str);
}


// manipulate
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
				if(root == NULL)
					root = current;
				if(current == NULL)
				{
					jsonfree(root);
					return NULL;
				}
				break;
			case '[':
				++i;
				current = jsonparse_array(js, len, current, &i);
				if(root == NULL)
					root = current;
				if(current == NULL)
				{
					jsonfree(root);
					return NULL;
				}
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
				{
					jsonfree(root);
					return NULL;
				}
				if(root == NULL)
					root = current;
				break;
			case 't':
			case 'f':
			case 'n':
				current = jsonparse_primitive(js, len, current, &i);
				if(current == NULL)
				{
					jsonfree(root);
					return NULL;
				}
				if(root == NULL)
					root = current;
				break;
			case '\"':
				++i;
				current = jsonparse_string(js, len, current, &i);
				if(current == NULL)
				{
					jsonfree(root);
					return NULL;
				}
				if(root == NULL)
					root = current;
				break;
			default:
				jsonfree(root);
				return NULL;
		}
	}
	return root;
}

static int64_t* jsonGetInt(cjson* json)
{
	if(json == NULL || json->type != JSONINT)
		return NULL;
	return json->ptr;
}

static int* jsonGetBool(cjson* json)
{
	if(json == NULL || json->type != JSONBOOL)
		return NULL;
	return json->ptr;
}

static float* jsonGetFloat(cjson* json)
{
	if(json == NULL || json->type != JSONFLOAT)
		return NULL;
	return json->ptr;
}

static const char* jsonGetStr(cjson* json)
{
	if(json == NULL || json->type != JSONSTR)
		return NULL;
	return json->ptr;
}

static cjson** jsonGetList(cjson* json)
{
	if(json == NULL || json->type != JSONLIST)
		return NULL;
	return json->ptr;
}

static dict* jsonGetObj(cjson* json)
{
	if(json == NULL || json->type != JSONOBJ)
		return NULL;
	return json->ptr;
}

static int jsonIsNull(cjson* json)
{
	if(json == NULL || json->type != JSONPRIM)
		return 0;
	return 1;
}

static size_t jsonListSize(cjson* json)
{
	if(json == NULL || json->type != JSONLIST)
		return 0;
	return json->size;
}

static int jsonListAppend(cjson* json, cjson* app)
{
	if(json == NULL || json->type != JSONLIST || json->size >= JSON_MAX)
		return -1;
	((cjson**)(json->ptr))[json->size] = app;
	json->size++;
	return 0;
}

static int jsonListSet(cjson* json, int index, cjson* add)
{
	if(json == NULL || json->type != JSONLIST || index >= json->size || index < 0)
		return -1;
	jsonfree(((cjson**)(json->ptr))[index]);
	((cjson**)(json->ptr))[index] = add;
	return 0;
}

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

static int jsonObjSet(cjson* json, char* key, cjson* set)
{
	if(json == NULL || json->type != JSONOBJ)
		return -1;
	dict *d = json->ptr;
	if(dictinstall(d->hashtab, key, set) == NULL)
		return -1;
	return 0;
}

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

	return json;
}

#endif
