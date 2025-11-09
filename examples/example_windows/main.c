//------------------------------------------------------------------------------
//  main.c
//  (C) 2025 Christian Bleicher
//------------------------------------------------------------------------------
#define CHRISSLY_DFPU_WINDOWS
#define CHRISSLY_DFPU_IMPLEMENTATION
#include "chrissly_dfpu.h"

//------------------------------------------------------------------------------
/**
*/
static void
print_number(decimal_t n)
{
    char number_string[14U] = {'\0'};
    decimal_to_string(n, number_string, sizeof(number_string));
    printf("value: %-12s format: %-2d.%-2d significand: %-12d isnan: %s\n", number_string,
        n.integer_places, n.decimal_places, n.significand, decimal_isnan(n) == 1 ? "true" : "false");
}

//------------------------------------------------------------------------------
/**
*/
int
main(void)
{
    dfpu_init();

    while (true)
    {
        if (_kbhit())
        {
            if (_getch() < 32) break;

            decimal_t vec_a[4U] = {{3, 2, 12345}, {3, 2, 12345}, {3, 2, 12345}, {3, 2, 12345}};
            decimal_t vec_b[4U] = {{1, 3, 6789}, {1, 3, 6789}, {1, 3, 6789}, {1, 3, 6789}};
            decimal_t vec_r[4U] = {0};

            dfpu_add_packed(vec_a, vec_b, vec_r);
            print_number(vec_r[0]); print_number(vec_r[1]); print_number(vec_r[2]); print_number(vec_r[3]);

            dfpu_subtract_packed(vec_a, vec_b, vec_r);
            print_number(vec_r[0]); print_number(vec_r[1]); print_number(vec_r[2]); print_number(vec_r[3]);

            dfpu_multiply_packed(vec_a, vec_b, vec_r);
            print_number(vec_r[0]); print_number(vec_r[1]); print_number(vec_r[2]); print_number(vec_r[3]);

            dfpu_divide_packed(vec_a, vec_b, vec_r);
            print_number(vec_r[0]); print_number(vec_r[1]); print_number(vec_r[2]); print_number(vec_r[3]);
        }
    }

    dfpu_term();

    return 0;
}