#include "rvm.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

//#define DEBUG_ON

#ifdef DEBUG_ON
#define DEBUG_MSG(...) fprintf (stderr, __VA_ARGS__)
#else
#define DEBUG_MSG(...) 
#endif

/************** The RVM APIs **************/

void get_file_path(char* directory, char* file_name, char* path){

	strncpy(path, directory, PREFIX_SIZE);
	strcat(path, "/");
	strncat(path, file_name, FILE_NAME_SIZE);
}

/* compare if the segnames are the same */
int segname_eq(seqsrchst_key a, seqsrchst_key b){
	return strcmp((char*)a, (char*)b) == 0;
}

/* compare if the segbases are the same */
int segbase_eq(seqsrchst_key a, seqsrchst_key b){
	return (void*)a == (void*)b;
}

void get_updated_data(rvm_t rvm, void *segbase, char *segname){
	int log_size;
	int fd;
	char *directory;

	char read_segname[FILE_NAME_SIZE];

	int offset;
	int size;
	void *data;

	char path[PREFIX_SIZE + FILE_NAME_SIZE + 1];
	struct stat file_state = {0};

	
	DEBUG_MSG("... get_updated_data()\n");
	
	directory = rvm->prefix;
	get_file_path(directory, "redo.log", path);

	if(stat(path, &file_state) == -1){
		perror("redo_log does not exist\n");
		return;
	}

	log_size = file_state.st_size;
	fd= open(path, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	/* read all segment data from the redo log in disk */
	while(log_size > 0){

		read(fd, read_segname, FILE_NAME_SIZE);
		read(fd, &offset, sizeof(int));
		read(fd, &size, sizeof(int));

		/* apply the log on segment if they have the same segname */
		if(strcmp(read_segname, segname) == 0){

			data = (void*)calloc(1, size);
			read(fd, data, size);
	
			strcpy((segbase + offset), data);
			free(data);
		}
		else{
			lseek(fd, size, SEEK_CUR);
		}
		log_size = log_size - sizeof(segentry_t) - 2*sizeof(int) - size;
	}

	close(fd);
}

/*
   Initialize the library with the specified directory as backing store.
 */
rvm_t rvm_init(const char *directory){
	rvm_t rvm;
	char path[PREFIX_SIZE + FILE_NAME_SIZE + 1];
	struct stat file_state = {0};

	DEBUG_MSG(">>> rvm_init()\n");

	rvm = (rvm_t)calloc(1, sizeof(struct _rvm_t));
	redo_log = (redo_t)calloc(1, sizeof(struct _redo_t));
	
	strcpy(rvm->prefix, directory);

	if(stat(directory, &file_state) == -1){
		if(mkdir(directory, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1){
			perror("File to create the new directory!\n");
			exit(-1);
		}
	}

	get_file_path(directory, "redo.log", path);

	/* create the redo log file on disk */
	rvm->redofd = open(path, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(rvm->redofd < 0){
		perror("Error creating the redo log file\n");
		exit(-1);
	}
	close(rvm->redofd);

	/* initiate search maps */
	seqsrchst_init(&(rvm->segst), segname_eq);
	seqsrchst_init(&segname_segment, segname_eq);
	seqsrchst_init(&segbase_segment, segbase_eq);

	return rvm;
}

/*
   map a segment from disk into memory. If the segment does not already exist, 
   then create it and give it size size_to_create. 
   If the segment exists but is shorter than size_to_create, 
   then extend it until it is long enough. 
   It is an error to try to map the same segment twice.
 */
void *rvm_map(rvm_t rvm, const char *segname, int size_to_create){
	
	segment_t seg;
	char seg_path[PREFIX_SIZE + FILE_NAME_SIZE + 1];
	int seg_fd;
	struct stat seg_file_state = {0};
	int seg_size = 0;

	char *data;
	
	DEBUG_MSG(">>> rvm_map()\n");

	if(seqsrchst_contains(&(rvm->segst), segname)){
		seg = seqsrchst_get(&segname_segment, segname);

		DEBUG_MSG("mapping for %s exists...\n", segname);

		if(seg->mapped == 1){			

			if(seg->size != size_to_create){

				DEBUG_MSG("... mapping is existing, reallocate the memory for segment\n");

				seqsrchst_delete(&segbase_segment, seg->segbase);
				seqsrchst_delete(&(rvm->segst), segname);

				/* reallocate a larger/smaller size for the segment */
				seg->segbase = realloc(seg->segbase, size_to_create);

				if(seg->segbase == NULL){

					printf("No sufficient space for the segment... exiting...\n");
					return (void *)-1;	
				}

				get_updated_data(rvm, seg->segbase, segname);

				seqsrchst_put(&(rvm->segst), segname, seg->segbase);
				seqsrchst_put(&segbase_segment, seg->segbase, seg);
				seg->size = size_to_create;
			}
			else{
				DEBUG_MSG("... mapping is existing, size is same or lager\n");
				return (void *)-1;		
			}	
		}
		
		else{

			DEBUG_MSG("... unmapped, but segment is still on memory\n");
			seg->mapped = 1;
			if(seg->size < size_to_create){
				seqsrchst_delete(&segbase_segment, seg->segbase);
				seqsrchst_delete(&(rvm->segst), segname);

				seg->segbase = realloc(seg->segbase, size_to_create);

				get_updated_data(rvm, seg->segbase, segname);

				seqsrchst_put(&(rvm->segst), segname, seg->segbase);
				seqsrchst_put(&segbase_segment, seg->segbase, seg);
				seg->size = size_to_create;
			}
		}

	}
	else{

		DEBUG_MSG("... No mapping for segment, allocate a memory region for the segment\n");
		
		/* create a new segment in memory */
		seg = (segment_t)calloc(1, sizeof(struct _segment_t));
		strcpy(seg->segname, segname);
		seg->size = size_to_create;
		seg->segbase = (void*)malloc(size_to_create);

		/* check if the external segment exist or not */
		if(stat(seg_path, &seg_file_state) != -1){ 

			DEBUG_MSG("... External segment exists... read from disk\n");
		
			get_file_path(rvm->prefix, segname, seg_path);			

			seg_fd = open(seg_path, O_RDONLY , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			seg_size = seg_file_state.st_size;

			if(size_to_create < seg_size){
				printf("Size to create is smaller than the external segment size... exit...\n");
				close(seg_fd);
				return (void *)-1;
			}

			data = calloc(1, seg_size);

			read(seg_fd, data, seg_size);
			memcpy(seg->segbase, data, seg_size);
			free(data);
			close(seg_fd);

			DEBUG_MSG("... Read file: %s from disk done\n", segname);

		}
		else{
			DEBUG_MSG("... External segment is not established\n");
		}

		/* read the updated data from the redo log */
		get_updated_data(rvm, seg->segbase, segname);

		seg->mapped = 1;

		steque_init(&(seg->mods));

		seqsrchst_put(&(rvm->segst), segname, seg->segbase);
		seqsrchst_put(&segname_segment, segname, seg);
		seqsrchst_put(&segbase_segment, seg->segbase, seg);

	}

	return seg->segbase;
}

/*
   unmap a segment from memory.
 */
void rvm_unmap(rvm_t rvm, void *segbase){
	segment_t seg;

	DEBUG_MSG(">>> rvm_unmap()\n");

	if(seqsrchst_contains(&segbase_segment, segbase) == 0){
		perror("Segment Not Found\n");
	}
	else{
		seg = seqsrchst_get(&segbase_segment, segbase);
		if(seg->mapped == 0)
			perror("Segment already unmmaped\n");
		else
			seg->mapped  = 0;
	}	
}

/*
   destroy a segment completely, erasing its backing store. This function should not be called on a segment that is currently mapped.
 */
void rvm_destroy(rvm_t rvm, const char *segname){
	void *segbase;
	segment_t seg;

	DEBUG_MSG(">>> rvm_destroy()\n");

	if(seqsrchst_contains(&(rvm->segst), segname)){
		seg = seqsrchst_get(&segname_segment, segname);
		if(seg->mapped == 1){
			perror("Segment is currently mapped\n");
			return;
		}
		else{
			segbase = seg->segbase;
			seqsrchst_delete(&(rvm->segst), segname);
			seqsrchst_delete(&segname_segment, segname);
			seqsrchst_delete(&segbase_segment, segbase);
			if(segbase != NULL)
				free(segbase);
			if(seg != NULL)
				free(seg);
		}
	}
	else{
		DEBUG_MSG("seg: %s does not exist ... destroy failed\n", segname);
		return;
	}
}

/*
   begin a transaction that will modify the segments listed in segbases. If any of the specified segments is already being modified by a transaction, then the call should fail and return (trans_t) -1. Note that trant_t needs to be able to be typecasted to an integer type.
 */
trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases){
	int i, j;
	segment_t segs[numsegs];
	trans_t tid;

	DEBUG_MSG(">>> rvm_begin_trans()\n");

	for (i = 0; i < numsegs; ++i)
	{
		if(seqsrchst_contains(&segbase_segment, segbases[i]) == 0){
			perror("Segment not found in transaction\n");
			exit(-1);
		}
		segs[i] = seqsrchst_get(&segbase_segment, segbases[i]);
		assert(segs[i]->mapped == 1);
		if(segs[i]->cur_trans != NULL){
			perror("Segment is already being modified\n");
			return (trans_t)-1;
		}
	}

	/* Established the stucture for transaction (trans_t) */
	tid = (trans_t)calloc(1, sizeof(struct _trans_t));
	tid->numsegs = numsegs;
	tid->segments = calloc(numsegs, sizeof(tid->segments));
	tid->rvm = rvm;


	/* assign the current transaction to the segments, 
	   and the segment structure structure */
	for (j = 0; j < numsegs; ++j)
	{
		assert(segs[j]->mapped == 1 && segs[j]->cur_trans == NULL);
		
		segs[j]->cur_trans = tid;
		tid->segments[j] = segs[j];
	}

	return tid;
}


/*
   declare that the library is about to modify a specified range of memory in 
   the specified segment. The segment must be one of the segments specified in 
   the call to rvm_begin_trans. Your library needs to ensure that the old memory 
   has been saved, in case an abort is executed. 
   It is legal call rvm_about_to_modify multiple times on the same memory area.
 */
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size){
	segment_t seg;
	mod_t *mod;

	DEBUG_MSG(">>> rvm_about_to_modify()\n");

	seg = seqsrchst_get(&segbase_segment, segbase);
	
	if(seg->cur_trans != tid){
		perror("Segment is not ready to be modified\n");
		exit(-1);
	}

	/* store the info of offset and size */
	mod = (mod_t*)calloc(1, sizeof(mod_t));
	mod->offset = offset;
	mod->size = size;

	/* assume that only one modification for each segment in each transaction */
	/* allocate a memory region for the undo log */
	mod->undo = (void*)calloc(1, size);
	memcpy(mod->undo, segbase+offset, size);

	/* put the undo log to the queue */
	steque_push(&(seg->mods), mod);

	redo_log->numentries++;

	DEBUG_MSG("... Preparing the entries in redo log structure... \n");

	/* populate the seg entry and add the segentry into redo log in memory */
	if(redo_log->numentries == 1){
		redo_log->entries = (segentry_t*)calloc(1, sizeof(segentry_t));
		strcpy(redo_log->entries->segname, seg->segname);
		redo_log->entries->segsize = seg->size;
		redo_log->entries->updatesize = size;
		redo_log->entries->numupdates++;
		redo_log->entries->offsets = (int*)calloc(1, sizeof(int));
		redo_log->entries->offsets[0] = mod->offset;
		redo_log->entries->sizes = (int*)calloc(1, sizeof(int));
		redo_log->entries->sizes[0] = mod->size;
	
	}
	else{
		redo_log->entries = (segentry_t*)realloc(redo_log->entries, redo_log->numentries* sizeof(segentry_t));
		strcpy(redo_log->entries[redo_log->numentries - 1].segname, seg->segname);
		redo_log->entries[redo_log->numentries - 1].segsize = seg->size;
		redo_log->entries[redo_log->numentries - 1].updatesize = size;
		redo_log->entries[redo_log->numentries - 1].numupdates++;
		redo_log->entries[redo_log->numentries - 1].offsets = (int*)calloc(1, sizeof(int));
		redo_log->entries[redo_log->numentries - 1].offsets[0] = mod->offset;
		redo_log->entries[redo_log->numentries - 1].sizes = (int*)calloc(1, sizeof(int));
		redo_log->entries[redo_log->numentries - 1].sizes[0] = mod->size;
	}
}

/*
   commit all changes that have been made within the specified transaction. When the call returns, then enough information should have been saved to disk so that, even if the program crashes, the changes will be seen by the program when it restarts.
 */
void rvm_commit_trans(trans_t tid){
	int fd, i, j;
	int numsegs;
	segment_t *segs;
	void *segbase;
	segentry_t *entries;
	void *data;
	int data_size;
	int offset;
	mod_t *mod;
	char path[PREFIX_SIZE + FILE_NAME_SIZE + 1];
	int num_entries;

	DEBUG_MSG(">>> rvm_commit_trans()\n");

	if(redo_log->numentries == 0){
		printf("No data could be committed ... exit ...\n");
		return;
	}

	get_file_path(tid->rvm->prefix, "redo.log", path);
	fd= open(path, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	entries = redo_log->entries;
	num_entries = redo_log->numentries;
	numsegs = tid->numsegs;
	segs = tid->segments;

	/* copy changed data to redo log */
	for(j = 0 ; j <num_entries; j++){
		
		data_size = *(entries[j].sizes);
		offset = *(entries[j].offsets);
		data = (void*)calloc(1, data_size);
		segbase = seqsrchst_get(&(tid->rvm->segst), entries[j].segname);
		
		strncpy(data, (segbase + offset), data_size);
		entries[j].data = data;

		/* write the redo log to log file */
		DEBUG_MSG("... redo_log is writing for %s\n", (entries[j].segname));
		write(fd, (entries[j].segname), FILE_NAME_SIZE);
		write(fd, (entries[j].offsets), sizeof(int));
		write(fd, (entries[j].sizes), sizeof(int));
		write(fd, (entries[j].data), data_size);
	}
	
	close(fd);

	/* free the redo_log after write into the log file */
	for(j = 0; j< num_entries; j++){
		free(entries[j].offsets);
		free(entries[j].sizes);
	}
	
	free(redo_log->entries);
	redo_log->numentries = 0;

	/* finish the transaction and set pointer to NULL */
	for(i = 0; i <numsegs; i++){
		segs[i]->cur_trans = NULL;
	}
	
	free(tid);

	DEBUG_MSG("... deleting undo records ... \n");

	for(i = 0; i < numsegs; i++)
	{
		while(steque_isempty(&(segs[i]->mods)) != 1)
		{
			mod = steque_pop(&(segs[i]->mods));

			if(mod != NULL && mod->undo != NULL){
				free(mod->undo);
				free(mod);
			}
		}
	}

}

/*
   undo all changes that have happened within the specified transaction.
 */
void rvm_abort_trans(trans_t tid){
	int numsegs, i, offset, j, k;
	mod_t *mod;

	numsegs = tid->numsegs;

	for(i = 0; i < numsegs; i++){
		while(steque_isempty(&(tid->segments[i]->mods))!= 1){
			
			/* copy old data from undo to segment */
			mod = steque_pop(&(tid->segments[i]->mods));
			offset = mod->offset;
			strcpy(tid->segments[i]->segbase + offset, mod->undo);
		}
		/* delete undo record */
		if(mod != NULL && mod->undo != NULL){
			free(mod->undo);
			free(mod);
		}
	}


	/* delete the redo log for those changes */
	for(j = 0; j < redo_log->numentries; j++)
	{
		free(redo_log->entries[j].offsets);
		free(redo_log->entries[j].sizes);
	}
	
	free(redo_log->entries);
	redo_log->numentries = 0;

	/* remove the transaction for the segments */
	for(k = 0; k < numsegs; k++){
		tid->segments[k]->cur_trans = NULL;
	}
	free(tid);
}

/*
   play through any committed or aborted items in the log file(s) and shrink the log file(s) as much as possible.
 */
void rvm_truncate_log(rvm_t rvm){

	int log_fd, seg_fd, log_size;
	char log_path[PREFIX_SIZE + FILE_NAME_SIZE + 1];
	char seg_path[PREFIX_SIZE + FILE_NAME_SIZE + 1];

	char read_segname[FILE_NAME_SIZE];

	int offset;
	int size;
	void *data;

	struct stat log_file_state = {0};

	DEBUG_MSG(">>> rvm_truncate_log()\n");

	get_file_path(rvm->prefix, "redo.log", log_path);

	if(stat(log_path, &log_file_state) == -1){
		perror("redo_log does not exist\n");
		return;
	}

	log_size = log_file_state.st_size;
	log_fd= open(log_path, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	/* read the redo log and apply the content to the corresponding segments */
	while(log_size > 0){

		read(log_fd, read_segname, FILE_NAME_SIZE);
		read(log_fd, &offset, sizeof(int));
		read(log_fd, &size, sizeof(int));

		DEBUG_MSG("... segment name: %s, ", read_segname);
		DEBUG_MSG("offset: %d, ", offset);
		DEBUG_MSG("data size: %d, ", size);

		get_file_path(rvm->prefix, read_segname, seg_path);

		data = (void*)calloc(1, size);
		read(log_fd, data, size);
		DEBUG_MSG("data: %s\n", (char*)data);

		seg_fd= open(seg_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

		if(lseek(seg_fd, offset, SEEK_SET) != offset){
			printf("move to offset %d of %s is failed\n", offset, read_segname);
		}
		
		if(write(seg_fd, data, size) != size){
			printf("Fail to write file: %s\n", read_segname);
		}

		DEBUG_MSG("Write file %s is finished\n",read_segname);

		free(data);

		log_size = log_size - sizeof(segentry_t) - 2*sizeof(int) - size;

		close(seg_fd);
	}

	/* truncate the redo log on disk */
	ftruncate(log_fd, 0);

	close(log_fd);
}


/************** steque APIs **************/

void steque_init(steque_t *stequ){
  stequ->front = NULL;
  stequ->back = NULL;
  stequ->N = 0;
}


void steque_enqueue(steque_t* stequ, steque_item item){
  steque_node_t* node;

  node = (steque_node_t*) malloc(sizeof(steque_node_t));
  node->item = item;
  node->next = NULL;
  
  if(stequ->back == NULL)
    stequ->front = node;
  else
    stequ->back->next = node;

  stequ->back = node;
  stequ->N++;
}

void steque_push(steque_t* stequ, steque_item item){
  steque_node_t* node;

  node = (steque_node_t*) malloc(sizeof(steque_node_t));
  node->item = item;
  node->next = stequ->front;

  if(stequ->back == NULL)
    stequ->back = node;
  
  stequ->front = node;
  stequ->N++;
}

int steque_size(steque_t* stequ){
  return stequ->N;
}

int steque_isempty(steque_t *stequ){
  return stequ->N == 0;
}

steque_item steque_pop(steque_t* stequ){
  steque_item ans;
  steque_node_t* node;
  
  if(stequ->front == NULL){
    fprintf(stderr, "Error: underflow in steque_pop.\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
  }

  node = stequ->front;
  ans = node->item;

  stequ->front = stequ->front->next;
  if (stequ->front == NULL) stequ->back = NULL;
  free(node);

  stequ->N--;

  return ans;
}

void steque_cycle(steque_t* stequ){
  if(stequ->back == NULL)
    return;
  
  stequ->back->next = stequ->front;
  stequ->back = stequ->front;
  stequ->front = stequ->front->next;
  stequ->back->next = NULL;
}

steque_item steque_front(steque_t* stequ){
  if(stequ->front == NULL){
    fprintf(stderr, "Error: underflow in steque_front.\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
  }
  
  return stequ->front->item;
}

void steque_destroy(steque_t* stequ){
  while(!steque_isempty(stequ))
    steque_pop(stequ);
}


/************** sequential search MAP APIs ****************/


void seqsrchst_init(seqsrchst_t* st, 
		    int (*keyeq)(seqsrchst_key a, seqsrchst_key b)){
  st->first = NULL;
  st->N = 0;
  st->keyeq = keyeq;
}

int seqsrchst_size(seqsrchst_t* st){
  seqsrchst_node* node;

  for( node = st->first; node != NULL; node = node->next){
      printf("node adr: %p\n", node);
      printf("saved segment adr: %p\n", node->value);
}
 
  return st->N;
}

int seqsrchst_isempty(seqsrchst_t* st){
  return st->N == 0;
}

int seqsrchst_contains(seqsrchst_t* st, seqsrchst_key key){
  return seqsrchst_get(st, key) != NULL;
}

seqsrchst_value seqsrchst_get(seqsrchst_t* st, seqsrchst_key key){
  seqsrchst_node* node;

  for( node = st->first; node != NULL; node = node->next)
    if(st->keyeq(node->key, key))
      return node->value;
  
  return NULL;
}

seqsrchst_value seqsrchst_delete(seqsrchst_t* st, seqsrchst_key key){
  seqsrchst_value ans;
  seqsrchst_node* node;
  seqsrchst_node* prev;
  
  node = st->first;
  while(node != NULL && !st->keyeq(key, node->key) ){
    prev = node;
    node = node->next;
  }
  
  if (node == NULL)
    return NULL;

  ans = node->value;
  
  if( node == st->first) st->first = node->next;
  else prev->next = node->next;
  st->N--;
  free(node);

  return ans;    
}

void seqsrchst_put(seqsrchst_t* st, seqsrchst_key key, seqsrchst_value value){
  seqsrchst_node* node;

  node = (seqsrchst_node*) malloc(sizeof(seqsrchst_node));
  node->key = key;
  node->value = value;
  node->next = st->first;
  st->first = node;
  st->N++;
}

void seqsrchst_destroy(seqsrchst_t *st){
  seqsrchst_node* node;
  seqsrchst_node* prev;
  
  node = st->first;
  while(node != NULL){
    prev = node;
    node = node->next;
    free(prev);
  }  
}
