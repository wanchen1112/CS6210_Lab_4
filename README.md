# CS6210_Lab_4
Recoverable Virtual Memory (group assignment)

## Overview
In this project, the recoverable virtual memory (RVM) is implemented based on the paper [Lightweight Recoverable Virtual Memory](http://dl.acm.org/citation.cfm?id=168631&dl=ACM&coll=DL&CFID=621058791&CFTOKEN=91409860). The LRVM mechanism is to maintain data persistency and improve the efficiency by updating only part of the file (logs).

## Background
- The procedure to perform a file access (read or write) is called **transaction**. When the transaction starts, the LRVM system will generate a *undo record* which is a memory copy to save the origin copy of the file section. During the transaction, the data to be updated is write to the *redo log* which is a log file that contains all the updated data for the file segments. When the transaction is finished (committed), the redo log will be updated to the disk. 
- If the transaction is failed (software crash or power failure before commit), then the ```abort()``` function is called. The file segment will be restored to the original copy.
- In order to maintain the usability, the redo log will be merged to corresponding original files periodically. This procedure is done by calling ```truncate()``` function. 

## API description
- ```rvm_t rvm_init(const char *directory)```  
The initialization of the LRVM system. It will allocate necessary memory sections to the log file and data structures. Besides, the directory for saving the logs and data segments (files) have to be specified by calling this function.
- ```void *rvm_map(rvm_t rvm, const char *segname, int size_to_create)```   
Map is a function that mapping a data segment on disk to a memory region. As the arguments shown in the function, in addition to the data structure ```rvm```, the segment name and the size to be created have to be specified in the map function. It will return the pointer of the allocated memory address if the map is successful, otherwise ```-1``` will be the return value. Note that for a data segment, the map procedure can be performed once during a transaction.  
- ```void rvm_unmap(rvm_t rvm, void *segbase)```  
The mapping of a data segment to a memory region is removed. However the mapped memory is not released.
- ```void rvm_destroy(rvm_t rvm, const char *segname)```  
The mapped region is destroyed completely, including removing the mapping and releasing the allocated memory.
- ```trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases)```  
The transaction is started by calling this function. Relevant data structures will be allocated memory.
- ```void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size)```  
The undo record is updated and the redo log is updated based on the size and offset.
- ```void rvm_commit_trans(trans_t tid)```  
The transaction is finished. All the modification in the redo log (on the memory) will be written to the disk. The relevant data structure and allocated memory will be released in this function.
- ```void rvm_abort_trans(trans_t tid)```  
If the transaction is failed due to power failure or software crash, the function is called to restore the data segment.
- ```void rvm_truncate_log(rvm_t rvm)```  
All the contents in the redo log will be updated to the corresponding data segments.

## Notes
- For looking up the segments, we use the search list APIs provided by the instructors. We can search a data segment by its name or base address in a transaction. 
- In ```\doc\readme.txt```, there are some questions from the instructors, and our explanation for those questions are provided in the file.
