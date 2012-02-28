#include "other.h"

#include <util/string/util.h>
#include <util/system/yassert.h>

/********************************************************/
/********************************************************/

static const Tr InvertTr(".:/?#", "\005\004\003\002\001");
static const Tr RevertTr("\005\004\003\002\001", ".:/?#");

void TrspChars(char *s) {
    InvertTr.Do(s);
}

void UnTrspChars(char *s) {
    RevertTr.Do(s);
}

void TrspChars(char *s, size_t l) {
    InvertTr.Do(s, l);
}

void UnTrspChars(char *s, size_t l) {
    RevertTr.Do(s, l);
}

void TrspChars(const char *s, char *d) {
    InvertTr.Do(s, d);
}

void UnTrspChars(const char *s, char *d) {
    RevertTr.Do(s, d);
}

void InvertDomain(char* begin, char* end) {
    // skip schema if it is present
    char* dot = (char*)memchr(begin, '.', end - begin);
    if (!dot)
        return; // no need to invert anything
    char* schemaend = (char*)TCharTraits<char>::Find(begin, end - begin, "://", 3);
    if (schemaend && schemaend < dot)
        begin = schemaend + 3;
    char* sl = (char*)memchr(begin, '/', end - begin);
    char* cl = (char*)memchr(begin, ':', sl ? sl - begin : end - begin);
    end = cl ? cl : (sl ? sl : end);

    // invert string
    for (size_t i = 0, n = end - begin; i < n / 2; ++i)
        DoSwap(begin[i], begin[n - i - 1]);

    // invert back each host name segment
    char* b = begin;
    while (true) {
        char* e = (char*)memchr(b, '.', end - b);
        if (!e)
            e = end;
        for (size_t i = 0, n = e - b; i < n / 2; ++i)
            DoSwap(b[i], b[n - i - 1]);
        if (e == end)
            break;
        b = e + 1;
    }
}