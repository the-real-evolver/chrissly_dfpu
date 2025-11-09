//------------------------------------------------------------------------------
/**
    teensy_firmware.ino

    Uses the USB-HID protocol to recieve/send data from/to client.

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
    int n = RawHID.recv(buffer, 0); // 0 timeout = do not wait
    if (n > 0)
    {
        decimal_t a[4U] = {0}, b[4U] = {0}, r[4U] = {0};
        a[0].integer_places = buffer[1]; a[0].decimal_places = buffer[2]; memcpy(&a[0].significand, &buffer[3], sizeof(int32_t));
        b[0].integer_places = buffer[7]; b[0].decimal_places = buffer[8]; memcpy(&b[0].significand, &buffer[9], sizeof(int32_t));
        a[1].integer_places = buffer[13]; a[1].decimal_places = buffer[14]; memcpy(&a[1].significand, &buffer[15], sizeof(int32_t));
        b[1].integer_places = buffer[19]; b[1].decimal_places = buffer[20]; memcpy(&b[1].significand, &buffer[21], sizeof(int32_t));
        a[2].integer_places = buffer[25]; a[2].decimal_places = buffer[26]; memcpy(&a[2].significand, &buffer[27], sizeof(int32_t));
        b[2].integer_places = buffer[31]; b[2].decimal_places = buffer[32]; memcpy(&b[2].significand, &buffer[33], sizeof(int32_t));
        a[3].integer_places = buffer[37]; a[3].decimal_places = buffer[38]; memcpy(&a[3].significand, &buffer[39], sizeof(int32_t));
        b[3].integer_places = buffer[43]; b[3].decimal_places = buffer[44]; memcpy(&b[3].significand, &buffer[45], sizeof(int32_t));

        unsigned int i;
        switch (buffer[0])
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

        buffer[0] = r[0].integer_places; buffer[1] = r[0].decimal_places; memcpy(&buffer[2], &r[0].significand, sizeof(int32_t));
        buffer[6] = r[1].integer_places; buffer[7] = r[1].decimal_places; memcpy(&buffer[8], &r[1].significand, sizeof(int32_t));
        buffer[12] = r[2].integer_places; buffer[13] = r[2].decimal_places; memcpy(&buffer[14], &r[2].significand, sizeof(int32_t));
        buffer[18] = r[3].integer_places; buffer[19] = r[3].decimal_places; memcpy(&buffer[20], &r[3].significand, sizeof(int32_t));

        n = RawHID.send(buffer, 100);
    }
}
