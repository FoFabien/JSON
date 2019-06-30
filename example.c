#include "json.h"
#include <stdlib.h>
#include <stdio.h>

// test json
static const char *TEST_JSON =
    "{"
    "\"string\" : \"hello world!\","
    "\"exp\" : 0.13674E+3,"
    "\"unicode\" : \"test: \\ud83c\","
    "\"pi\" : 3.14159265359,"
    "\"number\" : -56340,"
    "\"array\" : ["
    "\"first element\", 2, 3.3, \"4th\""
    "],"
    "\"boolean\" : true"
    "}";

// print the content of the json object, recursively
void print(hjson* json, int indent)
{
    int i;
    for(i = 0; i < indent; ++i)
        printf("\t");
    if(json != NULL)
    {
        switch(json->type)
        {
            case JSONOBJ:
                {
                    jdict* d = jsonGetObj(json);
                    jdict_entry* elem;
                    printf("{\n");
                    size_t j;
                    for(j = 0; j < HASHSIZE; ++j)
                    {
                        for(elem = d->hashtab[j]; elem != NULL; elem = elem->next)
                        {
                            for(i = 0; i < indent; ++i)
                                printf("\t");

                            printf("\"%s\" :\n", elem->key);
                            print(elem->item, indent+1);
                        }
                    }
                    for(i = 0; i < indent; ++i)
                        printf("\t");
                    printf("}\n");
                    break;
                }
            case JSONLIST:
                {
                    jlist* l = jsonGetList(json);
                    size_t j;
                    printf("[\n");
                    for (j = 0; j < l->size; ++j)
                        print(l->list[j], indent);
                    for(i = 0; i < indent; ++i)
                        printf("\t");
                    printf("]\n");
                    break;
                }
            case JSONSTR:
                printf("\"%s\"\n", jsonGetStr(json));
                break;
            case JSONPRIM:
                printf("null\n");
                break;
            case JSONFLOAT:
                {
                    double *d = jsonGetFloat(json);
                    printf("%f\n", *d);
                    break;
                }
            case JSONINT:
                {
                    int64_t *l = jsonGetInt(json);
                    printf("%lld\n", *l);
                    break;
                }
            case JSONBOOL:
                {
                    char *b = jsonGetBool(json);
                    if(*b != 0)
                        printf("true\n");
                    else
                        printf("false\n");
                    break;
                }
            default:
                printf("error\n");
                break;
        }
    }
    else
        printf("error null pointer\n");
}

int main()
{
    // create the json object from the string above
    hjson *json = jsonParse(TEST_JSON, strlen(TEST_JSON));
    // add a string value to our object
    hjson *tmp;
    if(jsonObjSet(json, "example", tmp = jsonAlloc(JSONSTR, "this is an example\nhello world!!")) != 0) // !! the example string should be a separate pointer, as jsonFree will free it
    {
        printf("failed to modify\n");
        jsonFree(tmp);
    }
    // print the content
    print(json, 0);
    // write to file
    if(jsonWrite("out.json", json) != 0)
        printf("save failed\n");
    // free the memory
    jsonFree(json);
    return EXIT_SUCCESS;
}
