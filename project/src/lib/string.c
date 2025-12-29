#include "string.h"


size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}


char* strcpy(char* to_here, const char* from_here) {
    char* dest = to_here;
    while (*from_here != '\0') {
        *to_here = *from_here;
        to_here++;
        from_here++;
    }
    *to_here = '\0'; 
    return dest;
}

char* strncpy(char* to_here, const char* from_here, size_t len) {
    char* dest = to_here;
    while (len > 0 && *from_here != '\0') {
        *to_here = *from_here;
        to_here++;
        from_here++;
        len--;
    }
    while (len > 0) {
        *to_here = '\0'; 
        to_here++;
        len--;
    }
    return dest;
}


int strcmp(const char* str1, const char* str2) {
    while (*str1 != '\0' && *str2 != '\0') {
        if (*str1 != *str2) {
            return (*str1 - *str2);
        }
        str1++;
        str2++;
    }
    return (*str1 - *str2);  
}

int strncmp(const char* str1, const char* str2, size_t len) {
    while (len > 0 && *str1 != '\0' && *str2 != '\0') {
        if (*str1 != *str2) {
            return (*str1 - *str2);
        }
        str1++;
        str2++;
        len--;
    }
    if (len > 0) {
        return (*str1 - *str2);
    }
    return 0;  
}


void strcat(char* dest, const char* src) {
    while (*dest != '\0') {
        dest++;  
    }
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';  
}

char* strchr(const char* s, int c) {
    while (*s != '\0') {
        if (*s == c) {
            return (char*)s;
        }
        s++;
    }
    return NULL;
}


char* strrchr(const char* s, int c) {
    const char* last = NULL;
    while (*s != '\0') {
        if (*s == c) {
            last = s; 
        }
        s++;
    }
    return (char*)last; 
}


void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) {
        *d = *s;
        d++;
        s++;
    }
    return dest;
}


void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    if (d < s) {
        while (n--) {
            *d = *s;
            d++;
            s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *(--d) = *(--s);
        }
    }
    return dest;
}


void* memset(char* dest, char value, size_t size) {
    unsigned char* d = (unsigned char*)dest;
    while (size--) {
        *d = (unsigned char)value;
        d++;
    }
    return dest;
}


int memcmp(const char* s1, const char* s2, size_t n) {
    while (n--) {
        if (*s1 != *s2) {
            return (*s1 - *s2);
        }
        s1++;
        s2++;
    }
    return 0;
}


void itoa (char *buf, int base, int d)
{
  char *p = buf;
  char *p1, *p2;
  unsigned long ud = d;
  int divisor = 10;

  if (base == 'd' && d < 0)
    {
      *p++ = '-';
      buf++;
      ud = -d;
    }
  else if (base == 'x')
    divisor = 16;


  do
    {
      int remainder = ud % divisor;
      
      *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    }
  while (ud /= divisor);


  *p = 0;
  

  p1 = buf;
  p2 = p - 1;
  while (p1 < p2)
    {
      char tmp = *p1;
      *p1 = *p2;
      *p2 = tmp;
      p1++;
      p2--;
    }
}









int vsnprintf(char *str, size_t size, const char *format, va_list args) {
    char *out = str, *end = str + size - 1;
    const char *p = format;
    char buf[32];

    while (*p) {
        if (*p != '%') {
            if (out < end) *out++ = *p++;
            else break;
        } else {
            p++;
            switch (*p++) {
            case 'd':
            case 'x':
            case 'u':{
                int value = va_arg(args, int);
                itoa(buf, *p, value);
                const char *tmp = buf;
                while (*tmp && out < end) *out++ = *tmp++;
                break;
            }
            case 's': {
                const char *value = va_arg(args, const char *);
                if (!value) value = "(null)";
                while (*value && out < end) *out++ = *value++;
                break;
            }
            default:
                if (out < end) *out++ = '%';
                if (out < end) *out++ = *(p - 1);
                break;
            }
        }
    }
    *out = '\0';
    return (out - str);
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}