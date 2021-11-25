#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <argp.h>
#include <sys/stat.h>
#include <locale.h>

#include "ufo_c/target/ufo_c.h"

#include "logging.h"

#define GB (1024UL * 1024UL * 1024UL)
#define MB (1024UL * 1024UL)
#define KB (1024UL)

#define MAX_UFOS 2

bool file_exists (const char *filename) {
  struct stat buffer;   
  return (stat (filename, &buffer) == 0);
}

typedef struct {
    char *csv_file;
    char *parameter_file;
    size_t ufos;
    size_t size;
    size_t sample_size;
    size_t min_load;
    size_t high_water_mark;
    size_t low_water_mark; 
    size_t writes;
    unsigned int seed;
} Arguments;

void parameters_to_csv(Arguments *config) {
    FILE *output_stream;
    // if (!file_exists(data->csv_file)) {
    output_stream = fopen(config->parameter_file, "w");
    fprintf(output_stream,                
            "ufos,"
            "size,"
            "sample_size,"
            "min_load,"
            "high_water_mark,"
            "low_water_mark,"
            "writes\n");
    
    fprintf(output_stream, 
            "%li,%li,%li,%li,%li,%li,%li\n",
            config->ufos,
            config->size,
            config->sample_size,
            config->min_load,
            config->high_water_mark,
            config->low_water_mark,
            config->writes);    

    fclose(output_stream);
}

