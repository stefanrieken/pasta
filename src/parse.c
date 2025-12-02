#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "terminal.h"
#include "pasta.h"
#include "stack.h"
#include "vars.h"
#include "base.h"
#include "int.h"
#include "file.h"

int read_char(FILE * file) {
    int result;
#ifdef ANSI_TERM
    result = isatty(fileno(file)) ? getch() : fgetc(file);
#else
    result = fgetc(file);
#ifdef PICO_SDK
    if (result == '\r') result = '\n'; // \r is EOL in serial terminal#endif
    if (isatty(fileno(file))) printf("%c", result);
#endif
#endif
  return result;
}

static inline
bool is_bracket_char(int ch) {
    return ch == '(' || ch == ')' || ch == '{' || ch == '}';
}

static inline
bool is_whitespace_char(int ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}
int next_non_whitespace_char(FILE * infile, int until) {
    int ch = read_char(infile);
    while (ch != EOF && ch != until && is_whitespace_char(ch)) ch = read_char(infile);
    return ch;
}


void add_command(unsigned char cmd, uint16_t value) {
    if (value > 59) {
        // Can't encode within 2^6 - 4
        // encode as 'word follows' (for now always 16-bit)
        memory[mem[TOP_OF+CODE]++] = cmd | (WORD_FOLLOWS << 2);
        // Little endian
        memory[mem[TOP_OF+CODE]++] = value & 0xFF;
        memory[mem[TOP_OF+CODE]++] = (value >> 8) & 0xFF;
    } else {
        memory[mem[TOP_OF+CODE]++] = cmd | (value << 2);
    }
    if(mem[TOP_OF+CODE] >= (uint16_t) (mem[END_OF+CODE]-1)) printf("Code overflow!\n");
}

int parse_int(FILE * infile, int ch, int * result, int radix) {
    // Allow letters for numbers 10 and higher, dependent on radix
    int normalized = ch;
    if (ch >= 'a' && ch <= 'z') normalized = '0'+10+(ch-'a');
    else if (ch >= 'A' && ch <= 'Z') normalized = '0'+10+(ch-'A');
    while(normalized >= '0' && normalized <= '0'+radix) {
        *result = (*result * radix) + (normalized-'0');
        ch = read_char(infile);
        normalized = ch;
        if (ch >= 'a' && ch <= 'z') normalized = '0'+10+(ch-'a');
        else if (ch >= 'A' && ch <= 'Z') normalized = '0'+10+(ch-'A');
        else if (ch < '0' || ch > '9') break;
    }
    return ch;
}

// Used for characters in strings and chars
char * escapes = "nrtfe";
char * ctrlchars = "\n\r\t\f\e";

// Call after having parsed '\'
int read_escaped_char(FILE * infile, int * result) {
    int ch = read_char(infile);
    if (ch == 'x') {
        ch = parse_int(infile, read_char(infile), result, 16);
    } else {
        for (int j=0; j<strlen(escapes); j++) {
            if (escapes[j] == ch) { ch = ctrlchars[j]; break; }
        }
        *result=ch;
        ch = read_char(infile);
    }

    return ch;
}

#ifdef ANALYZE_VARS
enum { VAR_LOCAL, VAR_INTERMEDIATE, VAR_GLOBAL };

void generate_var_ref(uint16_t name) {
//    printf("Start analysis of %s\n", (char*) &memory[name]);
    int var_type = VAR_LOCAL;
    int where = 0;

#ifdef LEXICAL_SCOPING
    int parents[16];
    int num_parents=0;
#endif

    int i=varstackmodel.length-1;
    while (i > 0 && varstackmodel.values[i] != name) {
        int16_t val = ((int16_t) (varstackmodel.values[i]));
#ifdef LEXICAL_SCOPING
        if (val == UQSTR_PARENT) {
            var_type=VAR_INTERMEDIATE;
            parents[num_parents++]=i*sizeof(Variable);
            if(num_parents > 16) printf("Too much nesting\n");
            i--;
        } else
#endif
        i--;
    }

    if (i < 0 || varstackmodel.values[i] != name) { printf("Not found: %s\n", (char *) &memory[name]); return; } // var not found
    else where=i*sizeof(Variable); // 2*16 = 32 bit

    while (i > 0) {
        if(varstackmodel.values[i] == UQSTR_PARENT) break;
        i--;
    }
    if(varstackmodel.values[i] != UQSTR_PARENT) var_type=VAR_GLOBAL;

    if(var_type == VAR_INTERMEDIATE) {
//        printf("Constructing intermediate reference\n");
#ifdef LEXICAL_SCOPING
        // `get-at` (name may be improved upon) gets relative indices
        // Note that in benchmark.pasta it is not actually faster than
        // the get "name" from below (but the scenario isn't too
        // challenging on this point either).
        // Also note that we do not yet have a 'set-at'.
        add_command(CMD_PUSH, GLOBAL_GET_AT);
        int previous=(varstackmodel.length)*sizeof(Variable);
        for (int i=0;i<num_parents;i++) {
            add_command(CMD_PUSH, previous - parents[i]);
            previous = parents[i]-sizeof(Variable); // TODO not sure why this -sizeof... works out
        }
        add_command(CMD_PUSH, previous - where);
        add_command(CMD_EVAL, num_parents+2);
#else
        // Just 'get' intermediate vars by name
        add_command(CMD_PUSH, GLOBAL_GET);
        add_command(CMD_PUSH, name);
        add_command(CMD_EVAL, 2);
#endif
    } else {
        if (var_type == VAR_GLOBAL) {} // where -= mem[VARS]; // offset in 32-bit Variables == even
        else where = ((varstackmodel.length*sizeof(Variable))-where) | 1; // make odd to flag local
        add_command(CMD_REF, where);

//printf("Var %s found on %d, type %d varstack length %d\n", (char *) &memory[name], where, var_type, varstackmodel.length);
    }
}
#endif

