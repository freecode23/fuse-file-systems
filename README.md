# File System using FUSE
Open the walkthrough pdf file [here](https://github.com/freecode23/fuse-file-systems/blob/main/walkthrough.pdf) for in-depth walkthrough of this project.

# 1. Install Required Packages.
```
sudo apt install check
sudo apt install libfuse-dev
sudo apt install zlib1g-dev
sudo apt install python2
```

## 2. Create the filesystem and the root directory.
1. Clone this repo and build the executable:
```
cd /fuse-file-systems
make 
```
You should see:
```
 cc -ggdb3 -Wall -O0   -c -o misc.o misc.c
 ...
```
2. Create a directory to serve as a mount point
Create a mount point called `fs`, that is, create a directory where a file system will be attached:
mkdir fs   

4. Show the disk space usage of the file system on which that directory is mounted:
df fs        
 Filesystem     1K-blocks    Used Available Use% Mounted on
 /dev/sda1        7092728 4536616   2172780  68% /


5. Mounts the file system from `test.img` to the mount point `fs` using the `lab5fuse` program:
./lab5fuse -image test.img fs
```
df fs
```

You should see that we have now associated fs with test.img

## Note:
- When you mount `test.img` to `fs`, the operating system makes the contents of `test.img` accessible through the directory structure starting at `fs`. Essentially, `fs` becomes the root directory of the file system contained in `test.img`.

## 4. Test the Commands.
```
1. cd fs
2. mkdir 
3. touch
4. cat
5. ls
6. rm 
```


## 5. Unmount the filsystem from `fs`
you can umount your fs5600 by:
```
fusermount -u fs
```