static error_t parse_opt (int key, char *value, struct argp_state *state) {
    /* Get the input argument from argp_parse, which we
        know is a pointer to our arguments structure. */
    Arguments *arguments = state->input;
    switch (key) {
        case 'u': arguments->ufos = (size_t) atol(value); break;
        case 's': arguments->size = (size_t) atol(value); break;
        case 'm': arguments->min_load = (size_t) atol(value); break;
        case 'h': arguments->high_water_mark = (size_t) atol(value); break;
        case 'l': arguments->low_water_mark = (size_t) atol(value); break;
        // case 'o': arguments->file = value; break;
        case 'o': arguments->csv_file = value; break;
        case 'p': arguments->parameter_file = value; break;
        case 'n': arguments->sample_size = (size_t) atol(value); break;
        case 'w': arguments->writes = (size_t) atol(value); break;
        case 'S': arguments->seed = (unsigned int) atoi(value); break;
        default:  return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

#define MAX_HISTORY 10000

typedef struct {
    char event_type;
    uint64_t timestamp; // nannos

    // How many UFOs are in use.
    uint64_t ufos;

    // How many chunks are materialized and are currently in use.
    uint64_t materialized_chunks;    

    // How much memory the user sets out to allocate as areas of memory. In
    // bytes.
    uint64_t intended_memory_usage;

    // How much memory is created as areas of memory. This is the number above
    // plus padding to page boundaries. In bytes.
    uint64_t apparent_memory_usage;    
    
    // How much memory is initialized and usable. This counts materialized
    // chunks and headers. In bytes.
    uint64_t memory_usage;

    // How much disk space is in use. This accounts for dematerialized chunks
    // being cached on disk. In bytes.
    uint64_t disk_usage;

    // How much memory is initialized by each UFO. In bytes.
    uint64_t memory_usage_per_ufo[MAX_UFOS];

    // How much memory is initialized by each UFO. In bytes.
    uint64_t disk_usage_per_ufo[MAX_UFOS];
} Event;

typedef struct {
    const char *csv_file;
    uint64_t events;
    Event current;
    Event history[MAX_HISTORY];
} CallbackData;

void callback_data_to_csv(CallbackData *data) {
    FILE *output_stream;
    // if (!file_exists(data->csv_file)) {
    output_stream = fopen(data->csv_file, "w");
    fprintf(output_stream,                
            "event,"
            "timestamp,"
            "ufos,"
            "materialized_chunks,"
            "intended_memory_usage,"
            "apparent_memory_usage,"
            "memory_usage,"
            "disk_usage,");

    for (size_t j = 0; j < MAX_UFOS; j++) {
        fprintf(output_stream, 
                "ufo_%li_memory_usage,"
                "ufo_%li_disk_usage,", j, j);
    }

    fprintf(output_stream,"\n");

    for (size_t i = 0; i < data->events; i++) {
        fprintf(output_stream, 
            "%c,%li,%li,%li,%li,%li,%li,%li,",
            data->history[i].event_type,
            data->history[i].timestamp,
            data->history[i].ufos,
            data->history[i].materialized_chunks,
            data->history[i].intended_memory_usage,
            data->history[i].apparent_memory_usage,
            data->history[i].memory_usage,
            data->history[i].disk_usage);

        for (size_t j= 0; j < MAX_UFOS; j++) {
            fprintf(output_stream, 
                    "%li,%li,",
                    data->history[i].memory_usage_per_ufo[j],
                    data->history[i].disk_usage_per_ufo[j]);
        }

        fprintf(output_stream, "\n");
    }

    fclose(output_stream);
}

void callback(void* raw_data, const UfoEventandTimestamp* info) {

    INFO("Callback(timestamp=%li, event=%u)\n", info->timestamp_nanos, info->event.tag);

    CallbackData *data = (CallbackData *) raw_data;

    // Everyone has a timestamp
    data->current.timestamp = info->timestamp_nanos;

    switch (info->event.tag) {
        case AllocateUfo: 
            data->current.event_type = 'A'; // llocate UFO
            data->current.ufos++;
            data->current.memory_usage += info->event.allocate_ufo.header_size_with_padding;
            data->current.intended_memory_usage += info->event.allocate_ufo.intended_header_size;
            data->current.intended_memory_usage += info->event.allocate_ufo.intended_body_size;
            data->current.apparent_memory_usage += info->event.allocate_ufo.total_size_with_padding;         
            //printf("UFO_ID: %li\n", info->event.allocate_ufo.ufo_id);

//            data->current.disk_usage_per_ufo[info->event.allocate_ufo.ufo_id - 1] 
        break;
        case FreeUfo:
            data->current.event_type = 'F'; // ree UFO
            data->current.ufos--;                        
            data->current.materialized_chunks -= info->event.free_ufo.chunks_freed;
            data->current.memory_usage -= info->event.allocate_ufo.header_size_with_padding;
            data->current.memory_usage -= info->event.free_ufo.memory_freed;
            data->current.disk_usage -= info->event.free_ufo.disk_freed;
            data->current.intended_memory_usage -= info->event.free_ufo.intended_header_size;
            data->current.intended_memory_usage -= info->event.free_ufo.intended_body_size;
            data->current.apparent_memory_usage -= info->event.free_ufo.total_size_with_padding;

            data->current.memory_usage_per_ufo[info->event.free_ufo.ufo_id - 1] -= info->event.allocate_ufo.header_size_with_padding;
            data->current.memory_usage_per_ufo[info->event.free_ufo.ufo_id - 1] -= info->event.free_ufo.memory_freed;
            data->current.disk_usage_per_ufo[info->event.free_ufo.ufo_id - 1] -= info->event.free_ufo.disk_freed;         
        break;
        case PopulateChunk:
            data->current.event_type = 'M'; // aterialize chunk
            data->current.materialized_chunks++;
            data->current.memory_usage += info->event.populate_chunk.memory_used;
            data->current.memory_usage_per_ufo[info->event.populate_chunk.ufo_id - 1] += info->event.populate_chunk.memory_used;
        break;
        case UnloadChunk:
            data->current.event_type = 'D'; // ematerialize chunk
            data->current.materialized_chunks--;
            data->current.memory_usage -= info->event.unload_chunk.memory_freed;
            data->current.memory_usage_per_ufo[info->event.unload_chunk.ufo_id - 1] -= info->event.unload_chunk.memory_freed;
            if (info->event.unload_chunk.disposition == NewlyDirty) {                
                data->current.disk_usage += info->event.unload_chunk.memory_freed;
                data->current.disk_usage_per_ufo[info->event.unload_chunk.ufo_id - 1] += info->event.unload_chunk.memory_freed;
            }                         
        break;
        case UfoReset:
            data->current.event_type = 'R'; // eset UFO
            data->current.disk_usage -= info->event.ufo_reset.disk_freed;
            data->current.memory_usage -= info->event.ufo_reset.memory_freed;
            data->current.materialized_chunks -= info->event.ufo_reset.chunks_freed;
            data->current.memory_usage_per_ufo[info->event.ufo_reset.ufo_id - 1] -= info->event.ufo_reset.memory_freed;
            data->current.disk_usage_per_ufo[info->event.ufo_reset.ufo_id - 1] -= info->event.ufo_reset.disk_freed;
        break;
        case GcCycleStart:
            data->current.event_type = 'S'; // tart GC
        break;
        case GcCycleEnd:
            data->current.event_type = 'E'; // nd GC            
        break;
        case NewCallbackAck:
            data->current.event_type = '*';
            // Ignore
        break;  
        case Shutdown:
            data->current.event_type = '+';
            // Ignore
        break;
        default:
            fprintf(stderr, "Unknown case in event handler %i.\n", info->event.tag);
            exit(2);
    }

    // Copy the contents of data->current into data->history as the most recent event
    memcpy(&(data->history[data->events]), &(data->current), sizeof(Event));

    INFO("History[%li]:\n", data->events);
    INFO("  * event_type:            %c\n",  data->history[data->events].event_type);
    INFO("  * timestamp:             %li\n", data->history[data->events].timestamp);
    INFO("  * ufos:                  %li\n", data->history[data->events].ufos);
    INFO("  * materialized_chunks:   %li\n", data->history[data->events].materialized_chunks);
    INFO("  * intended_memory_usage: %li\n", data->history[data->events].intended_memory_usage);
    INFO("  * apparent_memory_usage: %li\n", data->history[data->events].apparent_memory_usage);
    INFO("  * memory_usage:          %li\n", data->history[data->events].memory_usage);
    INFO("  * disk_usage:            %li\n", data->history[data->events].disk_usage);

    data->events++;
    if (data->events >= MAX_HISTORY) {
        fprintf(stderr, "Too many events (MAX_HISTORY=%i)", MAX_HISTORY);
        exit(1);
    }
}

typedef struct {
    size_t from;
    size_t to;
    size_t by;
    size_t length;
} Seq;

int32_t seq_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes) {

    Seq *data = (Seq *) user_data;
    int64_t* target = (int64_t *)  target_bytes;

    for (size_t i = 0; i < end - start; i++) {
        target[i] = data->from + data->by * (i + start);
    }

    return 0;
}

int64_t *seq_new(UfoCore *ufo_system, size_t from, size_t to, size_t by, bool read_only, size_t min_load_count) {
    Seq *data = (Seq *) malloc(sizeof(Seq));

    data->from = from; 
    data->to = to;
    data->length = (to - from) / by + 1;
    data->by = by;

    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(int64_t);
    parameters.element_ct = data->length;
    parameters.min_load_ct = min_load_count;
    parameters.read_only = false;
    parameters.populate_data = data; // populate data is copied by UFO, so this should be fine.
    parameters.populate_fn = seq_populate;
    parameters.writeback_listener = NULL;
    parameters.writeback_listener_data = NULL;

    UfoObj ufo_object = ufo_new_object(ufo_system, &parameters);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot create UFO object.\n");
        return NULL;
    }

    int64_t *pointer = ufo_header_ptr(&ufo_object);
    return pointer;
}

