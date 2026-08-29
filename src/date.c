/*
 * Civil date arithmetic on a (year, month, day) triple.
 *
 * There is no epoch anywhere in this program. NetworkProtocolCalendar resolves
 * every timestamp to local wall clock before it reaches us, so the only date
 * maths the client does is moving its own view anchor around -- and that is
 * exactly what these do.
 *
 * Pure: no platform, no network. tests/hosttest.c exercises all of it.
 */

#include "gcal.h"

static const unsigned char dim[13] = {
    0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const char dow3[7][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char mon3[13][4] = {
    "???", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* The full Gregorian rule, so 2100 comes out correctly as not a leap year. */
unsigned char date_leap(unsigned int y)
{
    if (y % 4)
        return 0;
    if (y % 100)
        return 1;
    return (y % 400) ? 0 : 1;
}

unsigned char date_dim(unsigned int y, unsigned char mo)
{
    if (mo < 1 || mo > 12)
        return 30;
    if (mo == 2)
        return (unsigned char) (28 + date_leap(y));
    return dim[mo];
}

void date_addday(unsigned int *y, unsigned char *mo, unsigned char *d)
{
    if (*d < date_dim(*y, *mo)) {
        (*d)++;
        return;
    }
    *d = 1;
    if (*mo < 12) {
        (*mo)++;
        return;
    }
    *mo = 1;
    (*y)++;
}

void date_subday(unsigned int *y, unsigned char *mo, unsigned char *d)
{
    if (*d > 1) {
        (*d)--;
        return;
    }
    if (*mo > 1) {
        (*mo)--;
    } else {
        *mo = 12;
        (*y)--;
    }
    *d = date_dim(*y, *mo);
}

/*
 * Month steps clamp the day to the new month's length. Letting Jan 31 become
 * Feb 31 would build a device spec the adapter rejects with
 * INVALID_DEVICESPEC, which reads to the user as a network failure.
 */
static void clamp_day(unsigned int y, unsigned char mo, unsigned char *d)
{
    unsigned char n = date_dim(y, mo);

    if (*d > n)
        *d = n;
}

void date_addmonth(unsigned int *y, unsigned char *mo, unsigned char *d)
{
    if (*mo < 12) {
        (*mo)++;
    } else {
        *mo = 1;
        (*y)++;
    }
    clamp_day(*y, *mo, d);
}

void date_submonth(unsigned int *y, unsigned char *mo, unsigned char *d)
{
    if (*mo > 1) {
        (*mo)--;
    } else {
        *mo = 12;
        (*y)--;
    }
    clamp_day(*y, *mo, d);
}

/* Sakamoto's method. 0 = Sunday. */
unsigned char date_dow(unsigned int y, unsigned char mo, unsigned char d)
{
    static const unsigned char t[13] = {
        0, 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4
    };
    unsigned int yy = y;

    if (mo < 1 || mo > 12)
        return 0;
    if (mo < 3)
        yy--;

    return (unsigned char)
        ((yy + yy / 4 - yy / 100 + yy / 400 + t[mo] + d) % 7);
}

/* YYYY-MM-DD, zero padded, into an 11-byte buffer. */
void date_iso(char *dst, unsigned int y, unsigned char mo, unsigned char d)
{
    dst[0] = (char) ('0' + (y / 1000) % 10);
    dst[1] = (char) ('0' + (y / 100) % 10);
    dst[2] = (char) ('0' + (y / 10) % 10);
    dst[3] = (char) ('0' + y % 10);
    dst[4] = '-';
    dst[5] = (char) ('0' + mo / 10);
    dst[6] = (char) ('0' + mo % 10);
    dst[7] = '-';
    dst[8] = (char) ('0' + d / 10);
    dst[9] = (char) ('0' + d % 10);
    dst[10] = '\0';
}

const char *date_dow3(unsigned char dow)
{
    return dow3[dow % 7];
}

const char *date_mon3(unsigned char mo)
{
    return mon3[(mo < 1 || mo > 12) ? 0 : mo];
}
