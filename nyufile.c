#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h> 
#include <errno.h>   
#include <openssl/sha.h>
#include <ctype.h>

#define SHA_DIGEST_LENGTH 20

#pragma pack(push,1)
typedef struct BootEntry {
  unsigned char  BS_jmpBoot[3];     // Assembly instruction to jump to boot code
  unsigned char  BS_OEMName[8];     // OEM Name in ASCII
  unsigned short BPB_BytsPerSec;    // Bytes per sector. Allowed values include 512, 1024, 2048, and 4096
  unsigned char  BPB_SecPerClus;    // Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB or smaller
  unsigned short BPB_RsvdSecCnt;    // Size in sectors of the reserved area
  unsigned char  BPB_NumFATs;       // Number of FATs
  unsigned short BPB_RootEntCnt;    // Maximum number of files in the root directory for FAT12 and FAT16. This is 0 for FAT32
  unsigned short BPB_TotSec16;      // 16-bit value of number of sectors in file system
  unsigned char  BPB_Media;         // Media type
  unsigned short BPB_FATSz16;       // 16-bit size in sectors of each FAT for FAT12 and FAT16. For FAT32, this field is 0
  unsigned short BPB_SecPerTrk;     // Sectors per track of storage device
  unsigned short BPB_NumHeads;      // Number of heads in storage device
  unsigned int   BPB_HiddSec;       // Number of sectors before the start of partition
  unsigned int   BPB_TotSec32;      // 32-bit value of number of sectors in file system. Either this value or the 16-bit value above must be 0
  unsigned int   BPB_FATSz32;       // 32-bit size in sectors of one FAT
  unsigned short BPB_ExtFlags;      // A flag for FAT
  unsigned short BPB_FSVer;         // The major and minor version number
  unsigned int   BPB_RootClus;      // Cluster where the root directory can be found
  unsigned short BPB_FSInfo;        // Sector where FSINFO structure can be found
  unsigned short BPB_BkBootSec;     // Sector where backup copy of boot sector is located
  unsigned char  BPB_Reserved[12];  // Reserved
  unsigned char  BS_DrvNum;         // BIOS INT13h drive number
  unsigned char  BS_Reserved1;      // Not used
  unsigned char  BS_BootSig;        // Extended boot signature to identify if the next three values are valid
  unsigned int   BS_VolID;          // Volume serial number
  unsigned char  BS_VolLab[11];     // Volume label in ASCII. User defines when creating the file system
  unsigned char  BS_FilSysType[8];  // File system type label in ASCII
} BootEntry;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct DirEntry {
  unsigned char  DIR_Name[11];      // File name
  unsigned char  DIR_Attr;          // File attributes
  unsigned char  DIR_NTRes;         // Reserved
  unsigned char  DIR_CrtTimeTenth;  // Created time (tenths of second)
  unsigned short DIR_CrtTime;       // Created time (hours, minutes, seconds)
  unsigned short DIR_CrtDate;       // Created day
  unsigned short DIR_LstAccDate;    // Accessed day
  unsigned short DIR_FstClusHI;     // High 2 bytes of the first cluster address
  unsigned short DIR_WrtTime;       // Written time (hours, minutes, seconds
  unsigned short DIR_WrtDate;       // Written day
  unsigned short DIR_FstClusLO;     // Low 2 bytes of the first cluster address
  unsigned int   DIR_FileSize;      // File size in bytes. (0 for directories)
} DirEntry;
#pragma pack(pop)