void seq_free(UfoCore *ufo_system, int64_t *ptr) {
    UfoObj ufo_object = ufo_get_by_address(ufo_system, ptr);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot free %p: not a UFO object.\n", ptr);
        return;
    }

    UfoParameters parameters;
    int result = ufo_get_params(ufo_system, &ufo_object, &parameters);
    if (result < 0) {
        REPORT("Unable to access UFO parameters, so cannot free source matrix\n");
    }
    free(parameters.populate_data);

    ufo_free(ufo_object);
}

int random_int(int ceiling) {
    return rand() % ceiling;
}

size_t select_ufo(size_t i, size_t n, size_t ufos) {
    for (size_t u = ufos; u > 0; u--) {
        if (i > (u * n / ufos)) {
            return u - 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {

    Arguments config;
    /* Default values. */
    config.ufos = 1;
    config.size = 1 *KB;
    config.min_load = 4 *KB;
    config.high_water_mark = 2 *GB;
    config.low_water_mark = 1 *GB;
    config.csv_file = "membench.csv";
    config.parameter_file = "parameters.csv";
    config.sample_size = 0; // 0 for all
    config.writes = 0; // 0 for none
    config.seed = 42;

    // // Parse arguments
    static char doc[] = "UFO performance benchmark utility.";
    static char args_doc[] = "";
    static struct argp_option options[] = {
        {"ufos",            'u', "N",              0,  "How many vectors to allocate and use"},
        {"sample-size",     'n', "FILE",           0,  "How many elements to read from vector: zero for all"},
        {"writes",          'w', "N",            0,  "One write will occur once for every N reads, zero for read-only"},
        {"size",            's', "#B",             0,  "Vector size (applicable for fib and seq)"},        
        {"min-load",        'm', "#B",             0,  "Min load count for ufo"},
        {"high-water-mark", 'h', "#B",             0,  "High water mark for ufo GC"},
        {"low-water-mark",  'l', "#B",             0,  "Low water mark for ufo GC"},
        {"csv_file",        'o', "FILE",           0,  "Path of CSV output file containing observations"},        
        {"parameter_file",  'p', "FILE",           0,  "Path of CSV output file containing parameters"},  
        {"seed",            'S', "N",              0,  "Random seed, default: 42"},
        { 0 }
    };
    static struct argp argp = { options, parse_opt, args_doc, doc };   
    argp_parse (&argp, argc, argv, 0, 0, &config);

    // Check.
    INFO("Benchmark configuration:\n");
    INFO("  * ufos:            %lu\n", config.ufos           );
    INFO("  * size:            %lu\n", config.size           );
    INFO("  * min_load:        %lu\n", config.min_load       );
    INFO("  * high_water_mark: %lu\n", config.high_water_mark);
    INFO("  * low_water_mark:  %lu\n", config.low_water_mark );
    INFO("  * csv_file:        %s\n",  config.csv_file       );
    INFO("  * parameter_file:  %s\n",  config.parameter_file );
    INFO("  * writes:          %lu\n", config.writes         );
    INFO("  * sample_size:     %lu\n", config.sample_size    );
    INFO("  * seed:            %u\n",  config.seed           );

    // Check how many UFOs we can handle in our recording structures.
    if (config.ufos > MAX_UFOS) {
        fprintf(stderr, "Exceeded max number of ufos: %li > %i\n", config.ufos, MAX_UFOS);
        exit(3);
    }

    // Set seed.
    srand(config.seed);

    //ufo_begin_log();

    // Run the benchmark
    CallbackData *data = (CallbackData *) calloc(1, sizeof(CallbackData));
    data->csv_file = config.csv_file;

    INFO("Starting up UFO core\n");
    UfoCore ufo_system = ufo_new_core("/tmp/", config.high_water_mark, config.low_water_mark);
    if (ufo_core_is_error(&ufo_system)) {
          REPORT("Cannot create UFO core system.\n");
          exit(1);
    }
    
    // Attach event recorder.
    ufo_new_event_handler(&ufo_system, (void *) data, &callback);

    // Allocate so many ufos.
    INFO("Allocating %li UFOs\n", config.ufos);

    int64_t **ufos = (int64_t **) malloc(config.ufos * sizeof(int64_t *));
    // for (size_t i = 0; i < config.ufos; i++) {
    //      ufos[i] = seq_new(&ufo_system, 0, config.size, 1, config.writes == 0, config.min_load);
    // }

    INFO("Running...\n");
    int64_t sum = 0;
    size_t last_write = 0;
    size_t n = (config.sample_size == 0 ? config.size : config.sample_size);  
    for (size_t j = 0; j < config.ufos; j++) {
        INFO("Allocating %li-th UFO\n", j);
        ufos[j] = seq_new(&ufo_system, 0, config.size / (j + 1), 1, config.writes == 0, config.min_load);
        for (size_t i = 0; i < n / (j + 1); i++) {        
            // INFO("UFO %li <- %li/%li\n", ufo, i, n);
            if ((config.writes != 0) && (i == last_write + config.writes)) {
                ufos[j][i] = 42;
                last_write = i;
            } else {
                sum += ufos[j][i];
            }
            if (i == n / 2 && j > 0) {
                INFO("Freeing %li-th UFO\n", i);
                seq_free(&ufo_system, ufos[j]);
            }
        }
    }

    INFO("Freeing %li-th UFO\n", config.ufos - 1);
    //for (size_t i = 0; i < config.ufos; i++) {
    seq_free(&ufo_system, ufos[config.ufos - 1]);
    //}
    // seq_free(&ufo_system, ufo);

    INFO("Shutting down UFO core\n");
    ufo_core_shutdown(ufo_system);
    INFO("Core shutdown, the shutdown message should strictly preceeed this\n");

    // Report events.
    INFO("Recorded %li events\n", data->events);
    INFO("Sum: %li\n", sum);
    callback_data_to_csv(data);
    parameters_to_csv(&config);

    free(data);

    return 0;
}
