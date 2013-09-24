#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include "file_cache.h"
#include "file_cache_utils.h"
#include <stdlib.h>
#include <string.h>
#include <sched.h>




/*initialize max_entries and default values*/
file_cache_t *filecacheconstruct(int max_cache_entries)
{
   file_cache_t *new_file_cache = (file_cache_t *) malloc(sizeof(file_cache_t));
   new_file_cache->max_entries = max_cache_entries;
   new_file_cache->pin_list_count = 0;
   new_file_cache->unpin_list_count=0;
   pthread_mutex_init(&new_file_cache->mutex, NULL);

   // initialize scratch variables
   new_file_cache->pinned_list_match = 
       (file_t**)malloc(sizeof(file_t*) * max_cache_entries);
   new_file_cache->unpinned_file_names =
       (char **)malloc(sizeof(char*) * max_cache_entries);
   return new_file_cache;
}

// iterates through all lists and writes dirty data to files
void filecachedestroy(file_cache_t *cache)
{
   pthread_mutex_lock(&cache->mutex);

   // flush pinned list to files
   file_t *file,*prev;
   for(file = cache->pin_list_head; file != NULL; ){
	flush_file(file);
        prev = file;
        file = file->next;
        free(prev);
        cache->pin_list_count--;
   }
   
   // flush unpinned list to files
   for(file = cache->unpin_list_head; file != NULL; file = file->next){
       remove_unpin_file(cache, file);
   }
   free(cache->pinned_list_match);
   free(cache->unpinned_file_names);
   pthread_mutex_unlock(&cache->mutex);
   pthread_mutex_destroy(&cache->mutex);
   free(cache);
}
/*return the file_t that matches the filname else NULL*/
static 
file_t *get_file_present_in_list(file_t *file_t_list, char *filename)
{
        while(file_t_list != NULL){
            if(strcmp(file_t_list->file_name, filename) == 0){
                return file_t_list;
            }
            file_t_list=file_t_list->next;
        }
        return file_t_list;
}

// generic linked list function to add file_t to list of file_t 
// returns:- success or failure
static
bool add_to_list(file_t **file_list, file_t *file)
{
    if (file_list == NULL || file == NULL) return false;
    file_t *temp = *file_list;
    *file_list = file;
    file->next = temp;
    return true;
}

// generic linked list function to remove file_t from list of file_t 
// returns:- success or failure
static
bool remove_from_list(file_t **file_list, file_t *file)
{
    file_t *prev;
    if (file_list == NULL || file == NULL) return false;

    if (*file_list == file){
        *file_list = file->next;
        return true;
    }
    
    file_t *temp = *file_list;
    while(temp != NULL){
        prev = temp;
	temp=temp->next;
	if(temp == file){
       	    prev->next = file->next;
	    return true;
	}
    }
    return false;
}

/* checks if file present in unpinned list
 * removes from unpinned list 
 * adds back to pinned_list 
 * increments pin_count
 * adjusts the pin_list_count and unpin_list_count in file_cache
 *
 * returns:-
 * file that was pinned or NULL*/
static
file_t *add_back_to_pinned_list(file_cache_t *cache, char *file_to_be_pinned)
{
        file_t *file = get_file_present_in_list(cache->unpin_list_head,
                file_to_be_pinned);

        if (file == NULL) return NULL;

        add_to_list(&cache->pin_list_head, file);
        remove_from_list(&cache->unpin_list_head, file);
        cache->pin_list_count++;
        cache->unpin_list_count--;
	file->pin_count++;
        return file;
}

/* checks if file present in unpinned list
 * removes from pinned list if pin_count is 1 
 * adds back to unpinned_list 
 * decrements pin_count
 * adjusts the pin_list_count and unpin_list_count in file_cache
 *
 * returns:-
 * file that was unpinned/decremented or NULL*/
static
file_t *add_back_to_unpinned_list(file_cache_t *cache, char *file_to_be_unpinned)
{
        file_t *file = get_file_present_in_list(cache->pin_list_head,
                file_to_be_unpinned);

        if (file == NULL) return NULL;

	file->pin_count--;
	if(file->pin_count > 0){
	     return file;
	}

        add_to_list(&cache->unpin_list_head, file);
        remove_from_list(&cache->pin_list_head, file);
        cache->unpin_list_count++;
        cache->pin_list_count--;
        return file;
}

/*create_new_file() 
* returns a new file_t and assign the filename
*/
static
file_t *create_new_file_t(const char *filename)
{
	file_t *new_file = malloc(sizeof(file_t));
	bzero(new_file, sizeof(file_t));
	
	new_file->pin_count = 1;
	new_file->file_name = filename;	
	return new_file;
}

//Note:- we will just decrement count of pin file in unpin function
// but during pinning, if we do not have space, we will see which files are having pin count of 0 and then replace them

// pins new file:- Also does the background task of reading from file and filling the cache
//		   Creates the file and fills it with 10KB zeros 
static
void pin_new_file(file_cache_t *cache, file_t *file)
{

	FILE *fp = fopen(file->file_name, "r");
	char fdata[FILE_SIZE] = {0,};
	if (fp == NULL){
		fp = fopen(file->file_name,"w+");
		fwrite(fdata, sizeof(fdata[0]), sizeof(fdata)/sizeof(fdata[0]), fp);
	}
	else{
		fread(fdata, sizeof(fdata[0]), sizeof(fdata)/sizeof(fdata[0]), fp);
	}
	memcpy(file->data, fdata, FILE_SIZE);
	fclose(fp);
	add_to_list(&cache->pin_list_head, file);
	
	cache->pin_list_count++;
}

