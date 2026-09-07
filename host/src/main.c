#include "vm.h"

#include <getopt.h>
#include <pthread.h>
#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/ioctl.h>

static pthread_mutex_t io_lock = PTHREAD_MUTEX_INITIALIZER;

static void flush_console_locked(struct vm *v)
{
	if (v->out_len) {
		fwrite(v->out_buf, 1, v->out_len, stdout);
		v->out_len = 0;
	}
}

static void flush_console(struct vm *v)
{
	pthread_mutex_lock(&io_lock);
	flush_console_locked(v);
	fflush(stdout);
	pthread_mutex_unlock(&io_lock);
}

static void vm_log(struct vm *v, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	pthread_mutex_lock(&io_lock);
	flush_console_locked(v);
	vprintf(fmt, ap);
	fflush(stdout);
	pthread_mutex_unlock(&io_lock);
	va_end(ap);
}

void* handler(void *arg)
{
	struct vm *v = (struct vm*)arg;
	while (v->stop == 0) {
		v->ret = ioctl(v->vcpu_fd, KVM_RUN, 0);
		if (v->ret == -1) {
			vm_log(v, "KVM_RUN failed for guest %s\n", v->image_path);
			vm_destroy(v);
			return NULL;
		}

		switch (v->run->exit_reason) {
		case KVM_EXIT_IO:
			if (v->run->io.direction == KVM_EXIT_IO_OUT && v->run->io.port == 0xE9) {
				char *p = (char *)v->run;
				char c = *(p + v->run->io.data_offset);

				v->out_buf[v->out_len++] = c;
				if (c == '\n' || v->out_len == sizeof(v->out_buf))
					flush_console(v);
			}
			continue;
		case KVM_EXIT_IRQ_WINDOW_OPEN:
			if (v->irqs_count > 0) {
				if (inject_irq(v, IRQ_NUM) < 0) {
					flush_console(v);
					vm_destroy(v);
					return NULL;
				}
				v->irqs_count--;
			} else {
				v->run->request_interrupt_window = 0;
			}
			continue;
		case KVM_EXIT_HLT:
			vm_log(v, "Guest %s finished!\n", v->image_path);
			v->stop = 1;
			break;
		case KVM_EXIT_SHUTDOWN:
			vm_log(v, "Shutdown\n");
			v->stop = 1;
			break;
		case KVM_EXIT_FAIL_ENTRY:
			vm_log(v, "VM stopped: guest %s failed to enter guest mode, "
			       "hardware entry failure reason 0x%llx\n",
			       v->image_path,
			       (unsigned long long)v->run->fail_entry.hardware_entry_failure_reason);
			v->stop = 1;
			break;
		case KVM_EXIT_INTERNAL_ERROR:
			vm_log(v, "VM stopped: guest %s internal KVM error, suberror %u\n",
			       v->image_path, v->run->internal.suberror);
			v->stop = 1;
			break;
		default:
			vm_log(v, "Default - guest %s exit reason: %d\n",
			       v->image_path, v->run->exit_reason);
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
			pthread_mutex_lock(&io_lock);
			printf("error: failed creating thread for %s\n", guest_paths[i]);
			pthread_mutex_unlock(&io_lock);
		}
	}

	// wait until guests finish
	for (int i = 0; i < n_guests; i++)
		pthread_join(guest_handlers[i], NULL);
 
	for (int i = 0; i < n_guests; i++)
		vm_destroy(&guests[i]);

	return 0;
}
