/*
 *
 * NMEA library
 * URL: http://nmea.sourceforge.net
 * Author: Tim (xtimor@gmail.com)
 * Licence: http://www.gnu.org/licenses/lgpl.html
 * $Id: tok.c 17 2008-03-11 11:56:11Z xtimor $
 *
 */

/*! \file tok.h */

#include "nmea/tok.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <errno.h>

#define NMEA_TOKS_COMPARE   (1)
#define NMEA_TOKS_PERCENT   (2)
#define NMEA_TOKS_WIDTH     (3)
#define NMEA_TOKS_TYPE      (4)

/**
 * \brief Calculate control sum of binary buffer
 */
int nmea_calc_crc(const char *buff, int buff_sz)
{
    int chsum = 0,
        it;

    for(it = 0; it < buff_sz; ++it)
        chsum ^= (int)buff[it];

    return chsum;
}

/**
 * \brief Convert string to number
 */
int nmea_atoi(const char *str, int str_sz, int radix)
{
    char *tmp_ptr;
    char buff[NMEA_CONVSTR_BUF];
    int res = 0;

    if(str && str_sz >= 0 && str_sz < NMEA_CONVSTR_BUF)
    {
        memcpy(&buff[0], str, str_sz);
        buff[str_sz] = '\0';
        res = strtol(&buff[0], &tmp_ptr, radix);
    }

    return res;
}

/**
 * \brief Convert string to fraction number
 */
double nmea_atof(const char *str, int str_sz)
{
    char *tmp_ptr;
    char buff[NMEA_CONVSTR_BUF];
    double res = 0;

    if(str && str_sz >= 0 && str_sz < NMEA_CONVSTR_BUF)
    {
        memcpy(&buff[0], str, str_sz);
        buff[str_sz] = '\0';
        res = strtod(&buff[0], &tmp_ptr);
    }

    return res;
}

/**
 * \brief Formating string (like standart printf) with CRC tail (*CRC)
 */
int nmea_printf(char *buff, int buff_sz, const char *format, ...)
{
    int retval, add = 0;
    va_list arg_ptr;

    if(buff_sz <= 0)
        return 0;

    va_start(arg_ptr, format);

    retval = NMEA_POSIX(vsnprintf)(buff, buff_sz, format, arg_ptr);

    if(retval > 0 && retval < buff_sz)
    {
        add = NMEA_POSIX(snprintf)(
            buff + retval, buff_sz - retval, "*%02x\r\n",
            nmea_calc_crc(buff + 1, retval - 1));
    }

    retval += add;

    if(retval < 0 || retval >= buff_sz)
    {
        memset(buff, ' ', buff_sz);
        retval = buff_sz;
    }

    va_end(arg_ptr);

    return retval;
}

/**
 * \brief Analyse string (specificate for NMEA sentences)
 */
int nmea_scanf(const char *buff, int buff_sz, const char *format, ...)
{
    const char *end, *begin, *stop;
    char number[NMEA_CONVSTR_BUF], *number_end, type;
    int count = 0, width, length, base;
    long integer;
    double real;
    void *target;
    va_list args;

    if(!buff || !format || buff_sz < 0)
        return -1;
    end = buff + buff_sz;
    va_start(args, format);
    while(*format && buff < end)
    {
        if(*format != '%')
        {
            if(*buff != *format)
                break;
            ++buff;
            ++format;
            continue;
        }
        ++format;
        width = 0;
        while(*format >= '0' && *format <= '9')
        {
            if(width > (INT_MAX - 9) / 10)
                goto invalid;
            width = width * 10 + *format++ - '0';
        }
        type = *format++;
        begin = buff;
        if(type == 's' || type == 'S')
        {
            /* String widths are capacities minus the trailing NUL. */
            if(width <= 0)
                goto invalid;
            stop = *format ? memchr(buff, *format, (size_t)(end - buff)) : end;
            if(!stop)
                stop = end;
            if(stop - buff > width)
                goto invalid;
            buff = stop;
        }
        else if(type == 'c' || type == 'C')
        {
            if(buff < end && *buff != *format)
                ++buff;
        }
        else if(width)
        {
            if(end - buff < width)
                goto invalid;
            buff += width;
        }
        else
        {
            stop = *format ? memchr(buff, *format, (size_t)(end - buff)) : end;
            buff = stop ? stop : end;
        }
        length = (int)(buff - begin);
        if(type == 's' || type == 'S' || type == 'c' || type == 'C')
        {
            target = va_arg(args, char *);
            if(target)
            {
                if(type == 's' || type == 'S')
                {
                    memcpy(target, begin, (size_t)length);
                    ((char *)target)[length] = '\0';
                }
                else
                    *(char *)target = length ? *begin : '\0';
            }
        }
        else
        {
            if(length >= NMEA_CONVSTR_BUF)
                goto invalid;
            memcpy(number, begin, (size_t)length);
            number[length] = '\0';
            errno = 0;
            switch(type)
            {
            case 'f': case 'g': case 'G': case 'e': case 'E':
                target = va_arg(args, double *);
                real = length ? strtod(number, &number_end) : 0.0;
                if(length && (number_end != number + length || errno == ERANGE ||
                   !(real <= DBL_MAX && real >= -DBL_MAX)))
                    goto invalid;
                if(target)
                    *(double *)target = real;
                break;
            case 'd': case 'i': case 'u': case 'x': case 'X': case 'o':
                target = va_arg(args, int *);
                base = (type == 'x' || type == 'X') ? 16 : (type == 'o' ? 8 : 10);
                integer = length ? strtol(number, &number_end, base) : 0;
                if(length && (number_end != number + length || errno == ERANGE ||
                   integer < INT_MIN || integer > INT_MAX))
                    goto invalid;
                if(target)
                    *(int *)target = (int)integer;
                break;
            default:
                goto invalid;
            }
        }
        ++count;
    }
    va_end(args);
    return count;
invalid:
    va_end(args);
    return -1;
}
