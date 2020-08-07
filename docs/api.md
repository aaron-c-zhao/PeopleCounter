@page api api documentation
# api
In this document the api is described, with what is handled the module and what will have to be handled by the firmware.

## input
- frame
  - null pointer of struct ip_mat
``` 
typedef struct mat
  {
    uint8_t *data;
  } ip_mat;
```
- output null array pointer of ip_object.
- output null pointer of ip_result

## Output
