#### Readme ####

- Language: C
- Environment: MaC OSX 10.9.5 and Ubuntu 12.xx

- This project uses the steque and seqsrchst APIs from Udacity Downloadables. 
  The functions and data structures are declared in rvm.c and rvm_internal.h

- How log files are used to accomplish persistency and transaction semantics.

* Undo record:

	1. In rvm_about_to_modify(), the undo record which is an in-memory log, is created as a copy of the segment, from the address of <segbase+offset> to <segbase+offset+size>.
	
	2. If the transaction is aborted, the memory region for the segments which are planned to be modified will be restored by replicating the content of undo log.

* Redo log:

	1. The redo log is an on-disk copy which stores the modified data of the segments. 

	2. During the transaction, the modification will be stored on memory temporarily. Once the function rvm_commit_trans() is called, which means all the modification will be stored on the redo log.

	3. The commit process is only committing the redo log (updated data) to the disk instead of writing the whole segments to disk, which consumes much time. Therefore, the data persistency can be remained by shrinking the time spent on writing to disk (light weight transactions).


- What is written in them? How do the files get cleaned up, so that they do not expand indefinitely?

* Undo record:

	1. The undo record stores the original data content of the segment from <segbase+offset> to <segbase+offset+size>.

	2. The records will be cleaned up when the transaction is aborted, or when the transaction is finished (commit). The clean up process is accomplished by free the memory allocated to the undo record.


* Redo log:

	1. The redo log stores the modification information, including the segment name, the offset and size of the modified data, and the data to be updated. 

	2. The redo log will be stored on disk until rvm_truncate_log() is called. The truncate process should be called periodically to make sure that the log does not expand indefinitely. 

