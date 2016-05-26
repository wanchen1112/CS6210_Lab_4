#ifndef __LIBRVMINT__
#define __LIBRVMINT__

#define PREFIX_SIZE 128
#define FILE_NAME_SIZE 128


/* steque structures */

typedef void* steque_item;

typedef struct steque_node_t{
  steque_item item;
  struct steque_node_t* next;
} steque_node_t;

typedef struct{
  steque_node_t* front;
  steque_node_t* back;
  int N;
}steque_t;

/* Initializes the data structure */
void steque_init(steque_t* stequ);

/* Return 1 if empty, 0 otherwise */
int steque_isempty(steque_t* stequ);

/* Returns the number of elements in the steque */
int steque_size(steque_t* stequ);

/* Adds an element to the "back" of the steque */
void steque_enqueue(steque_t* stequ, steque_item item);

/* Adds an element to the "front" of the steque */
void steque_push(steque_t* stequ, steque_item item);

/* Removes an element to the "front" of the steque */
steque_item steque_pop(steque_t* stequ);

/* Removes the element on the "front" to the "back" of the steque */
void steque_cycle(steque_t* stequ);

/* Returns the element at the "front" of the steque without removing it*/
steque_item steque_front(steque_t* stequ);

/* Empties the steque and performs any necessary memory cleanup */
void steque_destroy(steque_t* stequ);


/*******************************************************************/

/* steque structures */

typedef void* seqsrchst_key;
typedef void* seqsrchst_value;

typedef struct _seqsrchst_node{
  seqsrchst_key key;
  seqsrchst_value value;
  struct _seqsrchst_node* next;
} seqsrchst_node;

typedef struct{
  seqsrchst_node* first;
  int N;
  int (*keyeq)(seqsrchst_key a, seqsrchst_key b);
}seqsrchst_t;

void seqsrchst_init(seqsrchst_t* st, int (*keyeq)(seqsrchst_key a, seqsrchst_key b));
int seqsrchst_size(seqsrchst_t* st);
int seqsrchst_isempty(seqsrchst_t* st);
int seqsrchst_contains(seqsrchst_t* st, seqsrchst_key key);
seqsrchst_value seqsrchst_get(seqsrchst_t* st, seqsrchst_key key);
seqsrchst_value seqsrchst_delete(seqsrchst_t* st, seqsrchst_key key);
void seqsrchst_put(seqsrchst_t* st, seqsrchst_key key, seqsrchst_value value);
void seqsrchst_destroy(seqsrchst_t *st);

/********************************************************************************/

/* RVM structures */

/*For undo and redo logs*/
typedef struct mod_t{
  int offset;
  int size;
  void *undo;
} mod_t;


typedef struct _segment_t* segment_t;
typedef struct _redo_t* redo_t;

typedef struct _trans_t* trans_t;
typedef struct _rvm_t* rvm_t;
typedef struct segentry_t segentry_t;

struct _segment_t{
  char segname[FILE_NAME_SIZE];
  void *segbase;
  int size;
  int mapped;
  trans_t cur_trans;
  steque_t mods;
};

struct _trans_t{
  rvm_t rvm;          /*The rvm to which the transaction belongs*/
  int numsegs;        /*The number of segments involved in the transaction*/
  segment_t* segments;/*The array of segments*/
};

/*For redo*/
struct segentry_t{
  char segname[FILE_NAME_SIZE];
  int segsize;
  int updatesize;
  int numupdates;
  int* offsets;
  int* sizes;
  void *data;
};

/*The redo log */
struct _redo_t{
  int numentries;
  segentry_t* entries;
};

/* rvm */
struct _rvm_t{
  char prefix[FILE_NAME_SIZE];  /*The path to the directory holding the segments*/
  int redofd;                   /*File descriptor for the redo-log*/
  seqsrchst_t segst;            /*A sequential search dictionary mapping base pointers to segment names*/ 
};


redo_t redo_log;
seqsrchst_t segname_segment;
seqsrchst_t segbase_segment;


#endif
