#ifndef PARSE_RESULT_H
#define PARSE_RESULT_H

enum ParseResult
{
    PARSE_ERROR = 0,
    PARSE_SUCCESS,
    PARSE_INCOMPLETE
};

/** Names of enum values above; useful for debbugging;
 * Use with care, as only valid enums will have values
 */
extern const char *PARSE_RESULT_STRINGS_[];

#endif // PARSE_RESULT_H