// write file back to disk/storage if file_t data is modified 
static bool
flush_file(file_t *file)
{
	if (file->is_dirty == true){
		char fdata[FILE_SIZE] = {0,};
		memcpy(fdata, file->data, FILE_SIZE);
		FILE *fp = fopen(file->file_name,"w+");
		fwrite(fdata, sizeof(fdata[0]), sizeof(fdata)/sizeof(fdata[0]), fp);
		fclose(fp);
	}
	return file->is_dirty;
}

// file_t which is already unpinned is removed from cache after flushing content to file
static
void remove_unpin_file(file_cache_t *cache, file_t *file)
{

	remove_from_list(&cache->unpin_list_head, 
				file);

	flush_file(file);
	free(file);
	
	cache->unpin_list_count--;
}

/*By priority:- tries to create a new file_t
 * if not possible then replaces unpinned fie_t*/
static
bool create_pinned_file(file_cache_t *cache, const char *file_name)
{
	int free_slots = cache->max_entries - cache->pin_list_count -
					cache->unpin_list_count;

	if(free_slots > 0){
		file_t *new_file = create_new_file_t(file_name);
		pin_new_file(cache, new_file);
		return true;
	}

	if (cache->unpin_list_count > 0){
		remove_unpin_file(cache, 
				cache->unpin_list_head);
		
		file_t *new_file = create_new_file_t(file_name);
		pin_new_file(cache, new_file);
		
		return true;
	}
	return false;
}

void filecachepinfiles(file_cache_t *cache,
                      const char **files,
                      int num_files)
{
    int file_already_pinned_count = 0;
    int  i = 0;
    file_t *file_t_in_pinned_list = NULL;
    file_t *file_t_in_unpinned_list = NULL;
    int total_unused_file_t;
    int file_not_pinned_count = 0;
starting_new:    
    pthread_mutex_lock(&cache->mutex);
    file_already_pinned_count = 0;
    file_t_in_pinned_list = NULL;
    file_not_pinned_count = 0;

	// handle already pinned files: add them to pinned_list_match
	// add the filenames that dont match to unpinned_file_names
    for(i=0; i < num_files; i++){
        file_t_in_pinned_list = 
                    get_file_present_in_list(cache->pin_list_head, files[i]);
       if (file_t_in_pinned_list != NULL){             
            cache->pinned_list_match[file_already_pinned_count] =
                                                       file_t_in_pinned_list;
            file_already_pinned_count++;

        }
       else{
            cache->unpinned_file_names[file_not_pinned_count] =
                                                            files[i];
            file_not_pinned_count++;
       }
    }

	// since now we know how many are already pinned and 
	// how manyunpinned files we need to pin 
   	// we take stock here if it is possible to pin all remaining files
	// if possible then we go ahead and complete all pinning
	// else we just release lock/yield for other threads
    total_unused_file_t = cache->max_entries - cache->pin_list_count;
    if(file_not_pinned_count > total_unused_file_t){
	pthread_mutex_unlock(&cache->mutex);
        sched_yield();
        goto starting_new;
    }

    /* If we are here, it means we have the resources to pin all requested files
     * */

    // Increment pin_count of already pinned files
    for (i = 0;i < file_already_pinned_count; i++){
        cache->pinned_list_match[i]->pin_count++;
    }

    // add back to pinned list if files present in unpinned list
    char **files_new_pinned = malloc(sizeof(char*) * file_not_pinned_count);
    bzero(files_new_pinned, sizeof(char*) * file_not_pinned_count);
    int files_new_pinned_count = 0;

    for (i = 0; i < file_not_pinned_count; i++){
        file_t_in_unpinned_list = add_back_to_pinned_list(
                cache, cache->unpinned_file_names[i]);
	// if file_t_in_unpinned_list == NULL, then we need to replace some unpinned files
	// or if possible create new file_t
        if (file_t_in_unpinned_list == NULL){
            files_new_pinned[files_new_pinned_count++] =
                cache->unpinned_file_names[i];
        }
    }

    // create_pinned_file() creates/replaces unpinned files
    for (i=0; i<files_new_pinned_count; i++){
        create_pinned_file(cache, files_new_pinned[i]);
    }
    free(files_new_pinned);

    pthread_mutex_unlock(&cache->mutex);
}

// adds back pinned files to unpinned file list
// if pin count is more than one, just decrement pin_count
void filecacheunpinfiles(file_cache_t *cache,
                            const char **files,
                            int num_files)
{
    pthread_mutex_lock(&cache->mutex);
    int i =0;
    for (i=0; i < num_files; i++){
        add_back_to_unpinned_list(
                cache->pin_list_head, files[i]);
    }
    pthread_mutex_unlock(&cache->mutex);
}

// just find the file and return the data pointer
// nothing to worry here since we dont worry about synchronizing and non-pinned files
const char *filecachefiledata(file_cache_t *cache, const char *filename)
{
     file_t *file = get_file_present_in_list(cache->pin_list_head, filename);
     return (file != NULL)?file->data:NULL;
}

// just find the file and return the data pointer
// nothing to worry here since we dont worry about synchronizing and non-pinned files
char *filecachemutablefiledata(file_cache_t *cache, const char *filename)
{
    file_t * file = get_file_present_in_list(cache->pin_list_head, filename);
    if (file != NULL){
	pthread_mutex_lock(&cache->mutex);
        file->is_dirty = true;
        pthread_mutex_unlock(&cache->mutex);
        return file->data;
    }
    return NULL;
}
