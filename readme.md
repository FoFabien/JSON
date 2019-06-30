# JSON for C  
* C Header Only JSON implementation.  
  
# Notes  
* **Work in progress**, everything is subject to change.  
* Still lacking support for some features  
  
# Usage  
Simply `#include "json.h"`.  
  
* Reading:  
You can do either:
```c
hjson *json = jsonRead(your_file);
```  
or  
```c
hjson *json = jsonParse(your_json_string, strlen(your_json_string));
```  
Don't forget to free the memory once you are done:  
```c
jsonFree(json);
```  
  
* Writing:  
In a similar fashion, pass your json pointer to `jsonWrite()`:  
```c
if(jsonWrite(your_file_path, json) != 0) printf("failed to save\n");
```  
  
* Editing:  
A few functions are available to access the pointers or to manipulate the json list or objects in a simple way.  
Keep in mind the `hjson` structure represents a json value. For example, a `hjson` variable with the type `JSONLIST` will refers to a `jlist` structure which is pretty much an array of `hjson` pointers (in short: everything are `hjson`, the values contained in your list and the list itself).  
  
Here's a quick rundown:  
`jsonAlloc` - create a json object from a type and a pointer to the corresponding value.  
`jsonFree` - destroy the json object and free the memory from all values.  
`jsonGet<Type>` - multiple functions to access the pointer of the json object. If the type doesn't match (example: `jsonGetInt` on a `JSONSTR` value), a `null` pointer is returned.  
`jsonIsNull` - only for `null` values. Return `1` if `null`, `0` if it's an error.  
`jsonListSize` - returns the size of a list value or `0` if it's not a list.  
`jsonListAppend` - append a value to the list, returns `-1` if it's not a list.  
`jsonListSet` - set the value at the specified position in a list, returns `-1` if it's not a list or if the position is incorrect.  
`jsonObjGet` - retrieve a pointer the value at the specified key in an object, returns `null` if it's not an object or if the key is incorrect.  
`jsonObjSet` - set the value with the specified key in an object, returns `null` if it's not an object or if an error happens.  
`jsonObjDel` - delete the value with the specified key in an object, returns `-1` if it's not an object.  