// We compile toplevel code directly to memory as one big main function,
// which starts after the primitive references.
// Notice this presently still misses a proper skip_code header,
// like we have for blocks (but primitive references are differentiated with a zero).
char buffer[256];

int parse(FILE * infile, int until, bool repl) {

    int num_args = 0;
#ifdef ANALYZE_VARS
    bool in_args = false;
    bool in_define = false;
    uint16_t to_define = 0;
#ifndef AUTO_BIND
    bool in_bind = false;
#endif
#endif

    int ch = next_non_whitespace_char(infile, until);

    while (ch != until && ch != EOF) {
        if (ch == '#') {
            // skip comments
            do { ch = read_char(infile); } while (ch != '\n');
//            ch = next_non_whitespace_char(infile, until);
            if (ch == until) goto end;
        }

        if (ch >='0' && ch <= '9') {
            int radix = 10;
            if (ch == '0') {
                ch = read_char(infile);
                if(ch == 'b') { radix = 2; ch = read_char(infile); }
                else if(ch == 'x') { radix = 16; ch = read_char(infile); }
            }
            int result = 0;
            ch = parse_int(infile, ch, &result, radix);
            add_command(CMD_PUSH, result);
            num_args++;
            if (!is_whitespace_char(ch) || ch == until) continue; // so that superfluous 'ch' is processed in next round
        } else if (ch=='\'') {
            ch = read_char(infile);
            int result=ch;
            if(ch == '\\') ch = read_escaped_char(infile, &result);
            else ch=read_char(infile);
            if (ch != '\'') printf("Error: expecting char\n");
            add_command(CMD_PUSH, result);
            num_args++;
        } else if (ch=='\"') {
            int i = 0;
            ch = read_char(infile);
            while (ch != '\"') {
                int result = ch;
                if(ch == '\\') ch = read_escaped_char(infile, &result);
                else ch = read_char(infile);
                buffer[i++] = result;
            }
            buffer[i++] = 0;
            uint16_t uqstr = unique_string(buffer);
            add_command(CMD_PUSH, uqstr);
            num_args++;
#ifdef ANALYZE_VARS
            if (in_define && num_args == 2) { to_define = uqstr; }
            if (in_args) push(&varstackmodel, uqstr);
#endif
        } else if (ch == '{') {
// When manually binding, detect function at 'bind' to distinguish from closure
#ifdef ANALYZE_VARS
#ifndef AUTO_BIND
            if (in_bind)
#endif
            {
                push(&varstackmodel, UQSTR_CLOSURE); // Parent scope will have a closure var
            }
            int vml = varstackmodel.length;
#ifndef AUTO_BIND
            if (in_bind)
#endif
            {
                push(&varstackmodel, UQSTR_PARENT); // register scope boundary (== parent pointer) TODO ifndef AUTO_BIND, analyze call to `bind` instead
            }
#endif
            int skip_from = mem[TOP_OF+CODE];
            add_command(CMD_SKIP, 0xFFFF); // Force creation of word-sized value, to be overwritten later
            parse(infile, '}', repl);
            int relative = mem[TOP_OF+CODE] - skip_from - 3;
            memory[skip_from+1] = relative & 0xFF;
            memory[skip_from+2] = (relative >> 8) & 0xFF;
            num_args++;
#ifdef ANALYZE_VARS
            varstackmodel.length = vml; // Done parsing block == done analyzing block
//            push(&varstackmodel, UQSTR_CLOSURE); // Do leave closure
#endif
        } else if (ch == ';') {
            add_command(CMD_EVAL, num_args);
            add_command(CMD_EVAL, 0); // clear argstack
            num_args = 0;
#ifdef ANALYZE_VARS
            if (in_define) {
                push(&varstackmodel, to_define);
                in_define=false;
                to_define=0;
            }
            in_args = false;
#ifndef AUTO_BIND
            in_bind = false;
#endif
#endif
        } else if (ch == '(') {
            parse(infile, ')', repl);
            num_args++;
        } else if (!is_whitespace_char(ch)) {
            // printf("Label\n");
            int i=0;
            while (ch != EOF && !is_whitespace_char(ch) && !is_bracket_char(ch) && ch != ';') { buffer[i++] = ch; ch = read_char(infile); }
            buffer[i++] = 0;
            //printf("Label is: '%s'\n", buffer);
            uint16_t uqstr = unique_string(buffer);
#ifdef ANALYZE_VARS
            generate_var_ref(uqstr);
            num_args++;
            if(num_args == 1 && (UQSTR_ARGS == uqstr || UQSTR_ENUM == uqstr || UQSTR_BITFIELD == uqstr)) { in_args = true; }
            if(num_args == 1 && UQSTR_DEFINE == uqstr) { in_define = true; }
#ifndef AUTO_BIND
            if(num_args == 1 && UQSTR_BIND == uqstr) { in_bind = true; }
#endif
#else
            add_command(CMD_REF, uqstr); // #else
            num_args++;
#endif
            if (!is_whitespace_char(ch) || ch == until) continue; // so that superfluous 'ch' is processed in next round
        }

        ch = next_non_whitespace_char(infile, until);
    }

end:

    if(num_args > 0) add_command(CMD_EVAL, num_args);

#ifdef ANALYZE_VARS
        if (in_define) {
            push(&varstackmodel, to_define);
            in_define=false;
            to_define=0;
        }
        in_args = false;
#ifndef AUTO_BIND
        in_bind = false;
#endif
#endif

    return ch;
}

