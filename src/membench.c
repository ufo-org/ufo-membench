#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <sys/stat.h>
#include <locale.h>

#include "ufo_c/target/ufo_c.h"

#include "logging.h"

#define GB (1024UL * 1024UL * 1024UL)
#define MB (1024UL * 1024UL)
#define KB (1024UL)

typedef struct {
    char *timing;
    size_t size;
    size_t sample_size;
    size_t min_load;
    size_t high_water_mark;
    size_t low_water_mark; 
    size_t writes;
    unsigned int seed;
} Arguments;

static error_t parse_opt (int key, char *value, struct argp_state *state) {
    /* Get the input argument from argp_parse, which we
        know is a pointer to our arguments structure. */
    Arguments *arguments = state->input;
    switch (key) {
        case 's': arguments->size = (size_t) atol(value); break;
        case 'm': arguments->min_load = (size_t) atol(value); break;
        case 'h': arguments->high_water_mark = (size_t) atol(value); break;
        case 'l': arguments->low_water_mark = (size_t) atol(value); break;
        // case 'o': arguments->file = value; break;
        case 't': arguments->timing = value; break;
        case 'n': arguments->sample_size = (size_t) atol(value); break;
        case 'w': arguments->writes = (size_t) atol(value); break;
        case 'S': arguments->seed = (unsigned int) atoi(value); break;
        default:  return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

void callback(void* raw_data, UfoEventandTimestamp* info) {
    CallbackData *data = (CallbackData *) raw_data;
    switch (info->event.tag) {
        case NewCallbackAck:
            //data->zero = info->timestamp_nanos;
        break;  
        case AllocateUfo: 
            data->zero = info->timestamp_nanos;
        break;
        case PopulateChunk:     
        break;
        case GcCycleStart:
        break;
        case UnloadChunk:
        break;
        case UfoReset:
        break;
        case GcCycleEnd:
        break;
        case FreeUfo:
        break;
        case Shutdown:
        break;

    }
    
}

typedef struct {
    uint64_t zero;
} CallbackData;

void run(Arguments *config) {
    UfoCore ufo_system = ufo_new_core("/tmp/", config->high_water_mark, config->low_water_mark);
    if (ufo_core_is_error(&ufo_system)) {
         REPORT("Cannot create UFO core system.\n");
         exit(1);
    }
    
    ufo_new_event_handler(&ufo_system, &callback, NULL);                         

}

int main(int argc, char **argv) {

    Arguments config;
    /* Default values. */
    config.size = 1 *KB;
    config.min_load = 4 *KB;
    config.high_water_mark = 2 *GB;
    config.low_water_mark = 1 *GB;
    config.timing = "membench.csv";
    config.sample_size = 0; // 0 for all
    config.writes = 0; // 0 for none
    config.seed = 42;

    // Parse arguments
    static char doc[] = "UFO performance benchmark utility.";
    static char args_doc[] = "";
    static struct argp_option options[] = {
        {"sample-size",     'n', "FILE",           0,  "How many elements to read from vector: zero for all"},
        {"writes",          'w', "N%%",            0,  "One write will occur once for every N%% reads, zero for read-only"},
        {"size",            's', "#B",             0,  "Vector size (applicable for fib and seq)"},        
        {"min-load",        'm', "#B",             0,  "Min load count for ufo"},
        {"high-water-mark", 'h', "#B",             0,  "High water mark for ufo GC"},
        {"low-water-mark",  'l', "#B",             0,  "Low water mark for ufo GC"},
        {"timing",          't', "FILE",           0,  "Path of CSV output file for time measurements"},        
        {"seed",            'S', "N",              0,  "Random seed, default: 42"},
        { 0 }
    };
    static struct argp argp = { options, parse_opt, args_doc, doc };   
    argp_parse (&argp, argc, argv, 0, 0, &config);

    // Check.
    INFO("Benchmark configuration:\n");
    INFO("  * size:            %lu\n", config.size           );
    INFO("  * min_load:        %lu\n", config.min_load       );
    INFO("  * high_water_mark: %lu\n", config.high_water_mark);
    INFO("  * low_water_mark:  %lu\n", config.low_water_mark );
    INFO("  * timing:          %s\n",  config.timing         );
    INFO("  * writes:          %lu\n", config.writes         );
    INFO("  * sample_size:     %lu\n", config.sample_size    );
    INFO("  * seed:            %u\n",  config.seed           );





}