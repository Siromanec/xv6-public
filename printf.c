#include "types.h"
#include "stat.h"
#include "user.h"
#define INT_ASCII_START 48
#define isdigit(d) ((d) >= '0' && (d) <= '9')
#define toInt(c)   ((int) c - INT_ASCII_START)
static void
putc(int fd, char c) {
    write(fd, &c, 1);
}

static void
printint(int fd, int xx, int base, int sgn, int pad, char pad_char) {
    static char digits[] = "0123456789ABCDEF";
    char buf[16];
    int i, neg;
    uint x;

    neg = 0;
    if (sgn && xx < 0) {
        neg = 1;
        x = -xx;
    } else {
        x = xx;
    }

    i = 0;
    do {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);
    if (neg)
        buf[i++] = '-';
    if(pad > i){
        for (int j = 0; j < pad-i; ++j) {
            putc(fd, pad_char);
        }
    }
    while (--i >= 0)
        putc(fd, buf[i]);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
void
printf(int fd, const char *fmt, ...) {
    char *s;
    int c, i, state;
    uint *ap;

    state = 0;
    int pad = 0;
    int readding_padding = 0;
    char pad_char = ' ';
    ap = (uint *) (void *) &fmt + 1;
    for (i = 0; fmt[i]; i++) {
        c = fmt[i] & 0xff;
        if (state == 0) {
            if (c == '%') {
                state = '%';
            } else {
                putc(fd, c);
            }
        } else if (state == '%') {
            if (c == '0' && !readding_padding) {
                pad_char = c;
                readding_padding = 1;
                continue;
                // wait until
            }
            if(isdigit(c)) {
                readding_padding = 0;
                pad = toInt(c);
                continue;
            }
            if (c == 'd') {
                printint(fd, *ap, 10, 1, pad, pad_char);
                ap++;

            }
            else if (c == 'u') {
                printint(fd, *ap, 10, 0, pad, pad_char);
                ap++;
            } else if (c == 'x' || c == 'p') {
                printint(fd, *ap, 16, 0, pad, pad_char);
                ap++;
            } else if (c == 's') {
                s = (char *) *ap;
                if(pad!=0){
                    int charsToAdd = pad;
                    for (int j = 0; j < charsToAdd; ++j) {
                        putc(fd, pad_char);
                    }
                }
                ap++;

                if (s == 0)
                    s = "(null)";

                while (*s != 0) {
                    putc(fd, *s);
                    s++;
                }
            } else if (c == 'c') {
                putc(fd, *ap);
                ap++;
            } else if (c == '%') {
                putc(fd, c);
            } else {
                // Unknown % sequence.  Print it to draw attention.
                putc(fd, '%');
                putc(fd, c);
            }
            if(!readding_padding){
                pad = 0;
                pad_char=' ';
            }
            state = 0;
        }
    }
}
