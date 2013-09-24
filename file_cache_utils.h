
#define FILE_SIZE 10000

/*holds data/metadata for one file*/
typedef struct file_s {

    int pin_count;		// number of threads that have pinned this file
    char *file_name;		// filename corresponding to cache
    bool is_dirty;		// if opened in write mode
    char data[FILE_SIZE];	// copy of file in cache
    struct file_s *next;  	// pointer to next file in list: each file is maintained in linked list
} file_t;

/*holds complete state information file_cache*/
typedef struct file_cache{

    pthread_mutex_t mutex;      // one mutex per file_cache

    int max_entries;		// max possible cached files
    int pin_list_count;		// count of pinned files
    int unpin_list_count;	// count of unpinned files
				// we do not remove unpinned files immdeiately, since 
				// they might be pinned again by another thread
    file_t *pin_list_head;	// pointer to linked list of pinned files
    file_t *unpin_list_head;	// pointer to linked list of unpinned files

    file_t **pinned_list_match;	// scratch variable:- used by each thread for efficiency
    char **unpinned_file_names;	// scratch variable:- used by each thread for efficiency
} file_cache_t;

static file_t *get_file_present_in_list(file_t *file_t_list, char *filename);

static bool add_to_list(file_t **file_list, file_t *file);

static bool remove_from_list(file_t **file_list, file_t *file);

static file_t *add_back_to_pinned_list(file_cache_t *cache, char *file_to_be_pinned);

static file_t *add_back_to_unpinned_list(file_cache_t *cache, char *file_to_be_unpinned);

static file_t *create_new_file_t(const char *filename);

static void pin_new_file(file_cache_t *cache, file_t *file);

static bool flush_file(file_t *file);

static void remove_unpin_file(file_cache_t *cache, file_t *file);

static bool create_pinned_file(file_cache_t *cache, const char *file_name);


