#include <uuid/uuid.h>
#include <stdio.h>

#include "./id.h"

char *genUuid(char *uuid_str) {
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse_lower(uuid, uuid_str);
  return uuid_str;
}

int hash(const char *prefix, char *hash_str, char *str, size_t strlen) {
  int hash = 5381;
  size_t i = strlen;
  while (i) {
    hash = (hash * 33) ^ (int)(str[--i]);
  }
  return sprintf(hash_str, "%s%x", prefix, (unsigned int)hash >> 0);
}


int SelvaId_GenId(const char *prefix, char *hash_str) {
  char uuid_str[37];
  genUuid(uuid_str);
  hash(prefix, hash_str, uuid_str, 37);
  return 0;
}