int main(int argc, char *argv[]) {
    struct BootEntry; 
    if (argc <= 2) {
        printf("Usage: ./nyufile disk <options>\n  -i                     Print the file system information.\n  -l                     List the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n");
        exit(1);
    }
    if (strcmp(argv[2], "-l") != 0 && strcmp(argv[2], "-i") != 0 && strcmp(argv[2], "-r") != 0) {
        printf("Usage: ./nyufile disk <options>\n  -i                     Print the file system information.\n  -l                     List the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n");
        exit(1);
    }
    if (strcmp(argv[2], "-r") == 0) {
        if (!argv[3]) {
            printf("Usage: ./nyufile disk <options>\n  -i                     Print the file system information.\n  -l                     List the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n");
            exit(1);
            }
    }

    if (strcmp(argv[2], "-i") == 0) {
        // Open file
        int fd = open(argv[1], O_RDWR);
        if (fd == -1)
            exit(1);

        // Get file size
        struct stat sb;
        if (fstat(fd, &sb) == -1)
            exit(1);

        // Map file into memory
        char *addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED)
            exit(1);
        
        BootEntry *boot_addr = (BootEntry *)(addr);
        // DirEntry *dir_addr = (DirEntry *)(addr + 0x5000);
        printf("Number of FATs = %d\nNumber of bytes per sector = %d\nNumber of sectors per cluster = %d\nNumber of reserved sectors = %d\n", 
        boot_addr->BPB_NumFATs, boot_addr->BPB_BytsPerSec, boot_addr->BPB_SecPerClus, boot_addr->BPB_RsvdSecCnt);

    }
    if (strcmp(argv[2], "-l") == 0) {
         // Open file
        int fd = open(argv[1], O_RDWR);
        if (fd == -1)
            exit(1);

        // Get file size
        struct stat sb;
        if (fstat(fd, &sb) == -1)
            exit(1);

        // Map file into memory
        char *addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED)
            exit(1);
        
        BootEntry *boot_addr = (BootEntry *)(addr);

        int root_clus = boot_addr->BPB_RootClus;
        // now I have to find the root directory 
        // Then I need to find the first data sector 
        // Then based on the root cluster I calculate the first root directory sectory 
        int  fat_size = boot_addr ->BPB_FATSz32;
        int sec_count = boot_addr ->BPB_RsvdSecCnt;
        int fats = boot_addr -> BPB_NumFATs;

        int first_sec = (fats * fat_size) + sec_count;
        int sec_per_clus = boot_addr->BPB_SecPerClus;
        int root_sec = ((root_clus - 2) * sec_per_clus) + first_sec;
        int bytes_per_sec = boot_addr->BPB_BytsPerSec;

        int offset = root_sec * bytes_per_sec;
        int entries_per_sector = bytes_per_sec / sizeof(DirEntry);

        int total = 0;
        int current_clus = root_clus;

        while (current_clus < 0x0FFFFFF8) {
            for (int sec = 0; sec < sec_per_clus; sec++) {
                DirEntry *dir = (DirEntry *)(addr + offset + (sec * bytes_per_sec));
                for (int i = 0; i < entries_per_sector; i++, dir++) {
                    if (dir->DIR_Name[0] == 0xE5) {
                        continue;
                    }
                    if (dir->DIR_Attr == 0x0F) {
                        continue; 
                    }
                    if (dir->DIR_Name[0] == 0x00) {
                        printf("Total number of entries = %d\n", total);
                        return 0;
                    }
                    int base = 8;
                    while (base > 0 && dir->DIR_Name[base - 1] == ' ') {
                        base--;
                    }
                    int ext = 0;
                    while (ext < 3 && dir->DIR_Name[8 + ext] != ' ') {
                        ext++;
                    }
                    printf("%.*s", base, dir->DIR_Name);
                    if (ext > 0) {
                        printf(".%.*s", ext, dir->DIR_Name + 8);
                    }

                    if (dir->DIR_Attr == 0x10) {
                        printf("/ (starting cluster = %u)\n", dir->DIR_FstClusLO);
                    }
                    else {
                        if (dir->DIR_FileSize > 0) {
                            printf(" (size = %u, starting cluster = %u)\n", dir->DIR_FileSize, dir->DIR_FstClusLO);
                        }
                        else {
                            printf (" (size = 0)\n");
                        }
                    }

                    total++;
                }
            }
            int fat_offset = sec_count * bytes_per_sec + (current_clus * 4);
            if (fat_offset + 4 > sb.st_size) {
                break;
            }
            current_clus = *(int *)(addr + fat_offset);
            root_sec = ((current_clus -2) * sec_per_clus) + first_sec;
            offset = root_sec * bytes_per_sec;
        }

        printf("Total number of entries = %d\n", total);
    
    }
    if (strcmp(argv[2], "-r") == 0) {
        // printf("IM IN HERE HELLLLOOOO FUCK THIS");
        // Open file
        int fd = open(argv[1], O_RDWR);
        if (fd == -1) {
            // printf("EXITED");
            exit(1);
        }

        // Get file size
        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            // printf("EXITED");
            exit(1);
        }

        // Map file into memory
        char *addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED) {
            // printf("EXITED");
            exit(1);
        }
        
        BootEntry *boot_addr = (BootEntry *)(addr);

        int root_clus = boot_addr->BPB_RootClus;
        // now I have to find the root directory 
        // Then I need to find the first data sector 
        // Then based on the root cluster I calculate the first root directory sectory



    
        int  fat_size = boot_addr ->BPB_FATSz32;
        int sec_count = boot_addr ->BPB_RsvdSecCnt;
        int fats = boot_addr -> BPB_NumFATs;

        int first_sec = (fats * fat_size) + sec_count;
        int sec_per_clus = boot_addr->BPB_SecPerClus;
        // int root_sec = ((root_clus - 2) * sec_per_clus) + first_sec;
        int bytes_per_sec = boot_addr->BPB_BytsPerSec;
        // int offset = root_sec * bytes_per_sec;
        int entries_per_sector = bytes_per_sec / sizeof(DirEntry);
        int fat_base = sec_count * bytes_per_sec;

        if (argc <= 5) {
            // int total = 0;

            // T.A. pointed me in this direction for filename parsing 
            char *filename = argv[3];
            char target[11];
            memset(target, ' ', sizeof(target));
            int dot = -1;
            for (int i = 0; filename[i] != '\0'; i++) {
                if (filename[i] == '.') {
                    dot = i;
                    break;  
                }
            }
            int base;
            int ext;
            if (dot == -1) {
                base = strlen(filename);
                ext = 0;
                for (int i = 0; i < base; i++) {
                    target[i] = filename[i];
                }
            } 
            else {
                base = dot;
                ext = strlen(filename) - (dot + 1);
                for (int i = 0; i < base; i++) {
                    target[i] = filename[i];
                }
                for (int i = 0; i < ext; i++) {
                    target[8 + i] = filename[dot + 1 + i];
                }
            }

            // printf("%s", target);

            int found = 0; 
            int match_off = -1;
            int start_clus = 0;
            int current = root_clus; 
            bool gtfoh = false;
            int *fat = (int *)(addr + fat_base);

            while (current < 0x0FFFFFF8 && !gtfoh) {
                int cluster_off = (first_sec + (current - 2)*sec_per_clus) * bytes_per_sec;

                for (int s = 0; s < sec_per_clus; ++s) {
                    DirEntry *dir = (DirEntry*)(addr + cluster_off + s*bytes_per_sec);
                    for (int i = 0; i < entries_per_sector; ++i, ++dir) {
                        if (dir->DIR_Name[0] == 0x00) {
                            // printf("FUKMEHO")
                            gtfoh = true;
                            break;   
                        }
                        if (dir->DIR_Attr == 0x0F) {
                            continue;
                        
                        }

                        if (dir->DIR_Name[0] == 0xE5 &&
                            memcmp(dir->DIR_Name+1, target+1, 10) == 0)
                        {
                            match_off = (char *)dir - addr;
                            start_clus = ((int)dir->DIR_FstClusHI << 16) | dir->DIR_FstClusLO;
                            found += 1;
                        }
                    }
                }
                current = fat[current]; 

            }

            if (found == 0) {
                printf("%s: file not found\n", filename);
                munmap(addr, sb.st_size);
                close(fd);
                return 0;
            }
            if (found > 1) {
                printf("%s: multiple candidates found\n", filename);
                munmap(addr, sb.st_size);
                close(fd);
                return 0;
            }
            DirEntry *dir = (DirEntry*)(addr + match_off);
            int file_size = dir->DIR_FileSize;
            int bytes_per_cluster = bytes_per_sec * sec_per_clus;
            int n_clusters = (file_size + bytes_per_cluster - 1) / bytes_per_cluster;

            dir->DIR_Name[0] = target[0];
            for (int f = 0; f < fats; ++f) {
                int *fat_ptr = (int*)(addr + fat_base + f * fat_size * bytes_per_sec);
                int *fat_entry = (int*)(addr + fat_base + f * fat_size * bytes_per_sec + start_clus * 4);
                if (n_clusters <= 1) {
                    if (( *fat_entry & 0x0FFFFFFF) == 0) {
                        // printf("YESSSS!!!!!LFG\n");
                        *fat_entry = ( (*fat_entry) | 0x0FFFFFFF);
                    }
                }
                else {
                    for (int i = 0; i < n_clusters; ++i) {
                        int current = start_clus + i;
                        int next;
                        if (i < n_clusters - 1 ) {
                            next = current + 1;

                        }
                        else { 
                            next = 0x0FFFFFFF;
                        }
                        *(fat_ptr + current) = (*(fat_ptr + current)) | next;
                    }
                }
            }
            msync(addr, sb.st_size, MS_SYNC);

            printf("%s: successfully recovered\n", filename);
            munmap(addr, sb.st_size);
            close(fd);
            return 0;
        }

        else {
            // used this stack overflow link for understanding SHA: https://stackoverflow.com/questions/9284420/how-to-use-sha1-hashing-in-c-programming 
             // int total = 0;
            char *sha_str  = argv[5];  
            char *filename = argv[3];
            char target[11];
            memset(target, ' ', sizeof(target));
            int dot = -1;

            // T.A. pointed me in this direction for filename parsing 
            for (int i = 0; filename[i] != '\0'; i++) {
                if (filename[i] == '.') {
                    dot = i;
                    break;  
                }
            }
            int base;
            int ext;
            if (dot == -1) {
                base = strlen(filename);
                ext = 0;
                for (int i = 0; i < base; i++) {
                    target[i] = filename[i];
                }
            } 
            else {
                base = dot;
                ext = strlen(filename) - (dot + 1);
                for (int i = 0; i < base; i++) {
                    target[i] = filename[i];
                }
                for (int i = 0; i < ext; i++) {
                    target[8 + i] = filename[dot + 1 + i];
                }
            }

            unsigned char user_hash[SHA_DIGEST_LENGTH];
            for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
                unsigned int byte;
                sscanf(sha_str + 2*i, "%2x", &byte);
                user_hash[i] = (unsigned char)byte;
            }
            // int cluster_off_base =  (first_sec + (root_clus - 2) * sec_per_clus) * bytes_per_sec;

            bool recovered = false;
            unsigned char file_hash[SHA_DIGEST_LENGTH];
            DirEntry *dir; 

            int current = root_clus;
            int *fat = (int *)(addr + fat_base);
    
            while (current < 0x0FFFFFF8) {
                int cluster_off = (first_sec + (current - 2)*sec_per_clus) * bytes_per_sec;


                for (int s = 0; s < sec_per_clus; ++s) {
                    dir = (DirEntry*)(addr + cluster_off + s*bytes_per_sec);
                    for (int i = 0; i < entries_per_sector; ++i, ++dir) {
                        if (dir->DIR_Name[0] == 0x00) {
                            printf("%s: file not found\n", filename);
                            munmap(addr, sb.st_size);
                            close(fd);
                            return 0;
                        }
        
                        if (dir->DIR_Attr == 0x0F) {
                            continue;
                        }

                        if (dir->DIR_Name[0] != 0xE5) {
                            continue;
                        }
                        if (memcmp(dir->DIR_Name+1, target+1, 10) != 0) {
                            continue; 
                        }
                        int start_clus = ((int)dir->DIR_FstClusHI << 16) | dir->DIR_FstClusLO;
                        int file_size = dir->DIR_FileSize;

                        if (file_size == 0) {
                            // printf("EMPTYFILEEEEEE\n");

                            // asked my uncle (SWE) how to do this
                            unsigned char empty_sha[SHA_DIGEST_LENGTH] = {
                            0xda,0x39,0xa3,0xee,0x5e,0x6b,0x4b,0x0d,
                            0x32,0x55,0xbf,0xef,0x95,0x60,0x18,0x90,
                            0xaf,0xd8,0x07,0x09
                            };
                            memcpy(file_hash, empty_sha, SHA_DIGEST_LENGTH);
                        }
                        else {
                            int data_off = (int)(first_sec + (start_clus - 2) * sec_per_clus) * bytes_per_sec;
                            SHA1((unsigned char *)addr + data_off, file_size, file_hash);
                        }

                        if (memcmp(file_hash, user_hash, SHA_DIGEST_LENGTH) == 0) {
                            dir->DIR_Name[0] = target[0];
                            int file_size = dir->DIR_FileSize;
                            int bytes_per_cluster = bytes_per_sec * sec_per_clus;
                            int n_clusters = (file_size + bytes_per_cluster - 1) / bytes_per_cluster;
                            int fat_base = sec_count * bytes_per_sec;
                            for (int f = 0; f < fats; ++f) {
                                int *fat_ptr = (int*)(addr + fat_base + f * fat_size * bytes_per_sec);
                            for (int i = 0; i < n_clusters; ++i) {
                                    int current = start_clus + i;
                                    int next;
                                    if (i < n_clusters - 1 ) {
                                        next = current + 1;

                                    }
                                    else { 
                                        next = 0x0FFFFFFF;
                                    }
                                    *(fat_ptr + current) = (*(fat_ptr + current)) | next;
                                    }
                                }
                            msync(addr, sb.st_size, MS_SYNC);

                            printf("%s: successfully recovered with SHA-1\n", filename);
                            recovered = true; 
                            return 0; 
                        }
                    }
                }
                current = fat[current];
            }


            if (!recovered) {
                printf("%s: file not found\n", filename);
            }

            munmap(addr, sb.st_size);
            close(fd);
            return 0;
        }
    
    }

    
    
}

