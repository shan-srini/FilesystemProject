# Filesystem Project
A 1mb ext style filesystem with rollback and mark-and-sweep garbage collection, which I implemented for my CS3650 Computer Systems class. Design layout of pages is documented in DESIGN file

### How to use
1. Run two shells to use one to mount filesystem
2. make; make mount ( *on the shell which you chose to mount* )
3. cd mnt ( *on other shell* )
4. make unmount

### Rollbacks
note: DISKIMAGE if you ran make, will be disk0.cow by default
#### View most recent versions
1. cd to root dir ( *project, not mnt* )
2. ./nufstool versions DISKIMAGE
( *Currently set to show 7 most recent versions, can increase this in versions.c* )

#### Rollback
1. cd to root dir ( *of project, not mnt* )
2. ./nufstool rollback DISKIMAGE VERSION\_NUM
