/*
 *  String helper functions
 *  Copyright (C) 2008 Andreas Öman
 *  Copyright (C) 2008 Mattias Wadman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tvh_string.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

char *
hts_strndup(const char *src, size_t len)
{
  char *r = malloc(len + 1);
  r[len] = 0;
  return memcpy(r, src, len);
}

char *
htsstr_unescape(char *str) {
  char *s;

  for(s = str; *s; s++) {
    if(*s != '\\')
      continue;

    if(*(s + 1) == 'b')
      *s = '\b';
    else if(*(s + 1) == 'f')
      *s = '\f';
    else if(*(s + 1) == 'n')
      *s = '\n';
    else if(*(s + 1) == 'r')
      *s = '\r';
    else if(*(s + 1) == 't')
      *s = '\t';
    else
      *s = *(s + 1);

    if(*(s + 1)) {
      /* shift string left, copies terminator too */
      memmove(s + 1, s + 2, strlen(s + 2) + 1);
    }
  } 

  return str;
}

char *
htsstr_unescape_to(const char *src, char *dst, size_t dstlen)
{
  char *res = dst;

  while (*src && dstlen > 0) {
    if (*src == '\\') {
      if (dstlen < 2)
        break;
      src++;
      if (*src) {
        if (*src == 'b')
          *dst = '\b';
        else if (*src == 'f')
          *dst = '\f';
        else if (*src == 'n')
          *dst = '\n';
        else if (*src == 'r')
          *dst = '\r';
        else if (*src == 't')
          *dst = '\t';
        else
          *dst = *src;
        src++; dst++; dstlen--;
      }
      continue;
    } else {
      *dst = *src; src++; dst++; dstlen--;
    }
  }
  if (dstlen == 0)
    *(dst - 1) = '\0';
  else if (dstlen > 0)
    *dst = '\0';
  return res;
}

const char *
htsstr_escape_find(const char *src, size_t upto_index)
{
  while (upto_index && *src) {
    if (*src == '\\') {
      src++;
      upto_index--;
      if (*src == '\0' || upto_index == 0) {
        src--;
        break;
      }
    }
    src++;
    upto_index--;
  }
  if (*src)
    return src;
  return NULL;
}

static void
htsstr_argsplit_add
  (char ***argv, int *argc, const char *start, const char *stop)
{
  char *s = NULL;
  if (start)
    s = htsstr_unescape(hts_strndup(start, stop - start));
  *argv = realloc(*argv, sizeof((*argv)[0]) * (*argc + 1));
  (*argv)[(*argc)++] = s;
}

char **
htsstr_argsplit(const char *str) {
  int quote = 0;
  int quote_inbetween = 0; // when quote isn't start of argument
  int inarg = 0;
  const char *start = NULL;
  const char *s;
  char **argv = NULL;
  int argc = 0;

  for(s = str; *s; s++) {
    if(inarg) {
      switch(*s) {
        case '\\':
          s++;
          break;
        case '"':
          if(quote) {
            quote = 0;
            if (start) {
              htsstr_argsplit_add(&argv, &argc, start, s);
              start = NULL;
            }
          } else if (quote_inbetween) {
            quote_inbetween = 0;
          } else {
            quote_inbetween = 1;
          }
          break;
        case ' ':
          if(quote||quote_inbetween)
            break;
          inarg = 0;
          if (start) {
            htsstr_argsplit_add(&argv, &argc, start, s);
            start = NULL;
          }
          break;
        default:
          break;
      }
    } else {
      switch(*s) {
        case ' ':
          break;
        case '"':
          inarg = 1;
          quote = 1;
          start = s + 1;
          break;
        default:
          inarg = 1;
          start = s;
          break;
      }
    }
  }

  if(start)
    htsstr_argsplit_add(&argv, &argc, start, str + strlen(str));

  htsstr_argsplit_add(&argv, &argc, NULL, NULL);

  return argv;
}

void 
htsstr_argsplit_free(char **argv) {
  int i;

  for(i = 0; argv[i]; i++)
    free(argv[i]);
  
  free(argv);
}

const char *
htsstr_substitute_find(const char *src, int first)
{
  while (*src) {
    if (*src == '\\') {
      src++;
      if (*src == '\0')
        break;
    } else if (*src == first)
      return src;
    src++;
  }
  return NULL;
}

char *
htsstr_substitute(const char *src, char *dst, size_t dstlen,
                  int first, htsstr_substitute_t *sub, const void *aux,
                  char *tmp, size_t tmplen)
{
  htsstr_substitute_t *s;
  const char *p, *x, *v;
  char *res = dst;
  size_t l;

  if (!dstlen)
    return NULL;
  while (*src && dstlen > 0) {
    if (*src == '\\') {
      if (dstlen < 2)
        break;
      *dst = '\\'; src++; dst++; dstlen--;
      if (*src) {
        *dst = *src; src++; dst++; dstlen--;
      }
      continue;
    }
    if (first >= 0) {
      if (*src != first) {
        *dst = *src; src++; dst++; dstlen--;
        continue;
      }
      src++;
    }
    for (s = sub; s->id; s++) {
      for (p = s->id, x = src; *p; p++, x++) {
        if (*p == '?') {
          while (*x >= '0' && *x <= '9')
            x++;
          x--;
          continue;
        }
        if (*p != *x)
          break;
      }
      if (*p == '\0') {
        if ((l = dstlen) > 0) {
          v = s->getval(s->id, src, aux, tmp, tmplen);
          strncpy(dst, v, l);
          l = MIN(strlen(v), l);
          dst += l;
          dstlen -= l;
        }
        src = x;
        break;
      }
    }
    if (!s->id) {
      if (first >= 0) {
        *dst = first;
      } else {
        *dst = *src;
        src++;
      }
      dst++; dstlen--;
    }
  }
  if (dstlen == 0)
    *(dst - 1) = '\0';
  else if (dstlen > 0)
    *dst = '\0';
  return res;
}

#if 0
/*
 * gcc -g -I ../build.linux/ -o test htsstr.c
 */
void main(int argc, char *argv[])
{
  char *strings[] = {
    "sh -c '/bin/df -P -h /recordings >/config/.markers/recording-pre-process'",
    "sh -c \"/bin/df -P -h /recordings >/config/.markers/recording-pre-process\"",
    "bash -c '/bin/df -P -h /recordings >/config/.markers/recording-pre-process'",
    "bash -c \"/bin/df -P -h /recordings | tee /config/.markers/recording-pre-process\"",
    "/bin/grep --label=\"TVHeadend Recording\" \"start time\"",
    "/bin/grep --label=\"TVHeadend Recordings \"File \"start time\" /recordings",
    NULL,
  };
  char **s = strings, **x;
  while (*s) {
    printf("Test for >>>%s<<<\n", *s);
    x = htsstr_argsplit(*s);
    while (*x) {
      printf("  >%s<\n", *x);
      x++;
    }
    s++;
  }
}
#endif
