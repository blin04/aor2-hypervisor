#include "vm.h"

#include <getopt.h>
#include <pthread.h>
#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

void* handler(void *arg)
{
	struct vm *v = (struct vm*)arg;
	while (v->stop == 0) {
		v->ret = ioctl(v->vcpu_fd, KVM_RUN, 0);
		if (v->ret == -1) {
			printf("KVM_RUN failed\n");
			vm_destroy(v);
			return NULL;
		}

		switch (v->run->exit_reason) {
		case KVM_EXIT_IO:
			if (v->run->io.direction == KVM_EXIT_IO_OUT && v->run->io.port == 0xE9) {
				char *p = (char *)v->run;
				printf("%c", *(p + v->run->io.data_offset));
			}
			continue;
		case KVM_EXIT_IRQ_WINDOW_OPEN:
			if (v->irqs_count > 0) {
				if (inject_irq(v, IRQ_NUM) < 0) {
					vm_destroy(v);
					return NULL;
				}
				v->irqs_count--;
			} else {
				v->run->request_interrupt_window = 0;
			}
			continue;
		case KVM_EXIT_HLT:
			printf("KVM_EXIT_HLT\n");
			v->stop = 1;
			break;
		case KVM_EXIT_SHUTDOWN:
			printf("Shutdown\n");
			v->stop = 1;
			break;
		default:
			printf("Default - exit reason: %d\n", v->run->exit_reason);
			break;
		}
	}
	return NULL;
}

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
			printf("parsed m with arg %s\n", optarg);
			guest_memory_size = atoi(optarg) * 1024u * 1024u;		// MB
			break;
		case 'p':
			printf("parsed p with arg %s\n", optarg);
			if (*optarg == '2')
				guest_page_size = 2 * 1024u * 1024u;	// 2MB
			else
				guest_page_size = 4 * 1024u;			// 4kB
			break;
		case 'g':
			printf("parsed g with args: ");
		
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
		setup_vm(&guests[i], guest_paths[i]);
		int ret = pthread_create(
			&guest_handlers[i], NULL, handler, (void*)&guests[i]
		);
		if (ret < 0) {
			printf("error: failed creating thread for %s\n", guest_paths[i]);
		}
	}

	// wait unitl guests finish
	for (int i = 0; i < n_guests; i++)
		pthread_join(guest_handlers[i], NULL);
 
	for (int i = 0; i < n_guests; i++)
		vm_destroy(&guests[i]);

	return 0;
}
