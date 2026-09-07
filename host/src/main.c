#include "vm.h"
#include "handler.h"

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
		{0, 0, 0, 0}
	};

	int ret;
	int n_guests = 0;
	char** guest_paths = (char** )malloc(sizeof(char *));

	if (guest_paths == NULL) {
		printf("error: malloc failed\n");
		return 1;
	}

	unsigned int guest_memory_size;
	unsigned int guest_page_size;

	// parse arguments
	while ((ret = getopt_long(argc, argv, "m:p:g:", options, NULL)) != -1) {
		switch (ret) {
		case 'm':
			// printf("parsed m with arg %s\n", optarg);
			guest_memory_size = atoi(optarg) * 1024u * 1024u;		// MB
			break;
		case 'p':
			// printf("parsed p with arg %s\n", optarg);
			if (*optarg == '2')
				guest_page_size = 2 * 1024u * 1024u;	// 2MB
			else
				guest_page_size = 4 * 1024u;			// 4kB
			break;
		case 'g':
			// printf("parsed g with args: ");
		
			// first guest image is parsed by getopt
			guest_paths[0] = optarg;
			n_guests++;

			// parse the rest of guest images
			while (optind < argc && argv[optind][0] != '-') {
				guest_paths = realloc(guest_paths, (n_guests + 1) * sizeof(char *));
				guest_paths[n_guests++] = argv[optind];
				optind++;
			}

			for (int i = 0; i < n_guests; i++) {
				if (i != 0) printf(", ");
				printf("%s", guest_paths[i]);
			}				
			printf("\n");

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

	for (int i = 0; i < n_guests; i++) {
		setup_vm(&guests[i], guest_paths[i], guest_memory_size, guest_page_size);
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
