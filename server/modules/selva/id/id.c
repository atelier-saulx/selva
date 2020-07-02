#include <uuid/uuid.h>
#include <stdio.h>

#include "./id.h"

static char *genUuid(char *uuid_str) {
  uuid_t uuid;

  uuid_generate(uuid);
  uuid_unparse_lower(uuid, uuid_str);

  return uuid_str;
}

static int hash(const char *prefix, char *hash_str, char *str, size_t slen) {
  int hash = 5381;
  size_t i = slen;

  while (i) {
    hash = (hash * 33) ^ (int)(str[--i]);
  }

  return sprintf(hash_str, "%s%08x", prefix, (unsigned int)hash >> 0);
}


int SelvaId_GenId(const char *prefix, char *hash_str) {
  char uuid_str[37];

  genUuid(uuid_str);
  hash(prefix, hash_str, uuid_str, sizeof(uuid_str));

  return 0;
}
