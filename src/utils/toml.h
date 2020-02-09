#ifndef TOML_H_N6JCXECL
#define TOML_H_N6JCXECL

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <time.h>

// Values identifying what the TOML object is.
typedef enum {
  TOML_NOTYPE,
  TOML_TABLE,
  TOML_ARRAY,
  TOML_STRING,
  TOML_INT,
  TOML_DOUBLE,
  TOML_BOOLEAN,
  TOML_DATE,
  TOML_ERROR
} TOMLType;

// Values identifying what the underlying number type is.
// typedef enum {
//   TOML_INT,
//   TOML_DOUBLE
// } TOMLNumberType;

typedef enum {
  TOML_SUCCESS,
  TOML_ERROR_FILEIO,
  TOML_ERROR_FATAL,
  TOML_ERROR_TABLE_DEFINED,
  TOML_ERROR_ENTRY_DEFINED,
  TOML_ERROR_NO_VALUE,
  TOML_ERROR_NO_EQ,
  TOML_ERROR_INVALID_HEADER,
  TOML_ERROR_ARRAY_MEMBER_MISMATCH
} TOMLErrorType;

extern char *TOMLErrorStrings[];
extern char *TOMLErrorDescription[];

// Arbitrary pointer to a TOML object.
typedef void * TOMLRef;

// Struct defining the common part of all TOML objects, giving access to
// the type.
typedef struct TOMLBasic {
  TOMLType type;
} TOMLBasic;

// A TOML array.
typedef struct TOMLArray {
  TOMLType type;
  TOMLType memberType;
  int size;
  TOMLRef *members;
} TOMLArray;

// A TOML table.
typedef struct TOMLTable {
  TOMLType type;
  TOMLArray *keys;
  TOMLArray *values;
} TOMLTable;

// A TOML string.
typedef struct TOMLString {
  TOMLType type;
  int size;
  char content[];
} TOMLString;

// A TOML number.
typedef struct TOMLNumber {
  TOMLType type;
  union {
    int intValue;
    double doubleValue;
    char bytes[8];
  };
} TOMLNumber;

// A TOML boolean.
typedef struct TOMLBoolean {
  TOMLType type;
  int isTrue;
} TOMLBoolean;

// A TOML date.
typedef struct TOMLDate {
  TOMLType type;
  long int sinceEpoch;
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
} TOMLDate;

typedef struct TOMLError {
  TOMLType type;
  TOMLErrorType code;
  int lineNo;
  char * line;
  char * message;
  char * fullDescription;
} TOMLError;

/**********************
 ** Memory Functions **
 **********************/

// Allocate an empty TOML object of a given type.
TOMLRef TOML_alloc( TOMLType );

// Allocates a table and assigns key, value pairs. Table takes ownership of any
// keys and values given and they will be freed if overwritten with setKey or
// freed by TOML_free.
TOMLTable * TOML_allocTable( TOMLString *key, TOMLRef value, ... );

// Allocates an array and appends any values given. Array takes ownership of
// any keys and values given and they will be freed if overwritten with
// setIndex or freed by TOML_free.
TOMLArray * TOML_allocArray( TOMLType memberType, ... );

// TOML_allocString creates a copy of the given string.
TOMLString * TOML_allocString( const char *content );

// TOML_allocStringN creates a copy of the given string up to n.
TOMLString * TOML_allocStringN( char *content, int n );

// Allocate a TOMLNumber and store an int value.
TOMLNumber * TOML_allocInt( int value );

// Allocate a TOMLNumber and store a double value.
TOMLNumber * TOML_allocDouble( double value );

// Allocate a TOMLBoolean and store the truth.
TOMLBoolean * TOML_allocBoolean( int truth );

// Allocate a TOMLDate with the given values.
//
// These values work with c standard tm values. Month is bound 0 to 11.
TOMLDate * TOML_allocDate(
  int year, int month, int day, int hour, int minute, int second
);

// Allocate a TOMLDate with a given GMT timestamp in seconds.
TOMLDate * TOML_allocEpochDate( time_t stamp );

// Allocate an error to be filled by TOML_parse or TOML_stringify.
TOMLError * TOML_allocError( int code );

// Copy any TOML object.
TOMLRef TOML_copy( TOMLRef );

// Free a TOML object.
void TOML_free( TOMLRef );

/*****************
 ** Interaction **
 *****************/

int TOML_isType( TOMLRef, TOMLType );
int TOML_isNumber( TOMLRef );

// Dive through the hierarchy. For each level getKey and getIndex are called to
// get the next level. If it is a table getKey. If it is an array getIndex.
// Each lookup is a string, before getIndex is called, the string will be
// translated into an integer. TOML_find works this way to provide a convenient
// and clear API.
//
// Example:
// TOMLRef ref = TOML_find( table, "child", "nextchild", "0", NULL );
TOMLRef TOML_find( TOMLRef, ... );

// Get the value at the given key.
TOMLRef TOMLTable_getKey( TOMLTable *, const char * );

// Set the value at the given key. If the key is already set, the replaced
// value will be freed.
void TOMLTable_setKey( TOMLTable *, const char *, TOMLRef );

// Return the value stored at the index or NULL.
TOMLRef TOMLArray_getIndex( TOMLArray *, int index );

// Set index of array to the given value. If the index is greater than or equal
// to the current size of the array, the value will be appended to the end.
//
// If a value is replaced, the replaced value will be freed.
void TOMLArray_setIndex( TOMLArray *, int index, TOMLRef );

// Append the given TOML object to the array.
void TOMLArray_append( TOMLArray *, TOMLRef );

/****************
 ** Raw Values **
 ****************/

// Allocates a copy of the string held in TOMLString.
char * TOML_toString( TOMLString * );

// Return pointer to internal string of the node.
const char * TOML_getString( TOMLString * );

// Copy the content of TOMLString to a destination.
void TOML_strcpy( char *dest, TOMLString *src, int size );

// Copy a string.
#define TOML_copyString( self, size, string ) TOML_strcpy( string, self, size )

// Return the TOMLNumber value as integer.
int TOML_toInt( TOMLNumber * );

// Return the TOMLNumber value as double.
double TOML_toDouble( TOMLNumber * );

// Return the c standard tm struct filled with data from this date.
struct tm TOML_toTm( TOMLDate * );

// Return the truth of a TOMLBoolean.
int TOML_toBoolean( TOMLBoolean * );

/************************
 ** Loading and Saving **
 ************************/

// Allocates a table filled with the parsed content of the file.
// Returns non-zero if there was an error.
int TOML_load( char *filename, TOMLTable **, TOMLError * );

// Writes a stringified table to the indicated file.
// Returns non-zero if there was an error.
// TODO: Implement TOML_dump.
// int TOML_dump( char *filename, TOMLTable *, TOMLError * );

// Allocates a table filled with the parsed content of the buffer.
// Returns non-zero if there was an error.
int TOML_parse( char *buffer, TOMLTable **, TOMLError * );

// Allocates a string filled a string version of the table.
// Returns non-zero if there was an error.
int TOML_stringify( char **buffer, TOMLRef, TOMLError * );

#ifdef __cplusplus
};
#endif

#endif /* end of include guard: TOML_H_N6JCXECL */
