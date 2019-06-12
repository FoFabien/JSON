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
cjson *json = jsonRead(your_file);
```  
or  
```c
cjson *json = jsonParse(your_json_string, strlen(your_json_string));
```  
Don't forget to free the memory once you are done:  
```c
jsonfree(json);
```  
  
* Writing:  
In a similar fashion, pass your json pointer to `jsonWrite()`:  
```c
if(jsonWrite(your_file_path, json) != 0) printf("failed to save\n");
```  
  
* Editing:  
A few functions are available to access the pointers or edit the list or objects in a simple way.  
There is a few limitations currently.  