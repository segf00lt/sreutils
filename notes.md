# item 1: srefread

We need a custom function to execute a structural regular expression
whilst reading a file.

May look something like this:

```
char* srefread(char *buf, Reprog *prog, FILE *fp);
```

srefread takes a file and reads an unknown number of bytes
(usually a fixed default value though) into buf, and returns buf.
The number of bytes is unkown because the _shape_ of the buffer
is defined by a regular expression (compiled to an Reprog * and
passed to srefread).

srefread executes prog, reading bytes one by one from fp. As soon
as a match is found, it returns buf. If buf is filled before a match
is found, buf is realloc'd (this is why srefread must return buf) and
the execution continues.
