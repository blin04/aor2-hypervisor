#include "buffer.h"
#include "vm.h"
#include "handler.h"
#include "fileops.h"

#include <getopt.h>
#include <pthread.h>
#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[])
{
	// cli options
	struct option options[] = {
		{"memory", required_argument, NULL, 'm'},
		{"page", required_argument, NULL, 'p'},
		{"guest", required_argument, NULL, 'g'},
		{"fileq", required_argument, NULL, 'f'},
		{0, 0, 0, 0}
	};

	int ret;
	int n_guests = 0;
	char** guest_paths = (char** )malloc(sizeof(char *));
	int n_shared_files = 0;
	char** shared_files = NULL;

	if (guest_paths == NULL) {
		printf("error: malloc failed\n");
		return 1;
	}

	unsigned int guest_memory_size;
	unsigned int guest_page_size;

	// parse arguments
	while ((ret = getopt_long(argc, argv, "m:p:g:f::", options, NULL)) != -1) {
		switch (ret) {
		case 'm':
			if (*optarg == '2' || *optarg == '4' || *optarg == '8')
				guest_memory_size = atoi(optarg) * 1024u * 1024u;		// MB
			else {
				printf("error: invalid memory size\n");
				return 1;
			}
			break;
		case 'p':
			if (*optarg == '2')
				guest_page_size = 2 * 1024u * 1024u;	// 2MB
			else if (*optarg == '4')
				guest_page_size = 4 * 1024u;			// 4kB
			else {
				printf("error: invalid page size\n");
				return 1;
			}
			break;
		case 'g':
			// first guest image is parsed by getopt
			guest_paths[n_guests++] = optarg;

			// parse the rest of guest images
			while (optind < argc && argv[optind][0] != '-') {
				guest_paths = realloc(guest_paths, (n_guests + 1) * sizeof(char *));
				if (guest_paths == NULL) {
					printf("error: realloc failed\n");
					return 1;
				}
				guest_paths[n_guests++] = argv[optind];
				optind++;
			}

			break;
		case 'f':
			shared_files = (char**) malloc(sizeof(char*));
			if (shared_files == NULL) {
				printf("error: malloc failed\n");
				return 1;
			}

			shared_files[n_shared_files++] = optarg;
			while (optind < argc && argv[optind][0] != '-') {
				shared_files = realloc(shared_files, (n_shared_files + 1) * sizeof(char *));
				if (shared_files == NULL) {
					printf("error: realloc failed\n");
					return 1;
				}
				shared_files[n_shared_files++] = argv[optind];
				optind++;
			}

			break;
		case '?':
			if (optopt == 'm') {
				printf("error: missing guest memory size (-m, --memory)\n");
				return 0;
			}
			else if (optopt == 'p') {
				printf("error: missing page size (-p, --page)\n");
				return 0;
			}
			else if (optopt == 'g') {
				printf("error: missing page size (-g, --guest)\n");
				return 0;
			}
		}
	}

	struct vm guests[n_guests];
	pthread_t guest_handlers[n_guests];

	init_shared_files(shared_files, n_shared_files);
	buffer_init(n_guests - 1);	

	for (int i = 0; i < n_guests; i++) {
		setup_vm(&guests[i], i, guest_paths[i], guest_memory_size, guest_page_size);
		int ret = pthread_create(
			&guest_handlers[i], NULL, handler, (void*)&guests[i]
		);
		if (ret < 0) {
			printf("error: failed creating thread for %s\n", guest_paths[i]);
		}
	}

	// wait until guests finish
	for (int i = 0; i < n_guests; i++)
		pthread_join(guest_handlers[i], NULL);
 
	for (int i = 0; i < n_guests; i++)
		vm_destroy(&guests[i]);

	return 0;
}
