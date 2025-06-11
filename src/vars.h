#include <stdint.h>

#ifdef ANALYZE_VARS
extern uint16_t UQSTR_ARGS;
extern uint16_t UQSTR_DEFINE;
extern uint16_t GLOBAL_GET;
#endif

typedef struct __attribute__((__packed__)) Variable {
    uint16_t name;
    uint16_t value;
} Variable;

Variable * add_variable(char * name, uint16_t value);
Variable * add_var(uint16_t name, uint16_t value);
uint16_t set_var(uint16_t name, uint16_t value);
uint16_t get_var(uint16_t name);
Variable * quiet_lookup_variable(uint16_t name);
Variable * lookup_variable(uint16_t name);
Variable * safe_lookup_var(uint16_t name);

#ifdef ANALYZE_VARS
void init_varstackmodel();
#endif

