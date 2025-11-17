//------------------------------------------------------------------------------
/**
    teensy_firmware.ino

    Uses the USB-HID protocol to receive/send data from/to client.

    You must select Raw HID from the "Tools > USB Type" menu.

    (C) 2025 Christian Bleicher
*/

//------------------------------------------------------------------------------
#include <stdint.h>

typedef struct decimal_t
{
    unsigned char integer_places;
    unsigned char decimal_places;
    int32_t significand;
} decimal_t;

#define MAX_NUM_DIGITS_BASE10 10U
#define MAX_STR_LENGTH_BASE10 12U

//------------------------------------------------------------------------------
/**
*/
decimal_t
decimal_add(decimal_t a, decimal_t b)
{
    decimal_t r = {0};

    unsigned int i, d; int32_t scale = 1;
    if (a.decimal_places > b.decimal_places)
    {
        r.decimal_places = a.decimal_places;
        d = a.decimal_places - b.decimal_places;
        for (i = 0U; i < d; ++i) scale *= 10;
        r.significand = b.significand * scale + a.significand;
    }
    else
    {
        r.decimal_places = b.decimal_places;
        d = b.decimal_places - a.decimal_places;
        for (i = 0U; i < d; ++i) scale *= 10;
        r.significand = a.significand * scale + b.significand;
    }

    int32_t x = r.significand;
    unsigned char precision = 1U;
    while (x /= 10) ++precision;
    r.integer_places = precision > r.decimal_places ? precision - r.decimal_places : 0U;

    return r;
}

//------------------------------------------------------------------------------
/**
*/
decimal_t
decimal_subtract(decimal_t a, decimal_t b)
{
    decimal_t s = b;
    s.significand *= -1;
    return decimal_add(a, s);
}

//------------------------------------------------------------------------------
/**
*/
decimal_t
decimal_multiply(decimal_t a, decimal_t b)
{
    decimal_t r = {0};

    if ((a.significand == 0) || (b.significand == 0)) return r;

    int64_t significand = (int64_t)a.significand * (int64_t)b.significand;
    r.decimal_places = a.decimal_places + b.decimal_places;

    int64_t x = significand;
    unsigned char precision = 1U;
    while (x /= 10) ++precision;
    r.integer_places = precision > r.decimal_places ? precision - r.decimal_places : 0U;

    if (precision >= 9U)
    {
        unsigned char decimal_places = 9U - r.integer_places;
        int64_t f = 1;
        unsigned int i;
        for (i = 0U; i < (unsigned char)(r.decimal_places - decimal_places); ++i) f *= 10;
        r.significand = (int32_t)(significand / f);
        r.decimal_places = decimal_places;
    }
    else
    {
        r.significand = (int32_t)significand;
    }

    return r;
}

//------------------------------------------------------------------------------
/**
*/
decimal_t
decimal_divide(decimal_t dividend, decimal_t divisor)
{
    if (dividend.significand == 0 || divisor.significand == 0) {decimal_t r = {0}; return r;}

    decimal_t N = {dividend.integer_places, dividend.decimal_places, dividend.significand < 0 ? -dividend.significand : dividend.significand};
    decimal_t D = {divisor.integer_places, divisor.decimal_places, divisor.significand < 0 ? -divisor.significand : divisor.significand};

    decimal_t F = {0, divisor.integer_places, 1};

    decimal_t two = {1U, 0U, 2};

    unsigned int i;
    for (i = 0U; i < 8U; ++i)
    {
        N = decimal_multiply(F, N);
        D = decimal_multiply(F, D);
        F = decimal_subtract(two, D);
    }

    if ((dividend.significand < 0 && divisor.significand > 0) || ((dividend.significand >= 0 && divisor.significand < 0))) N.significand *= -1;

    return N;
}

//------------------------------------------------------------------------------
enum operation
{
    op_add = 0,
    op_subtract,
    op_multiply,
    op_divide
};

#define BUFFER_SIZE 64U

byte buffer[BUFFER_SIZE] = {0};

//------------------------------------------------------------------------------
/**
*/
void
setup()
{

}

//------------------------------------------------------------------------------
/**
*/
void
loop()
{
    int n = RawHID.recv(buffer, 0U); // 0 timeout = do not wait
    if (n <= 0) return;

    decimal_t a[4U] = {0}, b[4U] = {0}, r[4U] = {0};
    a[0U].integer_places = buffer[1U]; a[0U].decimal_places = buffer[2U]; memcpy(&a[0U].significand, &buffer[3U], sizeof(int32_t));
    b[0U].integer_places = buffer[7U]; b[0U].decimal_places = buffer[8U]; memcpy(&b[0U].significand, &buffer[9U], sizeof(int32_t));
    a[1U].integer_places = buffer[13U]; a[1U].decimal_places = buffer[14U]; memcpy(&a[1U].significand, &buffer[15U], sizeof(int32_t));
    b[1U].integer_places = buffer[19U]; b[1U].decimal_places = buffer[20U]; memcpy(&b[1U].significand, &buffer[21U], sizeof(int32_t));
    a[2U].integer_places = buffer[25U]; a[2U].decimal_places = buffer[26U]; memcpy(&a[2U].significand, &buffer[27U], sizeof(int32_t));
    b[2U].integer_places = buffer[31U]; b[2U].decimal_places = buffer[32U]; memcpy(&b[2U].significand, &buffer[33U], sizeof(int32_t));
    a[3U].integer_places = buffer[37U]; a[3U].decimal_places = buffer[38U]; memcpy(&a[3U].significand, &buffer[39U], sizeof(int32_t));
    b[3U].integer_places = buffer[43U]; b[3U].decimal_places = buffer[44U]; memcpy(&b[3U].significand, &buffer[45U], sizeof(int32_t));

    unsigned int i;
    switch (buffer[0U])
    {
        case op_add:
            for (i = 0U; i < 4U; ++i) r[i] = decimal_add(a[i], b[i]);
            break;
        case op_subtract:
            for (i = 0U; i < 4U; ++i) r[i] = decimal_subtract(a[i], b[i]);
            break;
        case op_multiply:
            for (i = 0U; i < 4U; ++i) r[i] = decimal_multiply(a[i], b[i]);
            break;
        case op_divide:
            for (i = 0U; i < 4U; ++i) r[i] = decimal_divide(a[i], b[i]);
            break;
        default:
            break;
    }

    buffer[0U] = r[0U].integer_places; buffer[1U] = r[0U].decimal_places; memcpy(&buffer[2U], &r[0U].significand, sizeof(int32_t));
    buffer[6U] = r[1U].integer_places; buffer[7U] = r[1U].decimal_places; memcpy(&buffer[8U], &r[1U].significand, sizeof(int32_t));
    buffer[12U] = r[2U].integer_places; buffer[13U] = r[2U].decimal_places; memcpy(&buffer[14U], &r[2U].significand, sizeof(int32_t));
    buffer[18U] = r[3U].integer_places; buffer[19U] = r[3U].decimal_places; memcpy(&buffer[20U], &r[3U].significand, sizeof(int32_t));

    RawHID.send(buffer, 100U);
}
