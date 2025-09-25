# NYUFile - FAT32 File Recovery Tool

A command-line utility for analyzing and recovering files from FAT32 file systems. This tool is designed for educational purposes as part of an NYU Operating Systems lab assignment.

## Features

- **File System Information**: Display FAT32 boot sector information including number of FATs, bytes per sector, sectors per cluster, and reserved sectors
- **Directory Listing**: List all files and directories in the root directory of a FAT32 file system
- **File Recovery**: Recover deleted files from FAT32 file systems with optional SHA-1 verification
- **Contiguous File Recovery**: Recover files that are stored in contiguous clusters

## Prerequisites

- GCC compiler
- OpenSSL development libraries (for SHA-1 hashing)
- Unix-like operating system (Linux, macOS, etc.)

## Building

To compile the project, use the provided Makefile:

```bash
make
```

This will create the `nyufile` executable.

To clean up build artifacts:

```bash
make clean
```

## Usage

```bash
./nyufile <disk_image> <options>
```

### Options

- `-i` - Print file system information
- `-l` - List the root directory contents
- `-r <filename> [-s <sha1>]` - Recover a contiguous file (optionally with SHA-1 verification)

### Examples

1. **Display file system information:**
   ```bash
   ./nyufile fat32.disk -i
   ```

2. **List root directory:**
   ```bash
   ./nyufile fat32.disk -l
   ```

3. **Recover a deleted file:**
   ```bash
   ./nyufile fat32.disk -r deleted_file.txt
   ```

4. **Recover a file with SHA-1 verification:**
   ```bash
   ./nyufile fat32.disk -r deleted_file.txt -s da39a3ee5e6b4b0d3255bfef95601890afd80709
   ```


## How It Works

### File System Analysis
The tool reads and parses the FAT32 boot sector to extract key information about the file system structure, including:
- Number of FATs
- Bytes per sector
- Sectors per cluster
- Number of reserved sectors

### Directory Traversal
The tool navigates through the FAT32 root directory by:
1. Calculating the root directory cluster location
2. Following the FAT chain to traverse all root directory clusters
3. Parsing directory entries to extract file information

### File Recovery
The recovery process involves:
1. **File Discovery**: Searching for deleted files (marked with 0xE5 in the first byte of the filename)
2. **FAT Chain Reconstruction**: Rebuilding the file allocation table entries for the recovered file
3. **SHA-1 Verification**: Optionally verifying file integrity using SHA-1 hashes
4. **Directory Entry Restoration**: Restoring the first byte of the filename to make the file visible again

### Data Structures

The tool uses two main data structures:

- **`BootEntry`**: Represents the FAT32 boot sector with all necessary file system parameters
- **`DirEntry`**: Represents a FAT32 directory entry containing file metadata

## Technical Details

- **Memory Mapping**: Uses `mmap()` for efficient file system access
- **FAT32 Support**: Implements FAT32-specific features like 32-bit cluster addressing
- **SHA-1 Hashing**: Uses OpenSSL's SHA-1 implementation for file verification
- **Error Handling**: Comprehensive error checking for file operations and memory allocation

## Limitations

- Only supports FAT32 file systems
- Limited to root directory operations
- Only supports contiguous file recovery
- No support for subdirectories

## File Structure

```
nyufile/
├── nyufile.c      # Main source code
├── Makefile       # Build configuration
├── fat32.disk     # Sample FAT32 disk image
└── README.md      # This file
```

## Educational Context

This project demonstrates key operating systems concepts including:
- File system internals and data structures
- Low-level disk I/O operations
- Memory mapping techniques
- File system recovery algorithms
- FAT (File Allocation Table) traversal

## License

This project is part of an educational assignment and is intended for learning purposes only.
