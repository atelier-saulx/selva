#pragma once
#ifndef _TYPESTR_H_
#define _TYPESTR_H_

/**
 * Get type of a variable as a string.
 * @param v is a variable.
 * @returns a C string.
 */
#define typeof_str(v) _Generic((0, v), \
    char: "char", \
    signed char: "signed char", \
    short: "short", \
    int: "int" , \
    long: "long", \
    long long: "long long", \
    unsigned char: "unsigned char", \
    unsigned short: "unsigned short", \
    unsigned int: "unsigned int", \
    unsigned long: "unsigned long", \
    unsigned long long: "unsigned long long", \
    float: "float", \
    double: "double", \
    long double: "long double", \
    char *: "char *", \
    signed char *: "signed char *", \
    short *: "short *", \
    int *: "int *", \
    long *: "long *", \
    long long *: "long long *", \
    unsigned char *: "unsigned char *", \
    unsigned short *: "unsigned short *", \
    unsigned int *: "unsigned int *", \
    unsigned long *: "unsigned long *", \
    unsigned long long *: "unsigned long long *", \
    float *: "float *", \
    double *: "double *", \
    long double *: "long double *", \
    default: "other type")

#endif /* _TYPESTR_H_ */
