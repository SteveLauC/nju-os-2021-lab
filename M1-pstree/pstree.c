#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/types.h>

// the max number of process we are going to deal with
#define MAX_PROC 500

// type of cli options
typedef int bool;
#define false 0
#define true 1
typedef struct {
    bool show_pid;
    bool numeric_sort;
    bool version; 
}options;

// parse the cli options
options get_options(int ac, char *av[]) {
    options opt = {
        false,
        false,
        false,
    };

    const char * short_option = ":pnV";
    const struct option long_opton[4] = {
        {"show-pids", 0, NULL, 'p'},
        {"numeric-sort", 0, NULL, 'n'},
        {"version", 0, NULL, 'V'},
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
            case 'V':
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

// parse the contents of `/proc/[pid]/stat` file and set `cmd` and `ppid` fields
void parse_stat(char * contents, processes * p) {
    char * c;
    int count = 0;  // record how many whitespaces have been converted
    int sign = 0;   // sign to indicate whether we are in the `()`

    // convert `pid (comm) state ppid...` into a format like this: `pid%%comm%%state%ppid...`, 
    // for the reason that field `comm` may has whitespace, which obstructs our splitting process.
    // 3 whitespaces and 2 parentheses need to be converted, procedure is done when 3 whitespaces are all converted.
    for(c=contents; count < 3; c++) {
        if ('(' == *c) {
            sign = 1;
            *c='%';
        }
        if (')' == *c) {
            sign = 0;
            *c='%';
        }

        if (0==sign && ' ' == *c) {
            *c = '%';
            count += 1;
        }
    }

    // parse the contents of stat file using delimiter `%`
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


        // set ppid and cmd fields
        parse_stat(buf, p);

        // some assertions
        assert(strlen(p->p_array[p->p_num].cmd) >=2);
        assert(p->p_array[p->p_num].pid >= 0);
        assert(p->p_array[p->p_num].ppid >= 0);

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

// sort the child processes of the process in `index`
void numeric_sort(processes * const p, const int index) {
    int indices_of_child_proc[MAX_PROC];   // store indices of child processes
    int amount_of_child_proc = 0;          // record how many child processes we have
    
    // put the indices of child processes in the array
    for (int i = 0; i < p->p_num; i++) {
        if (p->p_array[i].parent_index==index) {
            indices_of_child_proc[amount_of_child_proc++] = i;
        }
    }
    

    // sort the child processes 
    for (int i = 0; i < amount_of_child_proc-1; i++) {
        int ptr = 0;
        for (int j = indices_of_child_proc[ptr]; ptr < amount_of_child_proc-i-1; ptr++) {
            j = indices_of_child_proc[ptr];
            if (p->p_array[j].pid > p->p_array[indices_of_child_proc[ptr+1]].pid) {
                // swap the pid field
                int tmp_pid = p->p_array[j].pid;
                p->p_array[j].pid = p->p_array[indices_of_child_proc[ptr+1]].pid;
                p->p_array[indices_of_child_proc[ptr+1]].pid = tmp_pid;

                // swap the cmd field
                char tmp_cmd[256];
                *tmp_cmd = '\0';
                strncpy(tmp_cmd, p->p_array[j].cmd, strlen(p->p_array[j].cmd));
                strncpy(p->p_array[j].cmd, p->p_array[indices_of_child_proc[ptr+1]].cmd, strlen(p->p_array[indices_of_child_proc[ptr+1]].cmd));
                strncpy(p->p_array[indices_of_child_proc[ptr+1]].cmd, tmp_cmd, strlen(tmp_cmd));
            }
        }
    }
}

// recursively traverse the processes tree in preorder
void preorder_traverse(processes * const p, const int index, int level, const options * const opt_ptr) {
    if (0!=p->p_num) {
        for(int i = 0; i < level;i++) {
            printf("\t");
        }
        printf("%s", p->p_array[index].cmd);

        // if `-p/--show-pids` option is present, print the pid number
        if (opt_ptr->show_pid) {
            printf("(%d)", p->p_array[index].pid);
        }
        // if `-n/--numeric-sort` option is present, print the child processes in ascending order
        if (opt_ptr->numeric_sort) {
            numeric_sort(p, index);
        }
        printf("\n");

        for (int j = 0; j < p->p_num; j++ ) {
            if (p->p_array[j].parent_index==index) {
                preorder_traverse(p, j, level+1, opt_ptr);
            }
        }
    }
}
int main(int ac, char *av[]) {
    options opt = get_options(ac, av);
    if (opt.version) {
        printf("pstree 0.1\n");
        exit(EXIT_SUCCESS);
    }

    processes * p = (processes*)malloc(sizeof(processes));
    get_process(p);
    set_parent_process_index(p);
    preorder_traverse(p, 0, 0, &opt);
    free(p);
    return 0;
}