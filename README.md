# File System using FUSE
Open the walkthrough pdf file [here](https://github.com/freecode23/fuse-file-systems/blob/main/walkthrough.pdf) for in-depth walkthrough of this project.

## 1. Install Required Packages.
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
2. Create a mount point called `fs`, that is, create a directory where a file system will be attached:
```
mkdir fs
```

4. Show the disk space usage of the file system on which that directory is mounted:
```
df fs
```
You should see:
```
 Filesystem     1K-blocks    Used Available Use% Mounted on
 /dev/sda1        7092728 4536616   2172780  68% /
```

5. Mounts the file system from `test.img` to the mount point `fs` using the `lab5fuse` program:
```
./lab5fuse -image test.img fs
```

You should see that we have now associated `fs` with `test.img` if we run `df fs` again:
---> Insert new fs here.

### Note:
- When you mount `test.img` to `fs`, the operating system makes the contents of `test.img` accessible through the directory structure starting at `fs`. Essentially, `fs` becomes the root directory of the file system contained in `test.img`.

## 3. Test the Commands.

We can now test our new file system with the common linux commands:
```
cd
mkdir
touch
cat
ls
rm
etc.
```
Here are some sample demo of creating a directory, create a file within that directory.
Write to that file, and read back from that file.
Notice that all of these command is associated with the fuse file system!
### Demo1: Creating a a new directory and check that this directory is using our fuse filesystem.
<img width="1177" alt="Screenshot 2024-03-17 at 3 33 12 PM" src="https://github.com/freecode23/fuse-file-systems/assets/67333705/e117f42d-37e3-4f58-a33f-16945f3a4794">

### Demo2: Create a new text file, write and read them back using `cat`.
<img width="658" alt="Screenshot 2024-03-17 at 3 23 31 PM" src="https://github.com/freecode23/fuse-file-systems/assets/67333705/cebc1bcd-a812-4425-a9c9-c5c602ccdb42">

## 4. Unmount the filsystem from `fs`
you can umount your fs5600 by:
```
fusermount -u fs
```
