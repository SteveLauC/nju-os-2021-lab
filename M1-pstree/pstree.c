#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/types.h>

#define MAX_PROC 20000

// type of cli options
typedef int bool;
typedef struct {
    bool show_pid;
    bool numeric_sort;
    bool version; 
}options;

// void show_options(options * opt) {
//     printf("show pid: %d\nnumeric sort: %d\nversion: %d\n", opt->show_pid, opt->numeric_sort, opt->version);
// }

// parse the cli options
options get_options(int ac, char *av[]) {
    options opt = {
        0,
        0,
        0,
    };

    const char * short_option = ":pnv";
    const struct option long_opton[4] = {
        {"show-pids", 0, NULL, 'p'},
        {"numeric-sort", 0, NULL, 'n'},
        {"version", 0, NULL, 'v'},
        {NULL, 0, NULL, 0},
    };

    int option = -1;
    while (-1 != (option = getopt_long(ac, av, short_option, &long_opton[0], NULL))) {
        switch (option){
            case 'p':
                opt.show_pid = 1;
                break;
            case 'n':
                opt.numeric_sort = 1;
                break;
            case 'v':
                opt.version = 1;
                break;
            case ':':
                fprintf(stderr, "%c needs an additional argument\n", optopt);
                exit(EXIT_FAILURE);
                break;
            case '?':
                fprintf(stderr, "%c: invalid option\n", optopt);
                exit(EXIT_FAILURE);
                break;
        }
    }
    return opt;
}

// definition of the process
typedef struct {
    char cmd[256];
    pid_t pid;
    pid_t ppid;
    int parent_index;
} process;

// definition of the list of processes
typedef struct {
    process p_array[MAX_PROC];
    int p_num;
} processes;

void show_process(processes * p) {
    for (int i = 0; i < p->p_num; i++ ) {
        printf("cmd: %s\npid: %d\nppid: %d\nparent_index: %d\n\n", p->p_array[i].cmd, p->p_array[i].pid, p->p_array[i].ppid, p->p_array[i].parent_index);
        // if (0==p->p_array[i].ppid) {
        //     printf("%s/%d/%d/%d\n", p->p_array[i].cmd, p->p_array[i].pid, p->p_array[i].ppid, p->p_array[i].parent_index);
        // }
    }
}

void parse_stat(char * contents, processes * p) {
    char * c;
    int count = 0;
    int sign = 0;
    for(c=contents; count < 3; c++) {
        if ('(' == *c) {
            sign = 1;
        }
        if (')' == *c) {
            sign = 0;
        }

        if (0==sign && ' ' == *c) {
            *c = '%';
            count += 1;
        }
    }

    // printf("debug: stat = %s\n", contents);

    // parse the contents of stat file
    char * delimiter = "%";
    char * token = strtok(contents, delimiter);
    for (int i = 0; i < 3; i++) {
        token = strtok(NULL, delimiter);
        if (0==i){
            strncpy(p->p_array[p->p_num].cmd, token, strlen(token));
        }
        if (2==i) {
            p->p_array[p->p_num].ppid = atoi(token);
        }
    }
}

/*
  read subdirectories of `/proc` and file `/proc[pid]/stat` to get the processes info
  format of `/proc/[pid]/stat`: pid comm state ppid ... (other irrelevant fields are omitted)
*/
void get_process(processes * p) {
    // initialization
    p->p_num = 0;
    for (int i = 0; i < MAX_PROC; i++) {
        strncpy(p->p_array[i].cmd, "#", 2);
        p->p_array[i].pid = -1;
        p->p_array[i].ppid = -1;
        p->p_array[i].parent_index = -1;
    }

    DIR * dir_ptr = NULL;
    struct dirent * dirent_ptr = NULL;

    // open the `/proc` directory
    if (NULL==(dir_ptr = opendir("/proc"))) {
        fprintf(stderr, "Cannot read from /proc directory\n");
        perror(NULL);
    }
    // read the entries in the directory
    while (NULL != (dirent_ptr=readdir(dir_ptr))) {

        // skip the hidden and system-wide info files 
        if (0==strncmp(dirent_ptr->d_name, ".", 1))
            continue;
        if (isalpha(dirent_ptr->d_name[0]))
            continue;

        // set the pid field
        p->p_array[p->p_num].pid = atoi(dirent_ptr->d_name);

        // get the stat file path ready
        char stat[276];
        sprintf(stat, "/proc/%s/stat", dirent_ptr->d_name);
        FILE * fp = NULL;
        if (NULL==(fp=fopen(stat, "r"))) { // open it
            fprintf(stderr, "Cannot open file %s\n", stat);
            perror(NULL);
            exit(EXIT_FAILURE);
        }
        
        // read the contents of stat into a string
        // don't forget to deallocate the memery pointed by buf
        char * buf = NULL;
        size_t len = 0;
        if (-1 == getline(&buf, &len, fp)) {
            fprintf(stderr, "Cannot read file %s\n", stat);
            perror(NULL);
            exit(EXIT_FAILURE);
        }


        parse_stat(buf, p);

        // some assertions
        // assert(strlen(p->p_array[p->p_num].cmd) >=2);
        // assert(p->p_array[p->p_num].pid >= 0);
        // assert(p->p_array[p->p_num].ppid >= 0);

        fclose(fp); // close the file
        free(buf);  // free the memory
        p->p_num += 1; 
    } 
    closedir(dir_ptr);
}

// set the index of the parent process
void set_parent_process_index(processes * p) {
    for(int i = 0; i < p->p_num; i++) {
        if (0==p->p_array[i].ppid) 
            continue;
        for(int j = 0; j < p->p_num;j++) {
            if (p->p_array[i].ppid == p->p_array[j].pid) {
                p->p_array[i].parent_index = j;
                break;
            }
        }
    }
}

int main(int ac, char *av[]) {
    // options opt = get_options(ac, av);
    
    // show_options(&opt);
    processes * p = (processes*)malloc(sizeof(processes));
    get_process(p);
    set_parent_process_index(p);
    show_process(p);

    free(p);
    return 0;
}